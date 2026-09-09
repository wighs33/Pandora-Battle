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
#include "Engine/GameInstance.h"
#include "Data/ContentDataSubsystem.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
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

bool UDefaultPlayerProvisioner::Initialize(
	const UDefaultProvisionDefinition* InDefinition, const EDefaultProvisionMode InMode)
{
	if (!InDefinition || bShuttingDown)
	{
		return false;
	}
	if (ProvisionDefinition)
	{
		// A different map/mode owns a new provisioner. In-flight grants cannot be reconfigured.
		if (ProvisionDefinition != InDefinition || Mode != InMode)
		{
			UE_LOG(LogDefaultPlayerProvisioner, Error,
				TEXT("Provision definition and mode are fixed for this runtime; create a new provisioner for a different configuration."));
			return false;
		}
		return true;
	}
	ProvisionDefinition = const_cast<UDefaultProvisionDefinition*>(InDefinition);
	Mode = InMode;
	return true;
}

const UDefaultProvisionDefinition*
UDefaultPlayerProvisioner::GetDefinition() const
{
	return IsInitialized() ? ProvisionDefinition.Get() : nullptr;
}

void UDefaultPlayerProvisioner::ProvisionPlayer(APlayerController* PlayerController)
{
	if (!IsInitialized() || !IsValid(PlayerController) || !PlayerController->HasAuthority())
	{
		return;
	}

	if (!EnsureContentLoaded(PlayerController))
	{
		return;
	}

	if (TryProvisionPlayer(PlayerController))
	{
		ClearRetryTimer(PlayerController);
		OnPlayerProvisioned.Broadcast(PlayerController);
		return;
	}

	ScheduleRetry(PlayerController);
}

bool UDefaultPlayerProvisioner::EnsureContentLoaded(APlayerController* PlayerController)
{
	if (ContentState == EContentState::Ready)
	{
		return true;
	}
	if (ContentState == EContentState::Failed)
	{
		return false;
	}
	PendingContentControllers.AddUnique(PlayerController);
	if (ContentState == EContentState::Loading)
	{
		return false;
	}

	const UDefaultProvisionDefinition* Definition = GetDefinition();
	check(Definition);
	for (const FDefaultProvisionItemStackGrant& Grant : Definition->GetItemGrants())
	{
		if (Grant.ItemDefinitionId.IsValid() && Grant.Counts.GetCount(Mode) > 0)
		{
			RequiredContentIds.AddUnique(Grant.ItemDefinitionId);
		}
	}
	for (const FDefaultProvisionPandoraGrant& Grant
		: Definition->GetPandoraGrants())
	{
		if (Grant.PandoraDefinitionId.IsValid()
			&& Grant.Levels.GetLevel(Mode) >= 0)
		{
			RequiredContentIds.AddUnique(Grant.PandoraDefinitionId);
		}
	}
	for (const FDefaultProvisionGestureSlotGrant& Grant
		: Definition->GetGestureGrants())
	{
		if (Grant.SkinDefinitionId.IsValid()
			&& ResolveGestureSlotTag(Grant.GestureSlotIndex).IsValid())
		{
			RequiredContentIds.AddUnique(Grant.SkinDefinitionId);
		}
	}
	if (Definition->GetGrantAllWeapons().IsEnabled(Mode)
		|| Definition->GetGrantAllEquipment().IsEnabled(Mode))
	{
		TArray<FPrimaryAssetId> ItemDefinitionIds;
		UAssetManager::Get().GetPrimaryAssetIdList(
			FPrimaryAssetType(TEXT("ItemDefinition")),
			ItemDefinitionIds);
		if (ItemDefinitionIds.IsEmpty())
		{
			ContentState = EContentState::Failed;
			PendingContentControllers.Reset();
			UE_LOG(LogDefaultPlayerProvisioner, Error, TEXT("Default inventory policy requires an ItemDefinition catalog, but it is empty."));
			return false;
		}
		for (const FPrimaryAssetId& ItemDefinitionId
			: ItemDefinitionIds)
		{
			RequiredContentIds.AddUnique(ItemDefinitionId);
		}
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const bool bAllLoaded = !RequiredContentIds.ContainsByPredicate(
		[&AssetManager](const FPrimaryAssetId& AssetId)
		{
			return !AssetManager.GetPrimaryAssetObject(AssetId);
		});
	if (bAllLoaded)
	{
		ContentState = EContentState::Ready;
		PendingContentControllers.Reset();
		return true;
	}

	// All players in this runtime share one immutable content request and its lifetime.
	ContentState = EContentState::Loading;
	ContentLoadHandle = AssetManager.LoadPrimaryAssets(RequiredContentIds, {},
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleContentLoaded));
	return false;
}

