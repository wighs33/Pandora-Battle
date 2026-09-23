#include "Component/Pandora/PandoraComponent.h"

#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Definition/Item/ItemDefinition.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Pandora/PandoraSkillBinder.h"
#include "Pandora/PandoraSkillSource.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/SelectingPandoraAndWeaponComponent.h"
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
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

void FReplicatedPandoraEntry::PostReplicatedChange(const FReplicatedPandoraList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryAddedOrChanged(*this);
	}
}

void FReplicatedPandoraEntry::PreReplicatedRemove(const FReplicatedPandoraList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedEntryRemoved(PandoraDefinition);
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

	UProjectTagConfig::Get(this)->GetPandoraFilterTypeTags(FilterTypeTags);
	if (HasPandoraAuthority())
	{
		RebuildFilteredPandoraMap();
		AddPandorasByPrimaryAssetIds(AllPandroaDefinition);
	}
	else
	{
		RebuildPandoraListsFromReplicatedEntries();
	}

	RefreshPlacedPandoraSkillPreloads();
}

void UPandoraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingPandoraLoads();
	if (HasPandoraAuthority())
	{
		ClearGrantedPandoraContent();
	}
	ReleaseSkillSourcesForEndPlay();

	CachedCharacterOwner.Reset();
	ReplicatedEntries.Owner = nullptr;
	ReleasePlacedPandoraSkillPreloads();

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

// 잠긴 항목을 포함한 목록을 채우되, 이미 보유한 판도라는 잠그지 않는다.
void UPandoraComponent::AddPandorasByPrimaryAssetIds(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
	LoadPandoraDefinitions(PandoraDefinitions, {}, false);
}

void UPandoraComponent::ActivatePandoras(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
	ActivatePandorasWithLoadout(PandoraDefinitions, {});
}

void UPandoraComponent::ActivatePandorasWithLoadout(
	const TArray<FPrimaryAssetId>& PandoraDefinitions,
	const TMap<EEnum_Direction, FPrimaryAssetId>& PandoraLoadoutByDirection)
{
	LoadPandoraDefinitions(PandoraDefinitions, PandoraLoadoutByDirection, true);
}

// 이미 로드된 판도라를 지급할 때도 보유 상태와 복제 항목을 같은 경로로 변경한다.
bool UPandoraComponent::GrantPandoraDefinition(const UPandoraDefinition* PandoraDefinition)
{
	if (!HasPandoraAuthority() || !IsValid(PandoraDefinition))
	{
		return false;
	}
	if (AddPandoraDefinition(PandoraDefinition, true))
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
	return Entry && Entry->IsOwned;
}

void UPandoraComponent::LoadPandoraDefinitions(const TArray<FPrimaryAssetId>& PandoraDefinitions,
	const TMap<EEnum_Direction, FPrimaryAssetId>& PandoraLoadoutByDirection, const bool bOwned)
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
		[this, PandoraDefinitions, PandoraLoadoutByDirection, bOwned, RequestGeneration]()
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
				bChanged |= AddPandoraDefinition(Definition, bOwned);
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
					SetPandoraLoadoutSlotInternal(Pair.Key, Definition, true);
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

	const bool bHadPandoras = !AllPandoraList.Pandoras.IsEmpty() || !ReplicatedEntries.Entries.IsEmpty();
	const bool bHadSelection = CurrentPandoraDefinition != nullptr
		|| CurrentPandoraLoadoutDirection != EEnum_Direction::Center
		|| !GrantedPandoraAbilityHandles.IsEmpty();
	const bool bHadLoadout = !PandoraLoadoutSlots.IsEmpty();

	ClearGrantedPandoraContent();
	CurrentPandoraDefinition = nullptr;
	CurrentPandoraLoadoutDirection = EEnum_Direction::Center;
	PandoraLoadoutSlots.Reset();
	AllPandoraList.Pandoras.Reset();
	Map_Type_PandoraList.Reset();
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

