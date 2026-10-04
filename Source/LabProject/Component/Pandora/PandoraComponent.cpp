#include "Component/Pandora/PandoraComponent.h"

#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Definition/Item/ItemDefinition.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Component/Player/EquipmentComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraComponent)

DEFINE_LOG_CATEGORY(PandoraComponentLog)

namespace
{
	constexpr double ServerValidationLogIntervalSeconds = 5.0;
}

void FReplicatedPandoraEntry::PostReplicatedAdd(const FReplicatedPandoraList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->bReplicatedInventoryChanged = true;
	}
}

void FReplicatedPandoraEntry::PostReplicatedChange(const FReplicatedPandoraList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->bReplicatedInventoryChanged = true;
	}
}

void FReplicatedPandoraEntry::PreReplicatedRemove(const FReplicatedPandoraList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->bReplicatedInventoryChanged = true;
	}
}

void FReplicatedPandoraList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (Owner && Owner->bReplicatedInventoryChanged)
	{
		Owner->bReplicatedInventoryChanged = false;
		Owner->OnPandoraInventoryChanged.Broadcast();
	}
}

UPandoraComponent::UPandoraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	ReplicatedEntries.Owner = this;
}

void UPandoraComponent::BeginPlay()
{
	Super::BeginPlay();
	ReplicatedEntries.Owner = this;
}

void UPandoraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingPandoraLoads();
	if (HasPandoraAuthority())
	{
		ClearGrantedPandoraContent();
	}
	ReleaseSkillSourcesForEndPlay();

	ReplicatedEntries.Owner = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UPandoraComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraComponent, ReplicatedEntries, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraComponent, CurrentPandoraDefinition, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraComponent, CurrentPandoraLoadoutDirection, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraComponent, PandoraLoadoutSlots, Params);
}

bool UPandoraComponent::HasPandoraAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UPandoraComponent::LogRejectedServerRequest(
	const TCHAR* RequestName,
	const FString& Reason)
{
	uint32 SuppressedCount = 0;
	if (!ServerValidationLogLimiter.TryAcquire(
		ServerValidationLogIntervalSeconds,
		SuppressedCount))
	{
		return;
	}

	UE_LOG(
		PandoraComponentLog,
		Warning,
		TEXT("Rejected Pandora server request. Owner=%s Request=%s Reason=%s "
			"SuppressedSinceLast=%u"),
		*GetPathNameSafe(GetOwner()),
		RequestName,
		*Reason,
		SuppressedCount);
}

void UPandoraComponent::GrantPandorasByPrimaryAssetIds(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
	GrantPandorasWithLoadout(PandoraDefinitions, {});
}

