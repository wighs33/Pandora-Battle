#include "Provision/DefaultPlayerProvisioner.h"

#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/StatUpgradeComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemInstance.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraLoadoutTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DefaultPlayerProvisioner)

DEFINE_LOG_CATEGORY_STATIC(LogDefaultPlayerProvisioner, Log, All);

namespace
{
	FGameplayTag ResolveGestureSlotTag(const int32 GestureSlotIndex)
	{
		switch (GestureSlotIndex)
		{
		case 0:
			return LabGameplayTags::Skin_Gesture_Slot1;
		case 1:
			return LabGameplayTags::Skin_Gesture_Slot2;
		case 2:
			return LabGameplayTags::Skin_Gesture_Slot3;
		case 3:
			return LabGameplayTags::Skin_Gesture_Slot4;
		default:
			return FGameplayTag();
		}
	}
}

UWorld* UDefaultPlayerProvisioner::GetWorld() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && GetOuter()
		? GetOuter()->GetWorld()
		: nullptr;
}

void UDefaultPlayerProvisioner::SetDefinition(
	const UDefaultProvisionDefinition* InDefinition)
{
	if (ProvisionDefinition.Get() == InDefinition && !bShuttingDown)
	{
		return;
	}
	ProvisionDefinition =
		const_cast<UDefaultProvisionDefinition*>(InDefinition);
	InitializedInventories.Reset();
	InitializedInventoryModes.Reset();
	CompletedItemPlayerStates.Reset();
	InitializedModeValues.Reset();
	InitializedPandoras.Reset();
	InitializedGestureEquipment.Reset();
	PendingItemPlayerStates.Reset();
	AttemptedContentLoadModes.Reset();
	bLoggedMissingProvisionDefinition = false;
	bShuttingDown = false;
}

const UDefaultProvisionDefinition*
UDefaultPlayerProvisioner::GetDefinition() const
{
	return ProvisionDefinition
		? ProvisionDefinition.Get()
		: UDefaultProvisionDefinition::ResolveDefaultDefinition();
}

void UDefaultPlayerProvisioner::ProvisionPlayer(
	APlayerController* PlayerController,
	const EDefaultProvisionMode Mode)
{
	if (bShuttingDown || !PlayerController)
	{
		return;
	}

	if (!EnsureContentLoaded(PlayerController, Mode))
	{
		return;
	}

	if (TryProvisionPlayer(PlayerController, Mode))
	{
		ClearRetryTimer(PlayerController);
		return;
	}

	ScheduleRetry(PlayerController, Mode);
}