void UPandoraComponent::FilterPandoras(const UPandoraDefinition* PandoraDefinition)
{
	if (!IsValid(PandoraDefinition))
	{
		return;
	}

	if (!PandoraDefinition->GetIdTag().IsValid())
	{

		return;
	}

	for (const FGameplayTag& TypeTag : FilterTypeTags)
	{
		if (PandoraDefinition->GetIdTag().MatchesTag(TypeTag))
		{
			Map_Type_PandoraList.FindOrAdd(TypeTag).Pandoras.AddUnique(PandoraDefinition);
		}
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

	return SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition, true);
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

	return SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition, true);
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

	if (!SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition, true))
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

	if (!SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition, true))
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
	const FPandoraLoadoutSlot* Slot = FindPandoraLoadoutSlot(Direction);
	return Slot ? Slot->PandoraDefinition.Get() : nullptr;
}

void UPandoraComponent::ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig)
{
	const UProjectTagConfig* EffectiveConfig = ProjectTagConfig ? ProjectTagConfig : UProjectTagConfig::GetDefaultConfig();
	EffectiveConfig->GetPandoraFilterTypeTags(FilterTypeTags);

	RebuildFilteredPandoraMap();
	OnPandoraInventoryChanged.Broadcast();
}

void UPandoraComponent::RebuildPandoraListsFromReplicatedEntries()
{
	AllPandoraList.Pandoras.Reset();

	for (const FReplicatedPandoraEntry& Entry : ReplicatedEntries.Entries)
	{
		if (!IsValid(Entry.PandoraDefinition))
		{
			continue;
		}

		AllPandoraList.Pandoras.AddUnique(Entry.PandoraDefinition);
	}

	RebuildFilteredPandoraMap();
	OnPandoraInventoryChanged.Broadcast();
}

void UPandoraComponent::RebuildFilteredPandoraMap()
{
	Map_Type_PandoraList.Reset();

	for (const UPandoraDefinition* PandoraDefinition : AllPandoraList.Pandoras)
	{
		FilterPandoras(PandoraDefinition);
	}
}

void UPandoraComponent::HandleReplicatedEntryAddedOrChanged(const FReplicatedPandoraEntry& Entry)
{
	if (!IsValid(Entry.PandoraDefinition))
	{
		return;
	}

	if (!AllPandoraList.Pandoras.Contains(Entry.PandoraDefinition))
	{
		AllPandoraList.Pandoras.Add(Entry.PandoraDefinition);
		FilterPandoras(Entry.PandoraDefinition);
	}

	bReplicatedInventoryChanged = true;
}

void UPandoraComponent::HandleReplicatedEntryRemoved(const UPandoraDefinition* PandoraDefinition)
{
	if (!IsValid(PandoraDefinition))
	{
		return;
	}

	AllPandoraList.Pandoras.Remove(PandoraDefinition);

	RebuildFilteredPandoraMap();
	bReplicatedInventoryChanged = true;
}

bool UPandoraComponent::AddPandoraDefinition(const UPandoraDefinition* PandoraDefinition, const bool bOwned)
{
	FReplicatedPandoraEntry* Entry = FindReplicatedEntryByDefinition(PandoraDefinition);
	bool bChanged = false;
	if (!Entry)
	{
		Entry = &ReplicatedEntries.Entries.AddDefaulted_GetRef();
		Entry->PandoraDefinition = PandoraDefinition;
		bChanged = true;
	}
	if (bOwned && !Entry->IsOwned)
	{
		Entry->IsOwned = true;
		bChanged = true;
	}
	if (bChanged)
	{
		ReplicatedEntries.MarkEntryDirty(*Entry);
		MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, ReplicatedEntries, this);
	}

	if (!AllPandoraList.Pandoras.Contains(PandoraDefinition))
	{
		AllPandoraList.Pandoras.Add(PandoraDefinition);
		FilterPandoras(PandoraDefinition);
		bChanged = true;
	}
	return bChanged;
}

bool UPandoraComponent::SelectPandoraByPrimaryAssetId(
	FPrimaryAssetId PandoraDefinitionId,
	const EEnum_Direction RequestedDirection)
{
	if (!HasPandoraAuthority())
	{

		return false;
	}

	APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	UPdAbilitySystemComponent* ASC = PlayerStateOwner
		? Cast<UPdAbilitySystemComponent>(PlayerStateOwner->GetAbilitySystemComponent())
		: nullptr;

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

		RefreshSelectedPandoraAbilityBindings(ASC);
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

	RefreshCurrentPandoraForWeaponChange();

	return true;
}

