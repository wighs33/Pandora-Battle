#include "PandoraComponent.h"

#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Character/PdCharacterBase.h"
#include "Common/ProjectTagConfig.h"
#include "Engine/AssetManager.h"
#include "Item/ItemDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraInstance.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Pandora/PandoraSkillBinder.h"
#include "PlayerComponent/EquipmentComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraComponent)

DEFINE_LOG_CATEGORY(PandoraComponentLog)

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
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		InitializeReplicatedEntriesFromRuntimePandoras();
		AddPandorasByPrimaryAssetIds(AllPandroaDefinition);
	}
	else
	{
		RebuildRuntimePandorasFromReplicatedEntries();
	}
}

void UPandoraComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraComponent, ReplicatedEntries, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraComponent, CurrentPandoraDefinition, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPandoraComponent, PandoraLoadoutSlots, Params);
}

void UPandoraComponent::AddPandorasByPrimaryAssetIds(const TArray<FPrimaryAssetId>& PandoraDefinitions)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(PandoraComponentLog, Warning, TEXT("AddPandorasByPrimaryAssetIds failed: pandoras can only be modified on the authority."));
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
			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			for (const FPrimaryAssetId& PandoraDefinitionId : PandoraDefinitions)
			{
				const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(LoadedAssetManager.GetPrimaryAssetObject(PandoraDefinitionId));
				if (!IsValid(PandoraDefinition))
				{
					UE_LOG(PandoraComponentLog, Warning, TEXT("AddPandorasByPrimaryAssetIds failed: could not resolve pandora definition '%s'."), *PandoraDefinitionId.ToString());
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
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(PandoraComponentLog, Warning, TEXT("ActivatePandoras failed: pandoras can only be modified on the authority."));
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
			UAssetManager& LoadedAssetManager = UAssetManager::Get();

			for (const FPrimaryAssetId& PandoraDefinitionId : PandoraDefinitions)
			{
				const UPandoraDefinition* PandoraDefinition = Cast<UPandoraDefinition>(LoadedAssetManager.GetPrimaryAssetObject(PandoraDefinitionId));
				if (!IsValid(PandoraDefinition))
				{
					UE_LOG(PandoraComponentLog, Warning, TEXT("ActivatePandoras failed: could not resolve pandora definition '%s'."), *PandoraDefinitionId.ToString());
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
		}));
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
		UE_LOG(PandoraComponentLog, Warning, TEXT("FilterPandoras skipped pandora '%s': invalid IdTag."), *GetNameSafe(PandoraDefinition));
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
	const FPrimaryAssetId PandoraDefinitionId = PandoraDefinition ? PandoraDefinition->GetPrimaryAssetId() : FPrimaryAssetId();

	if (!GetOwner())
	{
		return false;
	}

	if (!GetOwner()->HasAuthority())
	{
		ServerRequestPandoraSelection(PandoraDefinitionId);
		return true;
	}

	return SelectPandoraByPrimaryAssetId(PandoraDefinitionId);
}

void UPandoraComponent::ServerRequestPandoraSelection_Implementation(FPrimaryAssetId PandoraDefinitionId)
{
	SelectPandoraByPrimaryAssetId(PandoraDefinitionId);
}

bool UPandoraComponent::RequestSetPandoraLoadoutSlot(
	const EEnum_Direction Direction,
	const UPandoraDefinition* PandoraDefinition)
{
	if (!PandoraLoadout::IsLoadoutDirection(Direction))
	{
		UE_LOG(PandoraComponentLog, Warning, TEXT("RequestSetPandoraLoadoutSlot failed: invalid direction=%d"),
			static_cast<int32>(Direction));
		return false;
	}

	if (!GetOwner())
	{
		return false;
	}

	const FPrimaryAssetId PandoraDefinitionId = PandoraDefinition ? PandoraDefinition->GetPrimaryAssetId() : FPrimaryAssetId();
	if (!GetOwner()->HasAuthority())
	{
		ServerSetPandoraLoadoutSlot(Direction, PandoraDefinitionId);
		return true;
	}

	return SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition, true);
}