bool UDefaultPlayerProvisioner::EnsureContentLoaded(
	APlayerController* PlayerController,
	const EDefaultProvisionMode Mode)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		if (!bLoggedMissingProvisionDefinition)
		{
			bLoggedMissingProvisionDefinition = true;
			UE_LOG(
				LogDefaultPlayerProvisioner,
				Error,
				TEXT("Required DA_DefaultProvision is missing; no default grants were applied."));
		}
		return true;
	}

	TArray<FPrimaryAssetId> ContentIds;
	for (const FDefaultProvisionPandoraGrant& Grant
		: Definition->GetPandoraGrants())
	{
		if (Grant.PandoraDefinitionId.IsValid()
			&& Grant.Levels.GetLevel(Mode) >= 0)
		{
			ContentIds.AddUnique(Grant.PandoraDefinitionId);
		}
	}
	for (const FDefaultProvisionGestureSlotGrant& Grant
		: Definition->GetGestureGrants())
	{
		if (Grant.SkinDefinitionId.IsValid()
			&& ResolveGestureSlotTag(Grant.GestureSlotIndex).IsValid())
		{
			ContentIds.AddUnique(Grant.SkinDefinitionId);
		}
	}
	if (Definition->GetGrantAllWeapons().IsEnabled(Mode)
		|| Definition->GetGrantAllEquipment().IsEnabled(Mode))
	{
		TArray<FPrimaryAssetId> ItemDefinitionIds;
		UAssetManager::Get().GetPrimaryAssetIdList(
			FPrimaryAssetType(TEXT("ItemDefinition")),
			ItemDefinitionIds);
		for (const FPrimaryAssetId& ItemDefinitionId
			: ItemDefinitionIds)
		{
			ContentIds.AddUnique(ItemDefinitionId);
		}
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const bool bAllLoaded = !ContentIds.ContainsByPredicate(
		[&AssetManager](const FPrimaryAssetId& AssetId)
		{
			return !AssetManager.GetPrimaryAssetObject(AssetId);
		});
	if (bAllLoaded)
	{
		return true;
	}

	const TObjectKey<AController> ControllerKey(PlayerController);
	if (const EDefaultProvisionMode* AttemptedMode =
		AttemptedContentLoadModes.Find(ControllerKey);
		AttemptedMode && *AttemptedMode == Mode)
	{
		return true;
	}
	if (PendingContentControllers.Contains(ControllerKey))
	{
		return false;
	}
	PendingContentControllers.Add(ControllerKey);

	const TWeakObjectPtr<APlayerController> WeakPlayer(PlayerController);
	TSharedPtr<FStreamableHandle> LoadHandle =
		AssetManager.LoadPrimaryAssets(
			ContentIds,
			{},
			FStreamableDelegate::CreateWeakLambda(
				this,
				[this, WeakPlayer, ControllerKey, Mode]()
				{
					AttemptedContentLoadModes.Add(ControllerKey, Mode);
					if (PendingContentControllers.Remove(ControllerKey) > 0
						&& !bShuttingDown)
					{
						ProvisionPlayer(WeakPlayer.Get(), Mode);
					}
					CleanupLoadHandles();
				}));
	if (LoadHandle.IsValid())
	{
		PendingLoadHandles.Add(LoadHandle);
		return false;
	}

	PendingContentControllers.Remove(ControllerKey);
	AttemptedContentLoadModes.Add(ControllerKey, Mode);
	return true;
}

bool UDefaultPlayerProvisioner::TryProvisionPlayer(
	APlayerController* PlayerController,
	const EDefaultProvisionMode Mode)
{
	APdPlayerState* PlayerState = PlayerController
		? PlayerController->GetPlayerState<APdPlayerState>()
		: nullptr;
	if (!PlayerState || !PlayerState->HasAuthority())
	{
		return false;
	}

	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		return true;
	}

	const bool bModeValuesReady = ApplyModeValues(PlayerState, Mode);
	const bool bItemsReady = ApplyItems(PlayerState, Mode);
	const bool bPandorasReady = ApplyPandoras(PlayerState, Mode);
	const bool bGesturesReady = ApplyGestures(
		PlayerController,
		PlayerState);
	PlayerState->ForceNetUpdate();
	return bModeValuesReady
		&& bItemsReady
		&& bPandorasReady
		&& bGesturesReady;
}

int32 UDefaultPlayerProvisioner::GetInventoryItemQuantity(
	const UInventoryComponent* InventoryComponent,
	const FPrimaryAssetId ItemDefinitionId) const
{
	if (!InventoryComponent || !ItemDefinitionId.IsValid())
	{
		return 0;
	}

	int32 TotalQuantity = 0;
	for (const UItemInstance* ItemInstance
		: InventoryComponent->GetAllItems().Items)
	{
		const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
			? ItemInstance->ItemDefinition.Get()
			: nullptr;
		if (ItemDefinition
			&& ItemDefinition->GetPrimaryAssetId() == ItemDefinitionId)
		{
			TotalQuantity += FMath::Max(ItemInstance->Quantity, 0);
		}
	}
	return TotalQuantity;
}