void UPandoraComponent::RefreshCurrentPandoraForWeaponChange()
{
	if (!HasPandoraAuthority())
	{

		return;
	}

	APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	UPdAbilitySystemComponent* ASC = PlayerStateOwner
		? Cast<UPdAbilitySystemComponent>(PlayerStateOwner->GetAbilitySystemComponent())
		: nullptr;
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
	if (ASC && CurrentPandoraDefinition && bApplyPandoraContentOnSelection && bCompatibleWithCurrentWeapon)
	{
		GrantedPandoraAbilityHandles.Append(FPandoraSkillBinder::GrantPandoraContent(
			this, ASC, CurrentPandoraDefinition, RuntimeLevel, EffectiveLoadoutDirection));

	}

	RefreshSelectedPandoraAbilityBindings(ASC);
	NotifyPandoraSelectionChanged();
}

bool UPandoraComponent::IsPandoraCompatibleWithCurrentWeapon(const UPandoraDefinition* PandoraDefinition) const
{
	return !PandoraDefinition || PandoraDefinition->IsCompatibleWithWeaponDefinition(GetCurrentWeaponDefinition());
}

void UPandoraComponent::ClearGrantedPandoraContent()
{
	APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	if (UPdAbilitySystemComponent* ASC = Cast<UPdAbilitySystemComponent>(
		PlayerStateOwner ? PlayerStateOwner->GetAbilitySystemComponent() : nullptr))
	{
		FPandoraSkillBinder::RemoveGrantedContent(ASC, GrantedPandoraAbilityHandles);
	}

	GrantedPandoraAbilityHandles.Reset();
	// 부여 실패나 GAS의 대기 중 추가 취소로 회수 콜백이 없었던 출처도 정리한다.
	// 목록 잠금으로 회수가 보류된 능력의 출처는 실제 회수 이벤트까지 유지한다.
	const TArray<TObjectPtr<UPandoraSkillSource>> Sources = OwnedSkillSources;
	for (UPandoraSkillSource* Source : Sources)
	{
		ReleaseSkillSourceIfUnused(Source);
	}
}