void UPandoraComponent::GrantPandorasWithLoadout(
	const TArray<FPrimaryAssetId>& PandoraDefinitions,
	const TMap<EEnum_Direction, FPrimaryAssetId>& PandoraLoadoutByDirection)
{
	if (!HasPandoraAuthority() || PandoraDefinitions.IsEmpty())
	{
		return;
	}
	PendingPandoraLoadHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& Handle)
	{
		return !Handle.IsValid() || Handle->HasLoadCompleted();
	});

	TArray<FPrimaryAssetId> DefinitionIdsToLoad = PandoraDefinitions;
	for (const TPair<EEnum_Direction, FPrimaryAssetId>& Pair : PandoraLoadoutByDirection)
	{
		if (PandoraLoadout::IsLoadoutDirection(Pair.Key) && Pair.Value.IsValid())
		{
			DefinitionIdsToLoad.AddUnique(Pair.Value);
		}
	}

	const uint64 RequestGeneration = PandoraLoadGeneration;
	TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::Get().LoadPrimaryAssets(
		DefinitionIdsToLoad, {}, FStreamableDelegate::CreateWeakLambda(this,
		[this, PandoraDefinitions, PandoraLoadoutByDirection, RequestGeneration]()
		{
			if (RequestGeneration != PandoraLoadGeneration || !HasPandoraAuthority())
			{
				return;
			}

			UAssetManager& AssetManager = UAssetManager::Get();
			bool bChanged = false;
			for (const FPrimaryAssetId& DefinitionId : PandoraDefinitions)
			{
				const UPandoraDefinition* Definition = Cast<UPandoraDefinition>(AssetManager.GetPrimaryAssetObject(DefinitionId));
				if (!IsValid(Definition))
				{
					UE_LOG(PandoraComponentLog, Error, TEXT("Failed to load Pandora definition '%s'."), *DefinitionId.ToString());
					continue;
				}
				bChanged |= AddOwnedPandoraEntry(Definition);
			}

			for (const TPair<EEnum_Direction, FPrimaryAssetId>& Pair : PandoraLoadoutByDirection)
			{
				// 슬롯 적용 이벤트에서 초기화가 요청되면 나머지 이전 작업도 중단한다.
				if (RequestGeneration != PandoraLoadGeneration)
				{
					return;
				}
				const UPandoraDefinition* Definition = Cast<UPandoraDefinition>(AssetManager.GetPrimaryAssetObject(Pair.Value));
				if (IsValid(Definition))
				{
					SetPandoraLoadoutSlotInternal(Pair.Key, Definition);
				}
			}
			if (bChanged && RequestGeneration == PandoraLoadGeneration)
			{
				OnPandoraInventoryChanged.Broadcast();
			}
		}));
	if (LoadHandle.IsValid())
	{
		PendingPandoraLoadHandles.Add(LoadHandle);
	}
}

// 이미 로드된 판도라를 지급할 때도 보유 상태와 복제 항목을 같은 경로로 변경한다.
bool UPandoraComponent::GrantPandoraDefinition(const UPandoraDefinition* PandoraDefinition)
{
	if (!HasPandoraAuthority() || !IsValid(PandoraDefinition))
	{
		return false;
	}
	if (AddOwnedPandoraEntry(PandoraDefinition))
	{
		OnPandoraInventoryChanged.Broadcast();
	}
	return true;
}

bool UPandoraComponent::HasPandoraDefinition(const UPandoraDefinition* PandoraDefinition) const
{
	if (!IsValid(PandoraDefinition))
	{
		return false;
	}
	const FReplicatedPandoraEntry* Entry = FindReplicatedEntryByDefinition(PandoraDefinition);
	return Entry != nullptr;
}

void UPandoraComponent::CancelPendingPandoraLoads()
{
	// 취소 전에 이미 완료 큐에 들어간 콜백도 초기화 이후에는 상태를 변경하지 못하게 한다.
	++PandoraLoadGeneration;
	for (const TSharedPtr<FStreamableHandle>& Handle : PendingPandoraLoadHandles)
	{
		if (Handle.IsValid() && !Handle->HasLoadCompleted())
		{
			Handle->CancelHandle();
		}
	}
	PendingPandoraLoadHandles.Reset();
}

void UPandoraComponent::ClearAllPandoras()
{
	if (!HasPandoraAuthority())
	{
		return;
	}

	CancelPendingPandoraLoads();

	const bool bHadPandoras = !ReplicatedEntries.Entries.IsEmpty();
	const bool bHadSelection = CurrentPandoraDefinition != nullptr
		|| CurrentPandoraLoadoutDirection != EEnum_Direction::Center
		|| !GrantedPandoraAbilityHandles.IsEmpty();
	const bool bHadLoadout = !PandoraLoadoutSlots.IsEmpty();

	ClearGrantedPandoraContent();
	CurrentPandoraDefinition = nullptr;
	CurrentPandoraLoadoutDirection = EEnum_Direction::Center;
	PandoraLoadoutSlots.Reset();
	ReplicatedEntries.Entries.Reset();
	ReplicatedEntries.MarkArrayDirty();

	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, CurrentPandoraDefinition, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, CurrentPandoraLoadoutDirection, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, PandoraLoadoutSlots, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, ReplicatedEntries, this);

	if (bHadSelection)
	{
		NotifyPandoraSelectionChanged();
	}

	if (bHadLoadout)
	{
		NotifyPandoraLoadoutChanged();
	}

	if (bHadPandoras)
	{
		OnPandoraInventoryChanged.Broadcast();
	}
}