bool UDefaultPlayerProvisioner::ApplyItems(
	APdPlayerState* PlayerState,
	const EDefaultProvisionMode Mode)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition || !PlayerState)
	{
		return true;
	}

	const bool bHasConfiguredItems =
		Definition->GetItemGrants().ContainsByPredicate(
			[Mode](const FDefaultProvisionItemStackGrant& Grant)
			{
				return Grant.ItemDefinitionId.IsValid()
					&& Grant.Counts.GetCount(Mode) > 0;
			});
	const bool bGrantAllWeapons =
		Definition->GetGrantAllWeapons().IsEnabled(Mode);
	const bool bGrantAllEquipment =
		Definition->GetGrantAllEquipment().IsEnabled(Mode);
	if (Mode == EDefaultProvisionMode::Lobby
		&& !bHasConfiguredItems
		&& !bGrantAllWeapons
		&& !bGrantAllEquipment)
	{
		return true;
	}

	UInventoryComponent* InventoryComponent =
		PlayerState->GetInventoryComponent();
	if (!InventoryComponent)
	{
		return false;
	}

	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	const TWeakObjectPtr<UInventoryComponent>* InitializedInventory =
		InitializedInventories.Find(PlayerStateKey);
	const bool bSameInventoryAndMode = InitializedInventory
		&& InitializedInventory->Get() == InventoryComponent
		&& InitializedInventoryModes.FindRef(PlayerStateKey) == Mode;
	if (!bSameInventoryAndMode)
	{
		InitializedInventories.Add(PlayerStateKey, InventoryComponent);
		InitializedInventoryModes.Add(PlayerStateKey, Mode);
		PendingItemPlayerStates.Remove(PlayerStateKey);
		CompletedItemPlayerStates.Remove(PlayerStateKey);
		if (Mode != EDefaultProvisionMode::Lobby)
		{
			InventoryComponent->ClearAllItems();
		}
	}
	else if (CompletedItemPlayerStates.Contains(PlayerStateKey))
	{
		return true;
	}

	if (PendingItemPlayerStates.Contains(PlayerStateKey))
	{
		if (InventoryComponent->HasPendingItemLoads())
		{
			return false;
		}
		PendingItemPlayerStates.Remove(PlayerStateKey);
	}

	bool bIssuedRequest = false;
	TSet<FPrimaryAssetId> ExplicitGrantIds;
	for (const FDefaultProvisionItemStackGrant& Grant
		: Definition->GetItemGrants())
	{
		const int32 Quantity = Grant.Counts.GetCount(Mode);
		if (!Grant.ItemDefinitionId.IsValid() || Quantity <= 0)
		{
			continue;
		}
		ExplicitGrantIds.Add(Grant.ItemDefinitionId);

		const int32 CurrentQuantity = GetInventoryItemQuantity(
			InventoryComponent,
			Grant.ItemDefinitionId);
		if (Grant.QuickSlotIndex >= 0
			&& Grant.QuickSlotIndex
				< UInventoryComponent::ConsumableQuickSlotCount)
		{
			const UItemInstance* QuickSlotItem =
				InventoryComponent->GetConsumableQuickSlotItem(
					Grant.QuickSlotIndex);
			const UItemDefinition* QuickSlotDefinition =
				IsValid(QuickSlotItem)
					? QuickSlotItem->ItemDefinition.Get()
					: nullptr;
			const bool bQuickSlotMatches = QuickSlotDefinition
				&& QuickSlotDefinition->GetPrimaryAssetId()
					== Grant.ItemDefinitionId;
			if (CurrentQuantity != Quantity || !bQuickSlotMatches)
			{
				InventoryComponent
					->SetConsumableItemQuantityAndQuickSlotByPrimaryAssetId(
						Grant.ItemDefinitionId,
						Quantity,
						Grant.QuickSlotIndex);
				bIssuedRequest = true;
			}
		}
		else if (CurrentQuantity != Quantity)
		{
			InventoryComponent->SetItemQuantityByPrimaryAssetId(
				Grant.ItemDefinitionId,
				Quantity);
			bIssuedRequest = true;
		}
	}

	if (bGrantAllWeapons || bGrantAllEquipment)
	{
		TSet<FPrimaryAssetId> ExistingItemIds;
		for (const UItemInstance* ItemInstance
			: InventoryComponent->GetAllItems().Items)
		{
			const UItemDefinition* ItemDefinition = IsValid(ItemInstance)
				? ItemInstance->ItemDefinition.Get()
				: nullptr;
			if (ItemDefinition)
			{
				ExistingItemIds.Add(ItemDefinition->GetPrimaryAssetId());
			}
		}

		const UProjectTagConfig* TagConfig =
			UProjectTagConfig::GetDefaultConfig();
		const FGameplayTag WeaponTypeTag = TagConfig
			? TagConfig->GetItemWeaponTypeTag()
			: LabGameplayTags::Item_Weapon;
		const FGameplayTag EquipmentTypeTag = TagConfig
			? TagConfig->GetItemEquipmentTypeTag()
			: LabGameplayTags::Item_Equipment;

		TArray<FPrimaryAssetId> AllItemDefinitionIds;
		UAssetManager& AssetManager = UAssetManager::Get();
		AssetManager.GetPrimaryAssetIdList(
			FPrimaryAssetType(TEXT("ItemDefinition")),
			AllItemDefinitionIds);
		if (AllItemDefinitionIds.IsEmpty())
		{
			return false;
		}

		TArray<FPrimaryAssetId> MissingPolicyItemIds;
		for (const FPrimaryAssetId& ItemDefinitionId
			: AllItemDefinitionIds)
		{
			if (!ItemDefinitionId.IsValid()
				|| ExplicitGrantIds.Contains(ItemDefinitionId)
				|| ExistingItemIds.Contains(ItemDefinitionId))
			{
				continue;
			}

			const UItemDefinition* ItemDefinition =
				Cast<UItemDefinition>(
					AssetManager.GetPrimaryAssetObject(
						ItemDefinitionId));
			if (!ItemDefinition)
			{
				continue;
			}

			const bool bMatchesWeapon = bGrantAllWeapons
				&& ItemDefinition->IsWeaponDefinition(WeaponTypeTag);
			const bool bMatchesEquipment = bGrantAllEquipment
				&& ItemDefinition->MatchesItemType(EquipmentTypeTag);
			if (bMatchesWeapon || bMatchesEquipment)
			{
				MissingPolicyItemIds.Add(ItemDefinitionId);
			}
		}
		if (!MissingPolicyItemIds.IsEmpty())
		{
			InventoryComponent->AddItemsByPrimaryAssetIds(
				MissingPolicyItemIds);
			bIssuedRequest = true;
		}
	}

	if (bIssuedRequest || InventoryComponent->HasPendingItemLoads())
	{
		PendingItemPlayerStates.Add(PlayerStateKey);
		return false;
	}
	CompletedItemPlayerStates.Add(PlayerStateKey);
	return true;
}