bool UPandoraComponent::RestorePandoraLoadoutSlot(
	const EEnum_Direction Direction,
	const UPandoraDefinition* PandoraDefinition)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	return SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition, false);
}

void UPandoraComponent::ServerSetPandoraLoadoutSlot_Implementation(
	EEnum_Direction Direction,
	FPrimaryAssetId PandoraDefinitionId)
{
	const UPandoraDefinition* PandoraDefinition = nullptr;
	if (PandoraDefinitionId.IsValid())
	{
		UPandoraInstance* PandoraInstance = FindPandoraInstanceByPrimaryAssetId(PandoraDefinitionId);
		PandoraDefinition = IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
	}

	SetPandoraLoadoutSlotInternal(Direction, PandoraDefinition, true);
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

TMap<FName, FName> UPandoraComponent::GetPandoraLoadoutSaveNames() const
{
	return PandoraLoadout::MakeSaveNames(PandoraLoadoutSlots);
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

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UKismetSystemLibrary::PrintString(this, TEXT("Filtering Pandoras"), true, true, FLinearColor(0.0f, 0.66f, 1.0f, 1.0f), 2.0f);
#endif
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
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(PandoraInstance))
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

bool UPandoraComponent::SelectPandoraByPrimaryAssetId(FPrimaryAssetId PandoraDefinitionId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	ClearSelectedPandoraContent();
	CurrentPandoraDefinition = nullptr;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, CurrentPandoraDefinition, this);

	APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	UPdAbilitySystemComponent* ASC = PlayerStateOwner ? PlayerStateOwner->GetPdAbilitySystemComponent() : nullptr;

	if (!PandoraDefinitionId.IsValid())
	{
		RefreshSelectedPandoraAbilityBindings(ASC);
		NotifyPandoraSelectionChanged();
		return true;
	}

	UPandoraInstance* PandoraInstance = FindPandoraInstanceByPrimaryAssetId(PandoraDefinitionId);
	const UPandoraDefinition* PandoraDefinition = IsValid(PandoraInstance) ? PandoraInstance->PandoraDefinition.Get() : nullptr;
	if (!PandoraInstance || !PandoraInstance->IsOwned || !PandoraDefinition)
	{
		UE_LOG(PandoraComponentLog, Warning, TEXT("SelectPandoraByPrimaryAssetId failed: id=%s instance=%s owned=%s definition=%s"),
			*PandoraDefinitionId.ToString(),
			*GetNameSafe(PandoraInstance),
			PandoraInstance && PandoraInstance->IsOwned ? TEXT("true") : TEXT("false"),
			*GetNameSafe(PandoraDefinition));
		NotifyPandoraSelectionChanged();
		return false;
	}

	CurrentPandoraDefinition = PandoraDefinition;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPandoraComponent, CurrentPandoraDefinition, this);
	RefreshCurrentPandoraForWeaponChange();
	UE_LOG(PandoraComponentLog, Log, TEXT("Selected pandora: owner=%s pandora=%s compatibleWeapon=%s requestedSkills=%d granted=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(PandoraDefinition),
		IsPandoraCompatibleWithCurrentWeapon(PandoraDefinition) ? TEXT("true") : TEXT("false"),
		PandoraDefinition->Skill.Num(),
		SelectedPandoraContent.GetGrantedAbilityCount());
	return true;
}