bool UPandoraComponent::RequestPandoraSelection(const UPandoraDefinition* PandoraDefinition)
{
	return RequestPandoraSelectionForDirection(EEnum_Direction::Center, PandoraDefinition);
}

bool UPandoraComponent::RequestPandoraSelectionForDirection(
	const EEnum_Direction Direction,
	const UPandoraDefinition* PandoraDefinition)
{
	const FPrimaryAssetId PandoraDefinitionId = PandoraDefinition ? PandoraDefinition->GetPrimaryAssetId() : FPrimaryAssetId();

	if (!GetOwner())
	{
		return false;
	}

	if (!HasPandoraAuthority())
	{
		ServerRequestPandoraSelection(PandoraDefinitionId, Direction);
		return true;
	}

	return SelectPandoraByPrimaryAssetId(PandoraDefinitionId, Direction);
}

void UPandoraComponent::ServerRequestPandoraSelection_Implementation(
	FPrimaryAssetId PandoraDefinitionId,
	EEnum_Direction RequestedDirection)
{
	if (RequestedDirection != EEnum_Direction::Center
		&& !PandoraLoadout::IsLoadoutDirection(RequestedDirection))
	{
		LogRejectedServerRequest(
			TEXT("Select"),
			FString::Printf(
				TEXT("invalid direction=%d"),
				static_cast<int32>(RequestedDirection)));
		return;
	}

	if (!SelectPandoraByPrimaryAssetId(PandoraDefinitionId, RequestedDirection))
	{
		LogRejectedServerRequest(
			TEXT("Select"),
			FString::Printf(
				TEXT("Pandora is not owned or cannot be resolved. AssetId=%s"),
				*PandoraDefinitionId.ToString()));
	}
}

bool UPandoraComponent::RequestSetPandoraLoadoutSlot(
	const EEnum_Direction Direction,
	const UPandoraDefinition* PandoraDefinition)
{
	if (!PandoraLoadout::IsLoadoutDirection(Direction))
	{
		return false;
	}

	if (!GetOwner())
	{
		return false;
	}

	const FPrimaryAssetId PandoraDefinitionId = PandoraDefinition ? PandoraDefinition->GetPrimaryAssetId() : FPrimaryAssetId();

	if (!HasPandoraAuthority())
	{
		ServerSetPandoraLoadoutSlot(Direction, PandoraDefinitionId);
		return true;
	}

	return SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition);
}

bool UPandoraComponent::RequestAutoSetPandoraLoadoutSlot(const UPandoraDefinition* PandoraDefinition)
{
	if (!GetOwner())
	{
		return false;
	}

	if (!PandoraDefinition)
	{
		return false;
	}

	const FPrimaryAssetId PandoraDefinitionId = PandoraDefinition->GetPrimaryAssetId();
	if (!HasPandoraAuthority())
	{
		ServerAutoSetPandoraLoadoutSlot(PandoraDefinitionId);
		return true;
	}

	EEnum_Direction Direction = EEnum_Direction::Center;
	if (!ResolvePreferredAutoPandoraLoadoutDirection(PandoraDefinition, Direction))
	{
		return false;
	}

	return SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition);
}