bool UDefaultPlayerProvisioner::ApplyModeValues(
	APdPlayerState* PlayerState,
	const EDefaultProvisionMode Mode)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition || !PlayerState || !PlayerState->HasAuthority())
	{
		return Definition == nullptr;
	}
	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	if (const EDefaultProvisionMode* InitializedMode =
		InitializedModeValues.Find(PlayerStateKey);
		InitializedMode && *InitializedMode == Mode)
	{
		return true;
	}

	const float StatusPointValue =
		Definition->GetStatusPointValues().GetValue(Mode);
	UStatUpgradeComponent* StatUpgradeComponent =
		PlayerState->GetStatUpgradeComponent();
	if (StatUpgradeComponent)
	{
		if (!StatUpgradeComponent->SetPointsForAllCategories(
			StatusPointValue))
		{
			return false;
		}
	}
	else if (StatusPointValue > 0.0f)
	{
		return false;
	}

	const int32 SoulDustValue =
		Definition->GetSoulDustValues().GetCount(Mode);
	UPandoraTreeComponent* PandoraTreeComponent =
		PlayerState->GetPandoraTreeComponent();
	if (PandoraTreeComponent)
	{
		PandoraTreeComponent->SetSoulDust(SoulDustValue);
	}
	else if (SoulDustValue > 0)
	{
		return false;
	}
	InitializedModeValues.Add(PlayerStateKey, Mode);
	return true;
}

