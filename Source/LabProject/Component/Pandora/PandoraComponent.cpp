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
#include "Pandora/PandoraInstance.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Pandora/PandoraSkillBinder.h"
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

UPandoraComponent::UPandoraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReplicatedEntries.Owner = this;
}

void UPandoraComponent::BeginPlay()
{
	Super::BeginPlay();

	ReplicatedEntries.Owner = this;

	UProjectTagConfig::Get(this)->GetPandoraFilterTypeTags(FilterTypeTags);
	if (HasPandoraAuthority())
	{
		InitializeReplicatedEntriesFromRuntimePandoras();
		AddPandorasByPrimaryAssetIds(AllPandroaDefinition);
	}
	else
	{
		RebuildRuntimePandorasFromReplicatedEntries();
	}

	RefreshPlacedPandoraSkillPreloads();
}

void UPandoraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasPandoraAuthority())
	{
		ClearSelectedPandoraContent();
	}

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

void UPandoraComponent::AddPandorasByPrimaryAssetIds(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
	if (!HasPandoraAuthority())
	{

		return;
	}

	if (PandoraDefinitions.IsEmpty())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.LoadPrimaryAssets(
		PandoraDefinitions,
		{},
		FStreamableDelegate::CreateWeakLambda(this, [this, PandoraDefinitions]()
		{
			if (!HasPandoraAuthority())
			{
				return;
			}

			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			for (const FPrimaryAssetId& PandoraDefinitionId : PandoraDefinitions)
			{
				const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(LoadedAssetManager.GetPrimaryAssetObject(PandoraDefinitionId));
				if (!IsValid(PandoraDefinition))
				{

					continue;
				}

				if (FindReplicatedEntryByDefinition(PandoraDefinition))
				{
					continue;
				}

				UPandoraInstance* NewPandoraInstance = NewObject<UPandoraInstance>(this);
				NewPandoraInstance->PandoraDefinition = PandoraDefinition;
				NewPandoraInstance->IsOwned = false;
				AddReplicatedPandora(NewPandoraInstance);
			}
		}));
}

void UPandoraComponent::ActivatePandoras(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
	ActivatePandorasWithLoadout(PandoraDefinitions, {});
}

void UPandoraComponent::ActivatePandorasWithLoadout(
	const TArray<FPrimaryAssetId>& PandoraDefinitions,
	const TMap<EEnum_Direction, FPrimaryAssetId>& PandoraLoadoutByDirection)
{
	if (!HasPandoraAuthority())
	{

		return;
	}

	if (PandoraDefinitions.IsEmpty())
	{
		return;
	}

	TArray<FPrimaryAssetId> DefinitionIdsToLoad = PandoraDefinitions;
	for (const TPair<EEnum_Direction, FPrimaryAssetId>& LoadoutPair : PandoraLoadoutByDirection)
	{
		if (PandoraLoadout::IsLoadoutDirection(LoadoutPair.Key) && LoadoutPair.Value.IsValid())
		{
			DefinitionIdsToLoad.AddUnique(LoadoutPair.Value);
		}
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.LoadPrimaryAssets(
		DefinitionIdsToLoad,
		{},
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, PandoraDefinitions, PandoraLoadoutByDirection]()
		{
			if (!HasPandoraAuthority())
			{
				return;
			}

			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			for (const FPrimaryAssetId& PandoraDefinitionId : PandoraDefinitions)
			{
				const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(LoadedAssetManager.GetPrimaryAssetObject(PandoraDefinitionId));
				if (!IsValid(PandoraDefinition))
				{

					continue;
				}

				if (FReplicatedPandoraEntry* ExistingEntry = FindReplicatedEntryByDefinition(PandoraDefinition))
				{
					if (!ExistingEntry->IsOwned)
					{
						ExistingEntry->IsOwned = true;
						ReplicatedEntries.MarkEntryDirty(*ExistingEntry);
						MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, ReplicatedEntries, this);
					}

					if (UPandoraInstance* PandoraInstance = FindPandoraInstanceByDefinition(PandoraDefinition))
					{
						PandoraInstance->IsOwned = true;
					}
					else
					{
						UPandoraInstance* NewPandoraInstance = NewObject<UPandoraInstance>(this);
						NewPandoraInstance->PandoraDefinition = PandoraDefinition;
						NewPandoraInstance->IsOwned = true;
						AllPandoraList.Pandoras.Add(NewPandoraInstance);
						FilterPandoras(NewPandoraInstance);
					}

					continue;
				}

				UPandoraInstance* NewPandoraInstance = NewObject<UPandoraInstance>(this);
				NewPandoraInstance->PandoraDefinition = PandoraDefinition;
				NewPandoraInstance->IsOwned = true;
				AddReplicatedPandora(NewPandoraInstance);
			}

			for (const EEnum_Direction Direction :
				{ EEnum_Direction::Left, EEnum_Direction::Up, EEnum_Direction::Right })
			{
				const FPrimaryAssetId* PandoraDefinitionId = PandoraLoadoutByDirection.Find(Direction);
				if (!PandoraDefinitionId || !PandoraDefinitionId->IsValid())
				{
					continue;
				}

				const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(
					LoadedAssetManager.GetPrimaryAssetObject(*PandoraDefinitionId));
				if (IsValid(PandoraDefinition))
				{
					SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition, true);
				}
			}
		}));
}