void UPandoraComponent::ServerSetPandoraLoadoutSlot_Implementation(
	EEnum_Direction Direction,
	FPrimaryAssetId PandoraDefinitionId)
{
	if (!PandoraLoadout::IsLoadoutDirection(Direction))
	{
		LogRejectedServerRequest(
			TEXT("SetLoadoutSlot"),
			FString::Printf(
				TEXT("invalid direction=%d"),
				static_cast<int32>(Direction)));
		return;
	}

	const UPandoraDefinition* PandoraDefinition = nullptr;
	if (PandoraDefinitionId.IsValid())
	{
		PandoraDefinition = FindOwnedPandoraDefinitionByPrimaryAssetId(PandoraDefinitionId);
		if (!PandoraDefinition)
		{
			LogRejectedServerRequest(
				TEXT("SetLoadoutSlot"),
				FString::Printf(
					TEXT("Pandora is not owned or cannot be resolved. AssetId=%s"),
					*PandoraDefinitionId.ToString()));
			return;
		}
	}

	if (!SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition))
	{
		LogRejectedServerRequest(
			TEXT("SetLoadoutSlot"),
			FString::Printf(
				TEXT("loadout policy rejected AssetId=%s Direction=%d"),
				*PandoraDefinitionId.ToString(),
				static_cast<int32>(Direction)));
	}
}

void UPandoraComponent::ServerAutoSetPandoraLoadoutSlot_Implementation(FPrimaryAssetId PandoraDefinitionId)
{
	if (!PandoraDefinitionId.IsValid())
	{
		LogRejectedServerRequest(
			TEXT("AutoSetLoadoutSlot"),
			TEXT("invalid Pandora PrimaryAssetId"));
		return;
	}

	const UPandoraDefinition* PandoraDefinition = FindOwnedPandoraDefinitionByPrimaryAssetId(PandoraDefinitionId);
	if (!PandoraDefinition)
	{
		LogRejectedServerRequest(
			TEXT("AutoSetLoadoutSlot"),
			FString::Printf(
				TEXT("Pandora is not owned or cannot be resolved. AssetId=%s"),
				*PandoraDefinitionId.ToString()));
		return;
	}

	EEnum_Direction Direction = EEnum_Direction::Center;
	if (!ResolvePreferredAutoPandoraLoadoutDirection(PandoraDefinition, Direction))
	{
		LogRejectedServerRequest(
			TEXT("AutoSetLoadoutSlot"),
			FString::Printf(
				TEXT("no compatible loadout slot. AssetId=%s"),
				*PandoraDefinitionId.ToString()));
		return;
	}

	if (!SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition))
	{
		LogRejectedServerRequest(
			TEXT("AutoSetLoadoutSlot"),
			FString::Printf(
				TEXT("loadout policy rejected AssetId=%s Direction=%d"),
				*PandoraDefinitionId.ToString(),
				static_cast<int32>(Direction)));
	}
}

const UPandoraDefinition* UPandoraComponent::GetPandoraLoadoutDefinition(const EEnum_Direction Direction) const
{
	const FPandoraLoadoutSlot* Slot = PandoraLoadout::FindSlot(PandoraLoadoutSlots, Direction);
	return Slot ? Slot->PandoraDefinition.Get() : nullptr;
}

bool UPandoraComponent::AddOwnedPandoraEntry(const UPandoraDefinition* PandoraDefinition)
{
	if (FindReplicatedEntryByDefinition(PandoraDefinition))
	{
		return false;
	}
	FReplicatedPandoraEntry& Entry = ReplicatedEntries.Entries.AddDefaulted_GetRef();
	Entry.PandoraDefinition = PandoraDefinition;
	ReplicatedEntries.MarkEntryDirty(Entry);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, ReplicatedEntries, this);
	return true;
}