bool UDefaultPlayerProvisioner::
ApplyConfiguredStatusPointsForPlayerState(
	APlayerState* PlayerState,
	const EDefaultProvisionMode Mode) const
{
	APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!PdPlayerState || !PdPlayerState->HasAuthority() || !Definition)
	{
		return false;
	}

	UStatUpgradeComponent* StatUpgradeComponent =
		PdPlayerState->GetStatUpgradeComponent();
	const float PointValue =
		Definition->GetStatusPointValues().GetValue(Mode);
	return StatUpgradeComponent
		? StatUpgradeComponent->SetPointsForAllCategories(PointValue)
		: PointValue <= 0.0f;
}

bool UDefaultPlayerProvisioner::ApplyPandoras(
	APdPlayerState* PlayerState,
	const EDefaultProvisionMode Mode)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		return true;
	}
	const bool bShouldApplyPandoras =
		Mode != EDefaultProvisionMode::Lobby
		|| Definition->HasPandoraGrants(Mode);
	if (!bShouldApplyPandoras)
	{
		return true;
	}

	UPandoraComponent* PandoraComponent = PlayerState
		? PlayerState->GetPandoraComponent()
		: nullptr;
	UPandoraTreeComponent* PandoraTreeComponent = PlayerState
		? PlayerState->GetPandoraTreeComponent()
		: nullptr;
	if (!PlayerState
		|| !PandoraComponent || !PandoraTreeComponent)
	{
		return false;
	}

	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	const FInitializedPandoraState* InitializedState =
		InitializedPandoras.Find(PlayerStateKey);
	if (InitializedState
		&& InitializedState->Mode == Mode
		&& InitializedState->PandoraComponent.Get() == PandoraComponent
		&& InitializedState->PandoraTreeComponent.Get()
			== PandoraTreeComponent)
	{
		return true;
	}

	TArray<FPrimaryAssetId> UnlockedPandoraIds;
	TArray<FName> OwnedPandoraNames;
	TArray<FGrantedPandora> GrantedPandoras;
	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FDefaultProvisionPandoraGrant& Grant
		: Definition->GetPandoraGrants())
	{
		const int32 Level = Grant.Levels.GetLevel(Mode);
		if (!Grant.PandoraDefinitionId.IsValid() || Level < 0)
		{
			continue;
		}

		UPandoraDefinition* PandoraDefinition =
			Cast<UPandoraDefinition>(
				AssetManager.GetPrimaryAssetObject(
					Grant.PandoraDefinitionId));
		if (!PandoraDefinition)
		{
			continue;
		}

		UnlockedPandoraIds.AddUnique(
			PandoraDefinition->GetPrimaryAssetId());
		OwnedPandoraNames.AddUnique(PandoraDefinition->GetFName());
		if (Level > 0)
		{
			GrantedPandoras.Add(FGrantedPandora(PandoraDefinition, Level));
		}
	}

	PandoraComponent->ClearAllPandoras();
	PandoraTreeComponent->SetOwnedPandoraNames(OwnedPandoraNames);
	const int32 ConfiguredSoulDust =
		Definition->GetSoulDustValues().GetCount(Mode);
	PandoraTreeComponent->InitializeFromDefaultProvision(
		GrantedPandoras,
		ConfiguredSoulDust);

	TMap<EEnum_Direction, FPrimaryAssetId> LoadoutByDirection;
	if (Mode == EDefaultProvisionMode::Gameplay)
	{
		if (UPdGameInstance* GameInstance =
			PlayerState->GetGameInstance<UPdGameInstance>())
		{
			TMap<EEnum_Direction, FName> CachedNamesByDirection;
			if (GameInstance
				->TryGetCachedLobbyPandoraLoadoutForPlayerState(
					PlayerState,
					CachedNamesByDirection))
			{
				for (const TPair<EEnum_Direction, FName>& Pair
					: CachedNamesByDirection)
				{
					if (!PandoraLoadout::IsLoadoutDirection(Pair.Key)
						|| Pair.Value.IsNone())
					{
						continue;
					}
					if (const UPandoraDefinition* PandoraDefinition =
						GameInstance->GetPandoraDefinitionByName(Pair.Value))
					{
						const FPrimaryAssetId PandoraId =
							PandoraDefinition->GetPrimaryAssetId();
						if (UnlockedPandoraIds.Contains(PandoraId))
						{
							LoadoutByDirection.Add(Pair.Key, PandoraId);
						}
					}
				}
			}
		}
	}
	PandoraComponent->ActivatePandorasWithLoadout(
		UnlockedPandoraIds,
		LoadoutByDirection);

	FInitializedPandoraState& NewState =
		InitializedPandoras.FindOrAdd(PlayerStateKey);
	NewState.Mode = Mode;
	NewState.PandoraComponent = PandoraComponent;
	NewState.PandoraTreeComponent = PandoraTreeComponent;
	return true;
}