void UDefaultPlayerProvisioner::HandleContentLoaded()
{
	if (bShuttingDown || ContentState != EContentState::Loading)
	{
		return;
	}
	for (const FPrimaryAssetId& Id : RequiredContentIds)
	{
		if (!UAssetManager::Get().GetPrimaryAssetObject(Id))
		{
			ContentState = EContentState::Failed;
			PendingContentControllers.Reset();
			UE_LOG(LogDefaultPlayerProvisioner, Error,
				TEXT("Required provisioning content failed to load: %s. Default grants were not applied."), *Id.ToString());
			return;
		}
	}
	ContentState = EContentState::Ready;
	const TArray<TWeakObjectPtr<APlayerController>> WaitingPlayers = PendingContentControllers;
	for (const TWeakObjectPtr<APlayerController>& Player : WaitingPlayers)
	{
		// A previous player's callback may log out another waiting player or shut down the runtime.
		if (PendingContentControllers.Remove(Player) > 0)
		{
			ProvisionPlayer(Player.Get());
		}
	}
}

bool UDefaultPlayerProvisioner::TryProvisionPlayer(APlayerController* PlayerController)
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
		return false;
	}

	const bool bModeValuesReady = ApplyModeValues(PlayerState);
	const bool bItemsReady = ApplyItems(PlayerState);
	const bool bPandorasReady = ApplyPandoras(PlayerState);
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

bool UDefaultPlayerProvisioner::ApplyItems(APdPlayerState* PlayerState)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition || !PlayerState)
	{
		return false;
	}

	const bool bHasConfiguredItems =
		Definition->GetItemGrants().ContainsByPredicate(
			[this](const FDefaultProvisionItemStackGrant& Grant)
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
	FInventoryProvisionState& State = InventoryStates.FindOrAdd(PlayerStateKey);
	if (State.Inventory.Get() == InventoryComponent && State.bCompleted)
	{
		return true;
	}
	// Inventory owns asynchronous mutations. Never reconcile a snapshot with unfinished writes,
	// even after a controller's provisioning records have been cleared on logout.
	if (InventoryComponent->HasPendingItemLoads())
	{
		return false;
	}
	if (State.Inventory.Get() != InventoryComponent)
	{
		State.Inventory = InventoryComponent;
		State.bCompleted = false;
		if (Mode != EDefaultProvisionMode::Lobby)
		{
			InventoryComponent->ClearAllItems();
		}
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
		return false;
	}
	// A completed default grant is not a permanent inventory minimum (items can be used/upgraded).
	State.bCompleted = true;
	return true;
}

bool UDefaultPlayerProvisioner::ApplyModeValues(APdPlayerState* PlayerState)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition || !PlayerState || !PlayerState->HasAuthority())
	{
		return false;
	}
	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	if (InitializedModeValues.Contains(PlayerStateKey))
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
	InitializedModeValues.Add(PlayerStateKey);
	return true;
}

bool UDefaultPlayerProvisioner::
ApplyConfiguredStatusPointsForPlayerState(
	APlayerState* PlayerState) const
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