bool UPandoraComponent::SelectPandoraByPrimaryAssetId(
	FPrimaryAssetId PandoraDefinitionId,
	const EEnum_Direction RequestedDirection)
{
	if (!HasPandoraAuthority())
	{
		return false;
	}

	UPdAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!PandoraDefinitionId.IsValid())
	{
		const EEnum_Direction SelectedDirection = ResolvePandoraSelectionDirection(nullptr, RequestedDirection);
		const bool bDefinitionChanged = CurrentPandoraDefinition != nullptr;
		const bool bDirectionChanged = CurrentPandoraLoadoutDirection != SelectedDirection;

		CurrentPandoraDefinition = nullptr;
		CurrentPandoraLoadoutDirection = SelectedDirection;

		if (bDefinitionChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, CurrentPandoraDefinition, this);
		}
		if (bDirectionChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, CurrentPandoraLoadoutDirection, this);
		}

		RefreshPandoraSkillInputBindings(ASC);
		NotifyPandoraSelectionChanged();
		return true;
	}

	const UPandoraDefinition* PandoraDefinition = FindOwnedPandoraDefinitionByPrimaryAssetId(PandoraDefinitionId);
	if (!PandoraDefinition)
	{
		return false;
	}

	const UPandoraDefinition* PreviousPandoraDefinition = CurrentPandoraDefinition;
	const EEnum_Direction PreviousDirection = CurrentPandoraLoadoutDirection;

	CurrentPandoraDefinition = PandoraDefinition;
	CurrentPandoraLoadoutDirection = ResolvePandoraSelectionDirection(PandoraDefinition, RequestedDirection);
	if (PreviousPandoraDefinition != CurrentPandoraDefinition)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, CurrentPandoraDefinition, this);
	}
	if (PreviousDirection != CurrentPandoraLoadoutDirection)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, CurrentPandoraLoadoutDirection, this);
	}

	RefreshCurrentPandoraSkills();

	return true;
}

void UPandoraComponent::RefreshCurrentPandoraSkills()
{
	if (!HasPandoraAuthority())
	{
		return;
	}

	UPdAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
	const int32 RuntimeLevel = ResolveSelectedPandoraRuntimeLevel(CurrentPandoraDefinition);

	const bool bCompatibleWithCurrentWeapon = IsPandoraCompatibleWithCurrentWeapon(CurrentPandoraDefinition);
	const EEnum_Direction EffectiveLoadoutDirection = ResolvePandoraSelectionDirection(
		CurrentPandoraDefinition,
		CurrentPandoraLoadoutDirection);

	if (CurrentPandoraLoadoutDirection != EffectiveLoadoutDirection)
	{
		CurrentPandoraLoadoutDirection = EffectiveLoadoutDirection;
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, CurrentPandoraLoadoutDirection, this);
	}
	if (ASC && CurrentPandoraDefinition && bCompatibleWithCurrentWeapon)
	{
		GrantPandoraSkills(ASC, CurrentPandoraDefinition, RuntimeLevel, EffectiveLoadoutDirection);
	}

	RefreshPandoraSkillInputBindings(ASC);
	NotifyPandoraSelectionChanged();
}

bool UPandoraComponent::IsPandoraCompatibleWithCurrentWeapon(const UPandoraDefinition* PandoraDefinition) const
{
	return !PandoraDefinition || PandoraDefinition->IsCompatibleWithWeaponDefinition(GetCurrentWeaponDefinition());
}

int32 UPandoraComponent::ResolveSelectedPandoraRuntimeLevel(const UPandoraDefinition* PandoraDefinition) const
{
	int32 PandoraLevel = 0;
	if (const APlayerState* PlayerStateOwner = GetPlayerState<APlayerState>())
	{
		if (UPandoraTreeComponent* PandoraTreeComponent = PlayerStateOwner->FindComponentByClass<UPandoraTreeComponent>())
		{
			PandoraLevel = PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition);
		}
	}

	if (PandoraLevel <= 0)
	{
		return 0;
	}

	const int32 MaxUnlockLevel = PandoraDefinition ? PandoraDefinition->GetMaxLevel() : 1;
	return FMath::Clamp(PandoraLevel, 1, FMath::Max(MaxUnlockLevel, 1));
}