bool UDefaultPlayerProvisioner::ApplyGestures(
	APlayerController* PlayerController,
	APdPlayerState* PlayerState)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition || Definition->GetGestureGrants().IsEmpty())
	{
		return true;
	}
	ACharacterBase* Character = PlayerController
		? Cast<ACharacterBase>(PlayerController->GetPawn())
		: nullptr;
	USkinComponent* SkinComponent = PlayerState
		? PlayerState->GetSkinComponent()
		: nullptr;
	USkinEquipmentComponent* SkinEquipmentComponent = Character
		? Character->GetSkinEquipmentComponent()
		: nullptr;
	if (!PlayerState
		|| !SkinComponent || !SkinEquipmentComponent)
	{
		return false;
	}

	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	if (const TWeakObjectPtr<USkinEquipmentComponent>* InitializedEquipment =
		InitializedGestureEquipment.Find(PlayerStateKey);
		InitializedEquipment
		&& InitializedEquipment->Get() == SkinEquipmentComponent)
	{
		return true;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FDefaultProvisionGestureSlotGrant& Grant
		: Definition->GetGestureGrants())
	{
		const FGameplayTag SlotTag =
			ResolveGestureSlotTag(Grant.GestureSlotIndex);
		USkinDefinition* SkinDefinition =
			Grant.SkinDefinitionId.IsValid() && SlotTag.IsValid()
				? Cast<USkinDefinition>(
					AssetManager.GetPrimaryAssetObject(
						Grant.SkinDefinitionId))
				: nullptr;
		if (!SkinDefinition)
		{
			continue;
		}

		TArray<USkinDefinition*> DefinitionsToGrant = { SkinDefinition };
		SkinComponent->AddSkinDefinitions(DefinitionsToGrant);
		SkinEquipmentComponent->RequestEquipSkinDefinition(
			SkinDefinition,
			SlotTag);
	}
	InitializedGestureEquipment.Add(
		PlayerStateKey,
		SkinEquipmentComponent);
	return true;
}