int32 UPandoraComponent::ResolveSelectedPandoraRuntimeLevel(const UPandoraDefinition* PandoraDefinition) const
{
	int32 PandoraLevel = 0;
	if (const APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner()))
	{
		if (UPandoraTreeComponent* PandoraTreeComponent = PlayerStateOwner->GetPandoraTreeComponent())
		{
			PandoraLevel = PandoraTreeComponent->GetCurrentPandoraLevel(const_cast<UPandoraDefinition*>(PandoraDefinition));
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

const ACharacterBase* UPandoraComponent::ResolveCurrentCharacterOwner() const
{
	const APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	if (!PlayerStateOwner)
	{
		CachedCharacterOwner.Reset();
		return nullptr;
	}

	if (const ACharacterBase* CachedCharacter = CachedCharacterOwner.Get())
	{
		if (CachedCharacter->GetPlayerState() == PlayerStateOwner)
		{
			return CachedCharacter;
		}

		CachedCharacterOwner.Reset();
	}

	if (ACharacterBase* CharacterOwner = Cast<ACharacterBase>(PlayerStateOwner->GetPawn()))
	{
		CachedCharacterOwner = CharacterOwner;
		return CharacterOwner;
	}

	if (const AController* ControllerOwner = Cast<AController>(PlayerStateOwner->GetOwner()))
	{
		if (ACharacterBase* CharacterOwner = Cast<ACharacterBase>(ControllerOwner->GetPawn()))
		{
			if (CharacterOwner->GetPlayerState() == PlayerStateOwner)
			{
				CachedCharacterOwner = CharacterOwner;
				return CharacterOwner;
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ACharacterBase> It(World); It; ++It)
		{
			ACharacterBase* Candidate = *It;
			if (Candidate && Candidate->GetPlayerState() == PlayerStateOwner)
			{
				CachedCharacterOwner = Candidate;
				return Candidate;
			}
		}
	}

	return nullptr;
}

const UEquipmentComponent* UPandoraComponent::GetCurrentEquipmentComponent() const
{
	const ACharacterBase* CharacterOwner = ResolveCurrentCharacterOwner();
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

void UPandoraComponent::RefreshSelectedPandoraAbilityBindings(UPdAbilitySystemComponent* AbilitySystemComponent) const
{
	if (!HasPandoraAuthority())
	{
		return;
	}

	if (!AbilitySystemComponent)
	{
		const APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
		AbilitySystemComponent = PlayerStateOwner
			? Cast<UPdAbilitySystemComponent>(PlayerStateOwner->GetAbilitySystemComponent())
			: nullptr;
	}

	if (!AbilitySystemComponent)
	{
		return;
	}

	FPandoraSkillBinder::RefreshInputBindings(
		AbilitySystemComponent,
		CurrentPandoraDefinition,
		ResolveSelectedPandoraRuntimeLevel(CurrentPandoraDefinition),
		bApplyPandoraContentOnSelection && CurrentPandoraDefinition && IsPandoraCompatibleWithCurrentWeapon(CurrentPandoraDefinition));
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
	APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	if (UPdAbilitySystemComponent* ASC = Cast<UPdAbilitySystemComponent>(
		PlayerStateOwner ? PlayerStateOwner->GetAbilitySystemComponent() : nullptr))
	{
		ASC->OnAbilitiesChangedNative.Broadcast();
	}

	OnPandoraSelectionChanged.Broadcast(const_cast<UPandoraDefinition*>(CurrentPandoraDefinition.Get()));
}

void UPandoraComponent::NotifyPandoraLoadoutChanged()
{
	RefreshPlacedPandoraSkillPreloads();
	OnPandoraLoadoutChanged.Broadcast();
}

void UPandoraComponent::RefreshPlacedPandoraSkillPreloads()
{
	TSet<FPrimaryAssetId> DesiredSkillDefinitionIds;
	for (const EEnum_Direction Direction :
		{ EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
	{
		const UPandoraDefinition* PandoraDefinition =
			GetPandoraLoadoutDefinition(Direction);
		if (!PandoraDefinition)
		{
			continue;
		}

		const int32 SkillCount = FMath::Min(
			PandoraDefinition->GetSkillCount(),
			UPandoraDefinition::GetFixedMaxLevel());
		for (int32 SkillIndex = 0; SkillIndex < SkillCount; ++SkillIndex)
		{
			const USkillDefinition* SkillDefinition =
				PandoraDefinition->GetSkillDefinition(SkillIndex);
			if (SkillDefinition)
			{
				DesiredSkillDefinitionIds.Add(
					SkillDefinition->GetPrimaryAssetId());
			}
		}
	}

	for (auto HandleIt = PlacedPandoraSkillPreloadHandles.CreateIterator();
		HandleIt;
		++HandleIt)
	{
		if (DesiredSkillDefinitionIds.Contains(HandleIt.Key()))
		{
			continue;
		}

		if (HandleIt.Value().IsValid())
		{
			HandleIt.Value()->CancelHandle();
			HandleIt.Value()->ReleaseHandle();
		}
		HandleIt.RemoveCurrent();
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		if (!DesiredSkillDefinitionIds.IsEmpty())
		{
			UE_LOG(
				PandoraComponentLog,
				Error,
				TEXT("Placed Pandora skill preload could not start because ContentDataSubsystem is unavailable. Owner=%s"),
				*GetPathNameSafe(GetOwner()));
		}
		return;
	}

	for (const FPrimaryAssetId& SkillDefinitionId : DesiredSkillDefinitionIds)
	{
		if (PlacedPandoraSkillPreloadHandles.Contains(SkillDefinitionId))
		{
			continue;
		}

		TSharedPtr<FStreamableHandle> PreloadHandle =
			ContentSubsystem->PreloadPrimaryAssetsAsync({ SkillDefinitionId });
		PlacedPandoraSkillPreloadHandles.Add(
			SkillDefinitionId,
			MoveTemp(PreloadHandle));
	}
}

void UPandoraComponent::ReleasePlacedPandoraSkillPreloads()
{
	for (TPair<FPrimaryAssetId, TSharedPtr<FStreamableHandle>>& HandlePair :
		PlacedPandoraSkillPreloadHandles)
	{
		if (HandlePair.Value.IsValid())
		{
			HandlePair.Value->CancelHandle();
			HandlePair.Value->ReleaseHandle();
		}
	}
	PlacedPandoraSkillPreloadHandles.Reset();
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
	const UPandoraDefinition* PandoraDefinition,
	const bool bRequireOwnedPandora)
{
	if (!HasPandoraAuthority() || !PandoraLoadout::IsLoadoutDirection(Direction))
	{

		return false;
	}
	APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	USelectingPandoraAndWeaponComponent* PandoraAndWeaponComponent = PlayerState ? PlayerState->GetSelectingPandoraAndWeaponComponent() : nullptr;
	const EEnum_Direction SelectedDirection = PandoraAndWeaponComponent
		? PandoraLoadout::GetDirectionFromLoadoutNumber(
			PandoraAndWeaponComponent->GetSelectedPandoraAndWeaponNumber())
		: EEnum_Direction::Center;
	const bool bUpdatesSelectedLoadout = SelectedDirection == Direction;

	if (PandoraDefinition && bRequireOwnedPandora)
	{
		if (!HasPandoraDefinition(PandoraDefinition))
		{

			return false;
		}
	}

	if (!PandoraDefinition)
	{
		for (int32 Index = PandoraLoadoutSlots.Num() - 1; Index >= 0; --Index)
		{
			if (PandoraLoadoutSlots[Index].Direction == Direction)
			{
				PandoraLoadoutSlots.RemoveAt(Index);
				MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, PandoraLoadoutSlots, this);
				NotifyPandoraLoadoutChanged();
				if (PandoraAndWeaponComponent && bUpdatesSelectedLoadout)
				{
					PandoraAndWeaponComponent->ApplySelectedPandoraAndWeapon();
				}
				return true;
			}
		}

		if (PandoraAndWeaponComponent && bUpdatesSelectedLoadout)
		{
			PandoraAndWeaponComponent->ApplySelectedPandoraAndWeapon();
		}
		return true;
	}

	if (FPandoraLoadoutSlot* ExistingSlot = FindPandoraLoadoutSlot(Direction))
	{
		if (ExistingSlot->PandoraDefinition == PandoraDefinition)
		{
			if (PandoraAndWeaponComponent && bUpdatesSelectedLoadout)
			{
				PandoraAndWeaponComponent->ApplySelectedPandoraAndWeapon();
			}
			return true;
		}

		ExistingSlot->PandoraDefinition = const_cast<UPandoraDefinition*>(PandoraDefinition);
	}
	else
	{
		FPandoraLoadoutSlot& NewSlot = PandoraLoadoutSlots.AddDefaulted_GetRef();
		NewSlot.Direction = Direction;
		NewSlot.PandoraDefinition = const_cast<UPandoraDefinition*>(PandoraDefinition);

	}

	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, PandoraLoadoutSlots, this);
	NotifyPandoraLoadoutChanged();
	if (PandoraAndWeaponComponent && bUpdatesSelectedLoadout)
	{
		PandoraAndWeaponComponent->ApplySelectedPandoraAndWeapon();
	}

	return true;
}

FPandoraLoadoutSlot* UPandoraComponent::FindPandoraLoadoutSlot(const EEnum_Direction Direction)
{
	return PandoraLoadout::FindSlot(PandoraLoadoutSlots, Direction);
}

const FPandoraLoadoutSlot* UPandoraComponent::FindPandoraLoadoutSlot(const EEnum_Direction Direction) const
{
	return PandoraLoadout::FindSlot(PandoraLoadoutSlots, Direction);
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
		if (Entry.IsOwned && IsValid(PandoraDefinition) && PandoraDefinition->GetPrimaryAssetId() == PandoraDefinitionId)
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

void UPandoraComponent::BindAbilityRemoval()
{
	if (AbilityRemovedHandle.IsValid())
	{
		return;
	}
	const APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	UPdAbilitySystemComponent* ASC = PlayerState
		? Cast<UPdAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent()) : nullptr;
	if (ASC && HasPandoraAuthority())
	{
		SourceAbilitySystemComponent = ASC;
		AbilityRemovedHandle = ASC->OnAbilityRemovedNative.AddUObject(this, &ThisClass::HandleGrantedAbilityRemoved);
	}
}

UPandoraSkillSource* UPandoraComponent::CreateSkillSource(const UPandoraDefinition* Definition,
	const int32 SkillIndex, const EEnum_Direction LoadoutDirection)
{
	if (!HasPandoraAuthority() || !Definition || !Definition->GetSkillDefinition(SkillIndex))
	{
		return nullptr;
	}
	BindAbilityRemoval();
	if (!SourceAbilitySystemComponent.IsValid())
	{
		return nullptr;
	}

	UPandoraSkillSource* Source = NewObject<UPandoraSkillSource>(this);
	Source->Initialize(Definition, SkillIndex, LoadoutDirection);
	OwnedSkillSources.Add(Source);
	if (IsReadyForReplication())
	{
		AddReplicatedSubObject(Source);
	}
	return Source;
}

void UPandoraComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	if (!HasPandoraAuthority())
	{
		return;
	}
	BindAbilityRemoval();
	// 능력 부여 여부와 무관하게, 복제 준비 전에 생성된 출처도 등록한다.
	for (UPandoraSkillSource* Source : OwnedSkillSources)
	{
		if (IsValid(Source))
		{
			AddReplicatedSubObject(Source);
		}
	}
}

void UPandoraComponent::HandleGrantedAbilityRemoved(const FGameplayAbilitySpec& Spec)
{
	ReleaseSkillSourceIfUnused(Cast<UPandoraSkillSource>(Spec.SourceObject.Get()), Spec.Handle);
}

void UPandoraComponent::ReleaseSkillSourceIfUnused(UPandoraSkillSource* Source, const FGameplayAbilitySpecHandle RemovedHandle)
{
	if (!HasPandoraAuthority() || !IsValid(Source) || Source->GetOuter() != this
		|| !OwnedSkillSources.Contains(Source))
	{
		return;
	}
	if (const UPdAbilitySystemComponent* ASC = SourceAbilitySystemComponent.Get())
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			// 회수 콜백은 Spec이 목록에서 삭제되기 전에 실행된다.
			if (Spec.Handle != RemovedHandle && Spec.SourceObject.Get() == Source)
			{
				return;
			}
		}
	}
	DestroyReplicatedSubObjectOnRemotePeers(Source);
	OwnedSkillSources.Remove(Source);
}

void UPandoraComponent::HandleSkillSourceReplicated(UPandoraSkillSource* Source)
{
	if (!IsValid(Source) || Source->GetOuter() != this)
	{
		return;
	}
	// 클라이언트에서는 SourceObject의 약한 참조와 별개로 출처를 보관한다.
	OwnedSkillSources.AddUnique(Source);
	const APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	if (UPdAbilitySystemComponent* ASC = PlayerState
		? Cast<UPdAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent()) : nullptr)
	{
		// LocalPredicted 시전의 성공 응답은 GAS가 확인만 한다. 출처 수신은 UI 준비 상태만 갱신한다.
		ASC->OnAbilitiesChangedNative.Broadcast();
	}
}

void UPandoraComponent::HandleSkillSourceDestroyed(UPandoraSkillSource* Source)
{
	OwnedSkillSources.Remove(Source);
}

void UPandoraComponent::ReleaseSkillSourcesForEndPlay()
{
	if (UPdAbilitySystemComponent* ASC = SourceAbilitySystemComponent.Get())
	{
		ASC->OnAbilityRemovedNative.Remove(AbilityRemovedHandle);
	}
	AbilityRemovedHandle.Reset();
	SourceAbilitySystemComponent.Reset();
	if (HasPandoraAuthority())
	{
		for (UPandoraSkillSource* Source : OwnedSkillSources)
		{
			if (IsValid(Source))
			{
				DestroyReplicatedSubObjectOnRemotePeers(Source);
			}
		}
	}
	OwnedSkillSources.Reset();
}