bool UDefaultPlayerProvisioner::ApplyPandoras(APdPlayerState* PlayerState)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		return false;
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
		&& InitializedState->PandoraComponent.Get() == PandoraComponent
		&& InitializedState->PandoraTreeComponent.Get()
			== PandoraTreeComponent)
	{
		return true;
	}

	TArray<FPrimaryAssetId> UnlockedPandoraIds;
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
		if (Level > 0)
		{
			GrantedPandoras.Add(FGrantedPandora(PandoraDefinition, Level));
		}
	}

	PandoraComponent->ClearAllPandoras();
	// 전체 초기화는 이전 목록 로딩도 취소하므로 잠긴 항목을 명시적으로 다시 채운다.
	PandoraComponent->AddPandorasByPrimaryAssetIds(PandoraComponent->AllPandroaDefinition);
	const int32 ConfiguredSoulDust =
		Definition->GetSoulDustValues().GetCount(Mode);
	PandoraTreeComponent->InitializeFromDefaultProvision(
		GrantedPandoras,
		ConfiguredSoulDust);

	TMap<EEnum_Direction, FPrimaryAssetId> LoadoutByDirection;
	if (Mode == EDefaultProvisionMode::Gameplay)
	{
		ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(PlayerState->GetGameInstance());
		UContentDataSubsystem* ContentDataSubsystem = UGameInstance::GetSubsystem<UContentDataSubsystem>(PlayerState->GetGameInstance());
		if (LobbySubsystem && ContentDataSubsystem)
		{
			TMap<EEnum_Direction, FName> CachedNamesByDirection;
			if (LobbySubsystem->TryGetCachedLobbyPandoraLoadoutForPlayerState(
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
						ContentDataSubsystem->GetPandoraDefinitionByName(Pair.Value))
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
	NewState.PandoraComponent = PandoraComponent;
	NewState.PandoraTreeComponent = PandoraTreeComponent;
	return true;
}

bool UDefaultPlayerProvisioner::ApplyGestures(
	APlayerController* PlayerController,
	APdPlayerState* PlayerState)
{
	const UDefaultProvisionDefinition* Definition = GetDefinition();
	if (!Definition)
	{
		return false;
	}
	if (Definition->GetGestureGrants().IsEmpty())
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

void UDefaultPlayerProvisioner::ScheduleRetry(APlayerController* PlayerController)
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
		return;
	}

	const TWeakObjectPtr<APlayerController> WeakPlayer(PlayerController);
	const FTimerHandle TimerHandle =
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(
				this,
				[this, WeakPlayer, ControllerKey]()
				{
					PendingRetryTimers.Remove(ControllerKey);
					ProvisionPlayer(WeakPlayer.Get());
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
}

void UDefaultPlayerProvisioner::ClearRuntimeStateForController(
	AController* Controller,
	APlayerState* PlayerState)
{
	if (APlayerController* PlayerController =
		Cast<APlayerController>(Controller))
	{
		ClearRetryTimer(PlayerController);
		PendingContentControllers.Remove(PlayerController);
	}
	if (!PlayerState)
	{
		return;
	}

	const TObjectKey<APlayerState> PlayerStateKey(PlayerState);
	InventoryStates.Remove(PlayerStateKey);
	InitializedModeValues.Remove(PlayerStateKey);
	InitializedPandoras.Remove(PlayerStateKey);
	InitializedGestureEquipment.Remove(PlayerStateKey);
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
	PendingContentControllers.Reset();
	InventoryStates.Reset();
	InitializedModeValues.Reset();
	InitializedPandoras.Reset();
	InitializedGestureEquipment.Reset();

	if (ContentLoadHandle.IsValid())
	{
		ContentLoadHandle->CancelHandle();
		ContentLoadHandle->ReleaseHandle();
		ContentLoadHandle.Reset();
	}
	RequiredContentIds.Reset();
	ProvisionDefinition = nullptr;
	OnPlayerProvisioned.Clear();
}