void UDefaultPlayerProvisioner::ScheduleRetry(
	APlayerController* PlayerController,
	const EDefaultProvisionMode Mode)
{
	UWorld* World = GetWorld();
	if (bShuttingDown || !World || !PlayerController)
	{
		return;
	}

	const TObjectKey<APlayerController> ControllerKey(PlayerController);
	if (FTimerHandle* ExistingTimer =
		PendingRetryTimers.Find(ControllerKey);
		ExistingTimer
		&& World->GetTimerManager().IsTimerActive(*ExistingTimer))
	{
		if (PendingRetryModes.FindRef(ControllerKey) == Mode)
		{
			return;
		}
		World->GetTimerManager().ClearTimer(*ExistingTimer);
	}

	PendingRetryModes.Add(ControllerKey, Mode);
	const TWeakObjectPtr<APlayerController> WeakPlayer(PlayerController);
	const FTimerHandle TimerHandle =
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(
				this,
				[this, WeakPlayer, ControllerKey, Mode]()
				{
					PendingRetryTimers.Remove(ControllerKey);
					PendingRetryModes.Remove(ControllerKey);
					ProvisionPlayer(WeakPlayer.Get(), Mode);
				}));
	PendingRetryTimers.Add(ControllerKey, TimerHandle);
}

void UDefaultPlayerProvisioner::ClearRetryTimer(
	APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	const TObjectKey<APlayerController> ControllerKey(PlayerController);
	if (UWorld* World = GetWorld())
	{
		if (FTimerHandle* TimerHandle =
			PendingRetryTimers.Find(ControllerKey))
		{
			World->GetTimerManager().ClearTimer(*TimerHandle);
		}
	}
	PendingRetryTimers.Remove(ControllerKey);
	PendingRetryModes.Remove(ControllerKey);
}

void UDefaultPlayerProvisioner::ClearRuntimeStateForController(
	AController* Controller,
	APlayerState* PlayerState)
{
	if (APlayerController* PlayerController =
		Cast<APlayerController>(Controller))
	{
		ClearRetryTimer(PlayerController);
	}
	if (Controller)
	{
		PendingContentControllers.Remove(
			TObjectKey<AController>(Controller));
		AttemptedContentLoadModes.Remove(
			TObjectKey<AController>(Controller));
	}
	if (!PlayerState)
	{
		return;
	}

	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	PendingItemPlayerStates.Remove(PlayerStateKey);
	InitializedInventories.Remove(PlayerStateKey);
	InitializedInventoryModes.Remove(PlayerStateKey);
	CompletedItemPlayerStates.Remove(PlayerStateKey);
	InitializedModeValues.Remove(PlayerStateKey);
	InitializedPandoras.Remove(PlayerStateKey);
	InitializedGestureEquipment.Remove(PlayerStateKey);
}

void UDefaultPlayerProvisioner::CleanupLoadHandles()
{
	PendingLoadHandles.RemoveAll(
		[](const TSharedPtr<FStreamableHandle>& LoadHandle)
		{
			return !LoadHandle.IsValid() || LoadHandle->HasLoadCompleted();
		});
}

void UDefaultPlayerProvisioner::Shutdown()
{
	bShuttingDown = true;
	if (UWorld* World = GetWorld())
	{
		for (TPair<TObjectKey<APlayerController>, FTimerHandle>& Pair
			: PendingRetryTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	PendingRetryTimers.Reset();
	PendingRetryModes.Reset();
	PendingContentControllers.Reset();
	AttemptedContentLoadModes.Reset();
	PendingItemPlayerStates.Reset();
	InitializedInventories.Reset();
	InitializedInventoryModes.Reset();
	CompletedItemPlayerStates.Reset();
	InitializedModeValues.Reset();
	InitializedPandoras.Reset();
	InitializedGestureEquipment.Reset();

	for (const TSharedPtr<FStreamableHandle>& LoadHandle
		: PendingLoadHandles)
	{
		if (LoadHandle.IsValid() && !LoadHandle->HasLoadCompleted())
		{
			LoadHandle->CancelHandle();
		}
	}
	PendingLoadHandles.Reset();
}