void UPandoraComponent::ClearAllPandoras()
{
	if (!HasPandoraAuthority())
	{
		return;
	}

	const bool bHadPandoras = !AllPandoraList.Pandoras.IsEmpty() || !ReplicatedEntries.Entries.IsEmpty();
	const bool bHadSelection = CurrentPandoraDefinition != nullptr
		|| CurrentPandoraLoadoutDirection != EEnum_Direction::Center
		|| !SelectedPandoraContent.AbilityHandles.IsEmpty();
	const bool bHadLoadout = !PandoraLoadoutSlots.IsEmpty();

	ClearSelectedPandoraContent();
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
		RebuildFilteredPandoraMap();
	}
}

void UPandoraComponent::FilterPandoras(UPandoraInstance* PandoraInstance)
{
	const UPandoraDefinition* PandoraDefinition = IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
	if (!PandoraDefinition)
	{
		return;
	}

	if (!PandoraDefinition->IdTag.IsValid())
	{

		return;
	}

	for (const FGameplayTag& TypeTag : FilterTypeTags)
	{
		if (PandoraDefinition->IdTag.MatchesTag(TypeTag))
		{
			AddValueToMap(TypeTag, PandoraInstance);
		}
	}
}

void UPandoraComponent::AddValueToMap(FGameplayTag TypeTag, UPandoraInstance* PandoraInstance)
{
	if (!IsValid(PandoraInstance))
	{
		return;
	}

	Map_Type_PandoraList.FindOrAdd(TypeTag).Pandoras.AddUnique(PandoraInstance);
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
		UPandoraInstance* PandoraInstance = FindOwnedPandoraInstanceByPrimaryAssetId(PandoraDefinitionId);
		if (!PandoraInstance)
		{
			LogRejectedServerRequest(
				TEXT("SetLoadoutSlot"),
				FString::Printf(
					TEXT("Pandora is not owned or cannot be resolved. AssetId=%s"),
					*PandoraDefinitionId.ToString()));
			return;
		}

		PandoraDefinition = IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
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

	const UPandoraDefinition* PandoraDefinition = nullptr;
	UPandoraInstance* PandoraInstance = FindOwnedPandoraInstanceByPrimaryAssetId(PandoraDefinitionId);
	if (!PandoraInstance)
	{
		LogRejectedServerRequest(
			TEXT("AutoSetLoadoutSlot"),
			FString::Printf(
				TEXT("Pandora is not owned or cannot be resolved. AssetId=%s"),
				*PandoraDefinitionId.ToString()));
		return;
	}

	PandoraDefinition =
		IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
	if (!PandoraDefinition)
	{
		LogRejectedServerRequest(
			TEXT("AutoSetLoadoutSlot"),
			FString::Printf(
				TEXT("owned Pandora instance has no definition. AssetId=%s"),
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

UPandoraInstance* UPandoraComponent::GetPandoraLoadoutInstance(const EEnum_Direction Direction) const
{
	return FindPandoraInstanceByDefinition(GetPandoraLoadoutDefinition(Direction));
}

void UPandoraComponent::ApplyProjectTagConfig(const UProjectTagConfig* ProjectTagConfig)
{
	const UProjectTagConfig* EffectiveConfig = ProjectTagConfig ? ProjectTagConfig : UProjectTagConfig::GetDefaultConfig();
	EffectiveConfig->GetPandoraFilterTypeTags(FilterTypeTags);

	RebuildFilteredPandoraMap();
}

void UPandoraComponent::InitializeReplicatedEntriesFromRuntimePandoras()
{
	for (UPandoraInstance* PandoraInstance : AllPandoraList.Pandoras)
	{
		AddReplicatedPandora(PandoraInstance);
	}
}

void UPandoraComponent::RebuildRuntimePandorasFromReplicatedEntries()
{
	AllPandoraList.Pandoras.Reset();

	for (const FReplicatedPandoraEntry& Entry : ReplicatedEntries.Entries)
	{
		if (!IsValid(Entry.PandoraDefinition))
		{
			continue;
		}

		UPandoraInstance* NewPandoraInstance = NewObject<UPandoraInstance>(this);
		NewPandoraInstance->PandoraDefinition = Entry.PandoraDefinition;
		NewPandoraInstance->IsOwned = Entry.IsOwned;
		AllPandoraList.Pandoras.Add(NewPandoraInstance);
	}

	RebuildFilteredPandoraMap();
}

void UPandoraComponent::RebuildFilteredPandoraMap()
{
	Map_Type_PandoraList.Reset();

	for (UPandoraInstance* PandoraInstance : AllPandoraList.Pandoras)
	{
		FilterPandoras(PandoraInstance);
	}
}

void UPandoraComponent::HandleReplicatedEntryAddedOrChanged(const FReplicatedPandoraEntry& Entry)
{
	if (!IsValid(Entry.PandoraDefinition))
	{
		return;
	}

	UPandoraInstance* PandoraInstance = FindPandoraInstanceByDefinition(Entry.PandoraDefinition);
	if (!IsValid(PandoraInstance))
	{
		PandoraInstance = NewObject<UPandoraInstance>(this);
		PandoraInstance->PandoraDefinition = Entry.PandoraDefinition;
		AllPandoraList.Pandoras.Add(PandoraInstance);
		FilterPandoras(PandoraInstance);
	}

	PandoraInstance->IsOwned = Entry.IsOwned;
}

void UPandoraComponent::HandleReplicatedEntryRemoved(const UPandoraDefinition* PandoraDefinition)
{
	if (!IsValid(PandoraDefinition))
	{
		return;
	}

	for (int32 Index = AllPandoraList.Pandoras.Num() - 1; Index >= 0; --Index)
	{
		UPandoraInstance* PandoraInstance = AllPandoraList.Pandoras[Index];
		if (IsValid(PandoraInstance) && PandoraInstance->PandoraDefinition == PandoraDefinition)
		{
			AllPandoraList.Pandoras.RemoveAt(Index);
		}
	}

	RebuildFilteredPandoraMap();
}

void UPandoraComponent::AddReplicatedPandora(UPandoraInstance* PandoraInstance)
{
	if (!HasPandoraAuthority() || !IsValid(PandoraInstance))
	{
		return;
	}

	const UPandoraDefinition* PandoraDefinition = PandoraInstance->PandoraDefinition.Get();
	if (!IsValid(PandoraDefinition))
	{
		return;
	}

	if (FReplicatedPandoraEntry* ExistingEntry = FindReplicatedEntryByDefinition(PandoraDefinition))
	{
		if (ExistingEntry->IsOwned != PandoraInstance->IsOwned)
		{
			ExistingEntry->IsOwned = PandoraInstance->IsOwned;
			ReplicatedEntries.MarkEntryDirty(*ExistingEntry);
			MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, ReplicatedEntries, this);
		}

		if (UPandoraInstance* ExistingInstance = FindPandoraInstanceByDefinition(PandoraDefinition))
		{
			ExistingInstance->IsOwned = PandoraInstance->IsOwned;
		}

		return;
	}

	AllPandoraList.Pandoras.AddUnique(PandoraInstance);
	FilterPandoras(PandoraInstance);

	FReplicatedPandoraEntry& NewEntry = ReplicatedEntries.Entries.AddDefaulted_GetRef();
	NewEntry.PandoraDefinition = PandoraDefinition;
	NewEntry.IsOwned = PandoraInstance->IsOwned;
	ReplicatedEntries.MarkEntryDirty(NewEntry);
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, ReplicatedEntries, this);
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
	UPdAbilitySystemComponent* ASC = PlayerStateOwner ? PlayerStateOwner->GetPdAbilitySystemComponent() : nullptr;

	if (!PandoraDefinitionId.IsValid())
	{
		const bool bDefinitionChanged = CurrentPandoraDefinition != nullptr;
		const bool bDirectionChanged = CurrentPandoraLoadoutDirection != EEnum_Direction::Center;

		ClearSelectedPandoraContent();
		CurrentPandoraDefinition = nullptr;
		CurrentPandoraLoadoutDirection = EEnum_Direction::Center;

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

	UPandoraInstance* PandoraInstance = FindOwnedPandoraInstanceByPrimaryAssetId(PandoraDefinitionId);
	const UPandoraDefinition* PandoraDefinition = IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
	if (!PandoraInstance || !PandoraDefinition)
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
	UPdAbilitySystemComponent* ASC = PlayerStateOwner ? PlayerStateOwner->GetPdAbilitySystemComponent() : nullptr;
	const int32 RuntimeLevel = ResolveSelectedPandoraRuntimeLevel(CurrentPandoraDefinition);

	ClearSelectedPandoraContent();

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
		FPandoraSkillBindingResult BindingResult = FPandoraSkillBinder::GrantPandoraContent(
			this,
			GetOwner(),
			ASC,
			CurrentPandoraDefinition,
			RuntimeLevel,
			EffectiveLoadoutDirection);
		SelectedPandoraContent.Capture(MoveTemp(BindingResult));

	}

	RefreshSelectedPandoraAbilityBindings(ASC);
	NotifyPandoraSelectionChanged();
}

bool UPandoraComponent::IsPandoraCompatibleWithCurrentWeapon(const UPandoraDefinition* PandoraDefinition) const
{
	return !PandoraDefinition || PandoraDefinition->IsCompatibleWithWeaponDefinition(GetCurrentWeaponDefinition());
}

void UPandoraComponent::ClearSelectedPandoraContent()
{
	APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	if (UPdAbilitySystemComponent* ASC = PlayerStateOwner ? PlayerStateOwner->GetPdAbilitySystemComponent() : nullptr)
	{
		ASC->ResetPandoraAbilityRuntimeState();
		FPandoraSkillBinder::RemoveGrantedContent(ASC, SelectedPandoraContent.AbilityHandles);
	}

	SelectedPandoraContent.Reset();
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
		return EEnum_Direction::Center;
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
		AbilitySystemComponent = PlayerStateOwner ? PlayerStateOwner->GetPdAbilitySystemComponent() : nullptr;
	}

	if (!AbilitySystemComponent)
	{
		return;
	}

	FPandoraSkillBinder::RefreshInputBindings(
		AbilitySystemComponent,
		CurrentPandoraDefinition,
		ResolveSelectedPandoraRuntimeLevel(CurrentPandoraDefinition),
		CurrentPandoraDefinition && IsPandoraCompatibleWithCurrentWeapon(CurrentPandoraDefinition));
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
	APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	if (UPdAbilitySystemComponent* ASC = PlayerStateOwner ? PlayerStateOwner->GetPdAbilitySystemComponent() : nullptr)
	{
		ASC->NotifyAbilitiesChanged();
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
			PandoraDefinition->Skill.Num(),
			UPandoraDefinition::GetFixedMaxLevel());
		for (int32 SkillIndex = 0; SkillIndex < SkillCount; ++SkillIndex)
		{
			const USkillDefinition* SkillDefinition =
				PandoraDefinition->Skill[SkillIndex].SkillDefinition.Get();
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
	const EEnum_Direction SelectedDirection = PlayerState
		? PandoraLoadout::GetDirectionFromLoadoutNumber(
			PlayerState->GetSelectedWeaponPandoraLoadoutNumber())
		: EEnum_Direction::Center;
	const bool bUpdatesSelectedLoadout = SelectedDirection == Direction;

	if (PandoraDefinition && bRequireOwnedPandora)
	{
		const UPandoraInstance* PandoraInstance = FindPandoraInstanceByDefinition(PandoraDefinition);
		if (!PandoraInstance || !PandoraInstance->IsOwned)
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
				if (PlayerState && bUpdatesSelectedLoadout)
				{
					PlayerState->ApplySelectedWeaponPandoraLoadout();
				}
				return true;
			}
		}

		if (PlayerState && bUpdatesSelectedLoadout)
		{
			PlayerState->ApplySelectedWeaponPandoraLoadout();
		}
		return true;
	}

	if (FPandoraLoadoutSlot* ExistingSlot = FindPandoraLoadoutSlot(Direction))
	{
		if (ExistingSlot->PandoraDefinition == PandoraDefinition)
		{
			if (PlayerState && bUpdatesSelectedLoadout)
			{
				PlayerState->ApplySelectedWeaponPandoraLoadout();
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
	if (PlayerState && bUpdatesSelectedLoadout)
	{
		PlayerState->ApplySelectedWeaponPandoraLoadout();
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

UPandoraInstance* UPandoraComponent::FindPandoraInstanceByDefinition(const UPandoraDefinition* PandoraDefinition) const
{
	if (!IsValid(PandoraDefinition))
	{
		return nullptr;
	}

	for (UPandoraInstance* PandoraInstance : AllPandoraList.Pandoras)
	{
		if (IsValid(PandoraInstance) && PandoraInstance->PandoraDefinition == PandoraDefinition)
		{
			return PandoraInstance;
		}
	}

	return nullptr;
}

UPandoraInstance* UPandoraComponent::FindPandoraInstanceByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId) const
{
	if (!PandoraDefinitionId.IsValid())
	{
		return nullptr;
	}

	for (UPandoraInstance* PandoraInstance : AllPandoraList.Pandoras)
	{
		const UPandoraDefinition* PandoraDefinition = IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
		if (PandoraDefinition && PandoraDefinition->GetPrimaryAssetId() == PandoraDefinitionId)
		{
			return PandoraInstance;
		}
	}

	return nullptr;
}

UPandoraInstance* UPandoraComponent::FindOwnedPandoraInstanceByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId) const
{
	UPandoraInstance* PandoraInstance = FindPandoraInstanceByPrimaryAssetId(PandoraDefinitionId);
	return IsValid(PandoraInstance) && PandoraInstance->IsOwned ? PandoraInstance : nullptr;
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