EEnum_Direction UPandoraComponent::ResolvePandoraSelectionDirection(
	const UPandoraDefinition* PandoraDefinition,
	const EEnum_Direction RequestedDirection) const
{
	if (!PandoraDefinition)
	{
		// 판도라가 없어도 선택한 슬롯 방향은 유지한다.
		return PandoraLoadout::IsLoadoutDirection(RequestedDirection) ? RequestedDirection : EEnum_Direction::Center;
	}

	if (PandoraLoadout::IsLoadoutDirection(RequestedDirection)
		&& GetPandoraLoadoutDefinition(RequestedDirection) == PandoraDefinition)
	{
		return RequestedDirection;
	}

	if (PandoraLoadout::IsLoadoutDirection(CurrentPandoraLoadoutDirection)
		&& GetPandoraLoadoutDefinition(CurrentPandoraLoadoutDirection) == PandoraDefinition)
	{
		return CurrentPandoraLoadoutDirection;
	}

	for (const EEnum_Direction Direction : { EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (GetPandoraLoadoutDefinition(Direction) == PandoraDefinition)
		{
			return Direction;
		}
	}

	return EEnum_Direction::Center;
}

// 판도라 스킬은 소유한 PlayerState의 ASC에 부여한다.
UPdAbilitySystemComponent* UPandoraComponent::GetOwnerAbilitySystemComponent() const
{
	return Cast<UPdAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
}

const UEquipmentComponent* UPandoraComponent::GetCurrentEquipmentComponent() const
{
	const APlayerState* PlayerState = GetPlayerState<APlayerState>();
	const ACharacterBase* CharacterOwner = PlayerState ? Cast<ACharacterBase>(PlayerState->GetPawn()) : nullptr;
	return CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
}

const UItemDefinition* UPandoraComponent::GetCurrentWeaponDefinition() const
{
	const UEquipmentComponent* EquipmentComponent = GetCurrentEquipmentComponent();
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponDefinition() : nullptr;
}

EEnum_Direction UPandoraComponent::GetCurrentWeaponLoadoutDirection() const
{
	const UEquipmentComponent* EquipmentComponent = GetCurrentEquipmentComponent();
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponLoadoutDirection() : EEnum_Direction::Center;
}

void UPandoraComponent::OnRep_CurrentPandoraDefinition()
{
	NotifyPandoraSelectionChanged();
}

void UPandoraComponent::OnRep_CurrentPandoraLoadoutDirection()
{
	NotifyPandoraSelectionChanged();
}

void UPandoraComponent::OnRep_PandoraLoadoutSlots()
{
	NotifyPandoraLoadoutChanged();
}

void UPandoraComponent::NotifyPandoraSelectionChanged()
{
	// 서버는 스킬 부여와 입력 연결을 마친 뒤, 클라이언트는 선택 복제를 받은 뒤 갱신한다.
	if (UPdAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent())
	{
		ASC->OnAbilitiesChangedNative.Broadcast();
	}

	OnPandoraSelectionChanged.Broadcast(CurrentPandoraDefinition);
}

void UPandoraComponent::NotifyPandoraLoadoutChanged()
{
	OnPandoraLoadoutChanged.Broadcast();
}

bool UPandoraComponent::ResolveAutoPandoraLoadoutDirection(
	const UPandoraDefinition* PandoraDefinition,
	EEnum_Direction& OutDirection) const
{
	OutDirection = EEnum_Direction::Center;
	if (!PandoraDefinition)
	{
		return false;
	}

	for (const EEnum_Direction Direction : { EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (GetPandoraLoadoutDefinition(Direction) == PandoraDefinition)
		{
			OutDirection = Direction;
			return true;
		}
	}

	for (const EEnum_Direction Direction : { EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		if (!GetPandoraLoadoutDefinition(Direction))
		{
			OutDirection = Direction;
			return true;
		}
	}

	return false;
}

bool UPandoraComponent::ResolvePreferredAutoPandoraLoadoutDirection(
	const UPandoraDefinition* PandoraDefinition,
	EEnum_Direction& OutDirection) const
{
	OutDirection = EEnum_Direction::Center;
	if (!PandoraDefinition)
	{
		return false;
	}

	const EEnum_Direction CurrentWeaponDirection = GetCurrentWeaponLoadoutDirection();
	if (PandoraLoadout::IsLoadoutDirection(CurrentWeaponDirection)
		&& IsPandoraCompatibleWithCurrentWeapon(PandoraDefinition))
	{
		const UPandoraDefinition* ExistingPandoraInWeaponSlot = GetPandoraLoadoutDefinition(CurrentWeaponDirection);
		if (!ExistingPandoraInWeaponSlot || ExistingPandoraInWeaponSlot == PandoraDefinition)
		{
			OutDirection = CurrentWeaponDirection;

			return true;
		}
}

	return ResolveAutoPandoraLoadoutDirection(PandoraDefinition, OutDirection);
}

bool UPandoraComponent::SetPandoraLoadoutSlotInternal(
	const EEnum_Direction Direction,
	const UPandoraDefinition* PandoraDefinition)
{
	if (!HasPandoraAuthority() || !PandoraLoadout::IsLoadoutDirection(Direction))
	{
		return false;
	}
	if (PandoraDefinition && !HasPandoraDefinition(PandoraDefinition))
	{
		return false;
	}

	bool bLoadoutChanged = false;
	if (!PandoraDefinition)
	{
		for (int32 Index = PandoraLoadoutSlots.Num() - 1; Index >= 0; --Index)
		{
			if (PandoraLoadoutSlots[Index].Direction == Direction)
			{
				PandoraLoadoutSlots.RemoveAt(Index);
				bLoadoutChanged = true;
				break;
			}
		}
	}
	else if (FPandoraLoadoutSlot* ExistingSlot = PandoraLoadout::FindSlot(PandoraLoadoutSlots, Direction))
	{
		if (ExistingSlot->PandoraDefinition != PandoraDefinition)
		{
			ExistingSlot->PandoraDefinition = PandoraDefinition;
			bLoadoutChanged = true;
		}
	}
	else
	{
		FPandoraLoadoutSlot& NewSlot = PandoraLoadoutSlots.AddDefaulted_GetRef();
		NewSlot.Direction = Direction;
		NewSlot.PandoraDefinition = PandoraDefinition;
		bLoadoutChanged = true;
	}

	if (bLoadoutChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, PandoraLoadoutSlots, this);
		NotifyPandoraLoadoutChanged();
	}
	return true;
}

const UPandoraDefinition* UPandoraComponent::FindOwnedPandoraDefinitionByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId) const
{
	if (!PandoraDefinitionId.IsValid())
	{
		return nullptr;
	}

	for (const FReplicatedPandoraEntry& Entry : ReplicatedEntries.Entries)
	{
		const UPandoraDefinition* PandoraDefinition = Entry.PandoraDefinition;
		if (IsValid(PandoraDefinition) && PandoraDefinition->GetPrimaryAssetId() == PandoraDefinitionId)
		{
			return PandoraDefinition;
		}
	}

	return nullptr;
}

int32 UPandoraComponent::FindReplicatedEntryIndexByDefinition(const UPandoraDefinition* PandoraDefinition) const
{
	if (!IsValid(PandoraDefinition))
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < ReplicatedEntries.Entries.Num(); ++Index)
	{
		if (ReplicatedEntries.Entries[Index].PandoraDefinition == PandoraDefinition)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

FReplicatedPandoraEntry* UPandoraComponent::FindReplicatedEntryByDefinition(const UPandoraDefinition* PandoraDefinition)
{
	const int32 EntryIndex = FindReplicatedEntryIndexByDefinition(PandoraDefinition);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}

const FReplicatedPandoraEntry* UPandoraComponent::FindReplicatedEntryByDefinition(const UPandoraDefinition* PandoraDefinition) const
{
	const int32 EntryIndex = FindReplicatedEntryIndexByDefinition(PandoraDefinition);
	return EntryIndex != INDEX_NONE ? &ReplicatedEntries.Entries[EntryIndex] : nullptr;
}

// ----------------------------------------------------------------------------------------------------------------------