void UPandoraComponent::RefreshCurrentPandoraForWeaponChange()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	UPdAbilitySystemComponent* ASC = PlayerStateOwner ? PlayerStateOwner->GetPdAbilitySystemComponent() : nullptr;

	ClearSelectedPandoraContent();

	const bool bCompatibleWithCurrentWeapon = IsPandoraCompatibleWithCurrentWeapon(CurrentPandoraDefinition);
	if (ASC && CurrentPandoraDefinition && bApplyPandoraContentOnSelection && bCompatibleWithCurrentWeapon)
	{
		FPandoraSkillBindingResult BindingResult = FPandoraSkillBinder::GrantPandoraContent(
			this,
			GetOwner(),
			ASC,
			CurrentPandoraDefinition,
			ResolveSelectedPandoraRuntimeLevel(CurrentPandoraDefinition));
		SelectedPandoraContent.Capture(MoveTemp(BindingResult));
	}
	else if (CurrentPandoraDefinition && !bCompatibleWithCurrentWeapon)
	{
		const UItemDefinition* CurrentWeaponDefinition = GetCurrentWeaponDefinition();
		UE_LOG(PandoraComponentLog, Log, TEXT("Selected pandora disabled by current weapon: owner=%s pandora=%s weapon=%s weaponTag=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(CurrentPandoraDefinition.Get()),
			*GetNameSafe(CurrentWeaponDefinition),
			CurrentWeaponDefinition && CurrentWeaponDefinition->IdTag.IsValid() ? *CurrentWeaponDefinition->IdTag.ToString() : TEXT("None"));
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
		FPandoraSkillBinder::RemoveGrantedContent(ASC, SelectedPandoraContent.AbilityHandles, SelectedPandoraContent.EffectHandles);
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
		PandoraLevel = 1;
	}

	const int32 MaxUnlockLevel = PandoraDefinition ? PandoraDefinition->GetMaxLevel() : 1;
	return FMath::Clamp(PandoraLevel, 1, FMath::Max(MaxUnlockLevel, 1));
}

const UItemDefinition* UPandoraComponent::GetCurrentWeaponDefinition() const
{
	const APdPlayerState* PlayerStateOwner = Cast<APdPlayerState>(GetOwner());
	const APdCharacterBase* CharacterOwner = PlayerStateOwner ? Cast<APdCharacterBase>(PlayerStateOwner->GetPawn()) : nullptr;
	const UEquipmentComponent* EquipmentComponent = CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponDefinition() : nullptr;
}

void UPandoraComponent::RefreshSelectedPandoraAbilityBindings(UPdAbilitySystemComponent* AbilitySystemComponent) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
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
	OnPandoraLoadoutChanged.Broadcast();
}

bool UPandoraComponent::SetPandoraLoadoutSlotInternal(
	const EEnum_Direction Direction,
	const UPandoraDefinition* PandoraDefinition,
	const bool bRequireOwnedPandora)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !PandoraLoadout::IsLoadoutDirection(Direction))
	{
		return false;
	}

	if (PandoraDefinition && bRequireOwnedPandora)
	{
		const UPandoraInstance* PandoraInstance = FindPandoraInstanceByDefinition(PandoraDefinition);
		if (!PandoraInstance || !PandoraInstance->IsOwned)
		{
			UE_LOG(PandoraComponentLog, Warning,
				TEXT("SetPandoraLoadoutSlot failed: pandora is not owned. owner=%s direction=%d pandora=%s instance=%s owned=%s"),
				*GetNameSafe(GetOwner()),
				static_cast<int32>(Direction),
				*GetNameSafe(PandoraDefinition),
				*GetNameSafe(PandoraInstance),
				PandoraInstance && PandoraInstance->IsOwned ? TEXT("true") : TEXT("false"));
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
				return true;
			}
		}

		return true;
	}

	if (FPandoraLoadoutSlot* ExistingSlot = FindPandoraLoadoutSlot(Direction))
	{
		if (ExistingSlot->PandoraDefinition == PandoraDefinition)
		{
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
	UE_LOG(PandoraComponentLog, Log, TEXT("SetPandoraLoadoutSlot succeeded: owner=%s direction=%d pandora=%s requireOwned=%s"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(Direction),
		*GetNameSafe(PandoraDefinition),
		bRequireOwnedPandora ? TEXT("true") : TEXT("false"));
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
