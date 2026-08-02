#include "Lobby/Services/LobbyPreviewGrantService.h"

#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Lobby/LobbyPreviewDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Engine/AssetManager.h"
#include "Item/ItemInstance.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPreviewGrantService)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyPreviewGrantService, Log, All);

namespace
{
	FGameplayTag ResolveLobbyGestureSlotTag(const int32 GestureSlotIndex)
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

void ULobbyPreviewGrantService::ScheduleGrant(
	APlayerController* PlayerController,
	const int32 RemainingAttempts)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !GameMode->HasAuthority() || !PlayerController)
	{
		return;
	}

	const ULobbyPreviewDefinition* PreviewDefinition = GetPreviewDefinition();
	if (!PreviewDefinition)
	{
		return;
	}

	const int32 AttemptsLeft = RemainingAttempts == INDEX_NONE
		? PreviewDefinition->GetGrantMaxAttempts()
		: RemainingAttempts;
	if (TryGrant(PlayerController))
	{
		ClearRetryTimer(PlayerController);
		return;
	}

	if (AttemptsLeft <= 0)
	{
		ClearRetryTimer(PlayerController);
		return;
	}

	UWorld* World = GameMode->GetWorld();
	if (!World)
	{
		return;
	}

	const TObjectKey<APlayerController> ControllerKey(PlayerController);
	if (const FTimerHandle* ExistingTimer = PendingRetryTimers.Find(ControllerKey);
		ExistingTimer && World->GetTimerManager().IsTimerActive(*ExistingTimer))
	{
		return;
	}

	TWeakObjectPtr<APlayerController> WeakPlayerController(PlayerController);
	FTimerHandle& RetryTimerHandle = PendingRetryTimers.FindOrAdd(ControllerKey);
	World->GetTimerManager().SetTimer(
		RetryTimerHandle,
		FTimerDelegate::CreateWeakLambda(
			this,
			[this, WeakPlayerController, AttemptsLeft, ControllerKey]()
			{
				PendingRetryTimers.Remove(ControllerKey);
				ScheduleGrant(WeakPlayerController.Get(), AttemptsLeft - 1);
			}),
		PreviewDefinition->GetGrantRetryDelay(),
		false);
}

void ULobbyPreviewGrantService::HandlePlayerLogout(AController* ExitingController)
{
	APlayerController* PlayerController = Cast<APlayerController>(ExitingController);
	if (!PlayerController)
	{
		return;
	}

	ClearRetryTimer(PlayerController);
	if (const APdPlayerState* PlayerState = PlayerController->GetPlayerState<APdPlayerState>())
	{
		const FObjectKey PlayerStateKey(PlayerState);
		RequestedPlayerStates.Remove(PlayerStateKey);
		PendingGestureGrantPlayerStates.Remove(PlayerStateKey);
	}
}

void ULobbyPreviewGrantService::Shutdown()
{
	if (ALobbyGameMode* GameMode = GetLobbyGameMode())
	{
		for (TPair<TObjectKey<APlayerController>, FTimerHandle>& RetryTimer : PendingRetryTimers)
		{
			GameMode->GetWorldTimerManager().ClearTimer(RetryTimer.Value);
		}
	}
	PendingRetryTimers.Empty();

	CancelContentLoads();
	RequestedPlayerStates.Empty();
	PendingGestureGrantPlayerStates.Empty();
}

ALobbyGameMode* ULobbyPreviewGrantService::GetLobbyGameMode() const
{
	return Cast<ALobbyGameMode>(GetOuter());
}

const ULobbyPreviewDefinition* ULobbyPreviewGrantService::GetPreviewDefinition() const
{
	const ALobbyGameMode* GameMode = GetLobbyGameMode();
	return GameMode ? GameMode->GetLobbyPreviewDefinition() : nullptr;
}

bool ULobbyPreviewGrantService::TryGrant(APlayerController* PlayerController)
{
	APdPlayerState* PdPlayerState = PlayerController
		? PlayerController->GetPlayerState<APdPlayerState>()
		: nullptr;
	if (!PdPlayerState)
	{
		return false;
	}

	UInventoryComponent* InventoryComponent = PdPlayerState->GetInventoryComponent();
	USkinComponent* SkinComponent = PdPlayerState->GetSkinComponent();
	UPandoraComponent* PandoraComponent = PdPlayerState->GetPandoraComponent();
	UPandoraTreeComponent* PandoraTreeComponent = PdPlayerState->GetPandoraTreeComponent();
	const ULobbyPreviewDefinition* PreviewDefinition = GetPreviewDefinition();
	if (!PreviewDefinition)
	{
		return false;
	}

	TArray<FLobbyPreviewItemStackGrant> ItemStackGrants;
	PreviewDefinition->GetEffectivePreviewItemStackGrants(ItemStackGrants);
	TArray<FLobbyPreviewGestureSlotGrant> GestureSlotGrants;
	PreviewDefinition->GetEffectivePreviewGestureSlotGrants(GestureSlotGrants);
	const bool bShouldGrantItemStacks = !ItemStackGrants.IsEmpty();
	const bool bShouldGrantGestures = !GestureSlotGrants.IsEmpty();
	const APdPlayer* LobbyPlayer = Cast<APdPlayer>(PlayerController->GetPawn());
	USkinEquipmentComponent* SkinEquipmentComponent =
		LobbyPlayer ? LobbyPlayer->GetSkinEquipmentComponent() : nullptr;
	if (((PreviewDefinition->ShouldGrantAllWeapons() || bShouldGrantItemStacks) && !InventoryComponent)
		|| ((PreviewDefinition->ShouldGrantSavedSkins() || bShouldGrantGestures) && !SkinComponent)
		|| (bShouldGrantGestures && !SkinEquipmentComponent)
		|| (PreviewDefinition->ShouldGrantPreviewPandoras() && (!PandoraComponent || !PandoraTreeComponent)))
	{
		return false;
	}

	const FObjectKey PlayerStateKey(PdPlayerState);
	if (RequestedPlayerStates.Contains(PlayerStateKey))
	{
		if (bShouldGrantGestures)
		{
			GrantGestures(PlayerController, PdPlayerState, *PreviewDefinition);
		}
		return true;
	}

	RequestedPlayerStates.Add(PlayerStateKey);
	if (PreviewDefinition->ShouldGrantAllWeapons())
	{
		GrantWeapons(InventoryComponent);
	}
	if (bShouldGrantItemStacks)
	{
		GrantItemStacks(InventoryComponent, *PreviewDefinition);
	}
	if (PreviewDefinition->ShouldGrantSavedSkins())
	{
		GrantSavedSkins(PlayerController, PdPlayerState);
	}
	if (bShouldGrantGestures)
	{
		GrantGestures(PlayerController, PdPlayerState, *PreviewDefinition);
	}
	if (PreviewDefinition->ShouldGrantPreviewPandoras())
	{
		GrantPandoras(PdPlayerState);
	}

	return true;
}

void ULobbyPreviewGrantService::GrantItemStacks(
	UInventoryComponent* InventoryComponent,
	const ULobbyPreviewDefinition& PreviewDefinition)
{
	if (!InventoryComponent
		|| !InventoryComponent->GetOwner()
		|| !InventoryComponent->GetOwner()->HasAuthority())
	{
		return;
	}

	TArray<FLobbyPreviewItemStackGrant> ItemStackGrants;
	PreviewDefinition.GetEffectivePreviewItemStackGrants(ItemStackGrants);
	for (const FLobbyPreviewItemStackGrant& StackGrant : ItemStackGrants)
	{
		if (!StackGrant.ItemDefinitionId.IsValid() || StackGrant.Quantity <= 0)
		{
			continue;
		}

		if (StackGrant.QuickSlotIndex >= 0
			&& StackGrant.QuickSlotIndex < UInventoryComponent::ConsumableQuickSlotCount)
		{
			InventoryComponent->SetConsumableItemQuantityAndQuickSlotByPrimaryAssetId(
				StackGrant.ItemDefinitionId,
				StackGrant.Quantity,
				StackGrant.QuickSlotIndex);
		}
		else
		{
			InventoryComponent->SetItemQuantityByPrimaryAssetId(
				StackGrant.ItemDefinitionId,
				StackGrant.Quantity);
		}
	}
}

void ULobbyPreviewGrantService::GrantWeapons(UInventoryComponent* InventoryComponent)
{
	if (!InventoryComponent
		|| !InventoryComponent->GetOwner()
		|| !InventoryComponent->GetOwner()->HasAuthority())
	{
		return;
	}

	TArray<FPrimaryAssetId> ItemDefinitionIds;
	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("ItemDefinition")), ItemDefinitionIds);
	if (ItemDefinitionIds.IsEmpty())
	{
		return;
	}

	TWeakObjectPtr<UInventoryComponent> WeakInventoryComponent(InventoryComponent);
	TSharedPtr<FStreamableHandle> LoadHandle = AssetManager.LoadPrimaryAssets(
		ItemDefinitionIds,
		{},
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, WeakInventoryComponent, ItemDefinitionIds]()
			{
				UInventoryComponent* ResolvedInventoryComponent = WeakInventoryComponent.Get();
				if (!ResolvedInventoryComponent
					|| !ResolvedInventoryComponent->GetOwner()
					|| !ResolvedInventoryComponent->GetOwner()->HasAuthority())
				{
					CleanupLoadHandles();
					return;
				}

				TSet<FPrimaryAssetId> ExistingItemDefinitionIds;
				for (const UItemInstance* ItemInstance : ResolvedInventoryComponent->GetAllItems().Items)
				{
					const UItemDefinition* ItemDefinition =
						IsValid(ItemInstance) ? ItemInstance->ItemDefinition.Get() : nullptr;
					if (ItemDefinition)
					{
						ExistingItemDefinitionIds.Add(ItemDefinition->GetPrimaryAssetId());
					}
				}

				const UProjectTagConfig* ProjectTagConfig =
					UProjectTagConfig::Get(ResolvedInventoryComponent);
				const FGameplayTag WeaponTypeTag = ProjectTagConfig
					? ProjectTagConfig->GetItemWeaponTypeTag()
					: FGameplayTag();

				TArray<FPrimaryAssetId> MissingWeaponDefinitionIds;
				UAssetManager& LoadedAssetManager = UAssetManager::Get();
				for (const FPrimaryAssetId& ItemDefinitionId : ItemDefinitionIds)
				{
					const UItemDefinition* ItemDefinition =
						Cast<UItemDefinition>(LoadedAssetManager.GetPrimaryAssetObject(ItemDefinitionId));
					if (!IsValid(ItemDefinition)
						|| !ItemDefinition->IsWeaponDefinition(WeaponTypeTag)
						|| ExistingItemDefinitionIds.Contains(ItemDefinition->GetPrimaryAssetId()))
					{
						continue;
					}

					MissingWeaponDefinitionIds.Add(ItemDefinition->GetPrimaryAssetId());
				}

				if (!MissingWeaponDefinitionIds.IsEmpty())
				{
					ResolvedInventoryComponent->AddItemsByPrimaryAssetIds(MissingWeaponDefinitionIds);
				}

				CleanupLoadHandles();
			}));

	if (LoadHandle.IsValid())
	{
		PendingLoadHandles.Add(LoadHandle);
	}
}

void ULobbyPreviewGrantService::GrantSavedSkins(
	APlayerController* PlayerController,
	APdPlayerState* PdPlayerState)
{
	if (!PlayerController || !PdPlayerState || !PdPlayerState->HasAuthority())
	{
		return;
	}

	APdPlayerController* PdPlayerController =
		Cast<APdPlayerController>(PlayerController);
	if (!PdPlayerController)
	{
		return;
	}

	// Never load a remote player's profile from the listen host's filesystem.
	// The host applies its own local profile directly; remote clients submit
	// their local cosmetic profile through APdPlayerController's bounded RPC.
	PdPlayerController->RequestLocalCosmeticProfileSync();
}

void ULobbyPreviewGrantService::GrantGestures(
	APlayerController* PlayerController,
	APdPlayerState* PdPlayerState,
	const ULobbyPreviewDefinition& PreviewDefinition)
{
	if (!PlayerController || !PdPlayerState || !PdPlayerState->HasAuthority())
	{
		return;
	}

	TArray<FLobbyPreviewGestureSlotGrant> GestureSlotGrants;
	PreviewDefinition.GetEffectivePreviewGestureSlotGrants(GestureSlotGrants);
	TArray<FPrimaryAssetId> GestureDefinitionIds;
	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FLobbyPreviewGestureSlotGrant& GestureGrant : GestureSlotGrants)
	{
		const FGameplayTag SlotTag = ResolveLobbyGestureSlotTag(GestureGrant.GestureSlotIndex);
		if (!GestureGrant.SkinDefinitionId.IsValid() || !SlotTag.IsValid())
		{
			continue;
		}
		GestureDefinitionIds.AddUnique(GestureGrant.SkinDefinitionId);
	}

	if (GestureDefinitionIds.IsEmpty())
	{
		return;
	}

	const bool bAllGestureDefinitionsLoaded = GestureDefinitionIds.ContainsByPredicate(
		[&AssetManager](const FPrimaryAssetId& GestureDefinitionId)
		{
			return !AssetManager.GetPrimaryAssetObject(GestureDefinitionId);
		}) == false;
	if (bAllGestureDefinitionsLoaded)
	{
		ApplyLoadedGestures(PlayerController, PdPlayerState, GestureSlotGrants);
		return;
	}

	const FObjectKey PlayerStateKey(PdPlayerState);
	if (PendingGestureGrantPlayerStates.Contains(PlayerStateKey))
	{
		return;
	}
	PendingGestureGrantPlayerStates.Add(PlayerStateKey);

	const TWeakObjectPtr<APlayerController> WeakPlayerController(PlayerController);
	const TWeakObjectPtr<APdPlayerState> WeakPlayerState(PdPlayerState);
	TSharedPtr<FStreamableHandle> LoadHandle = AssetManager.LoadPrimaryAssets(
		GestureDefinitionIds,
		{},
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, WeakPlayerController, WeakPlayerState, PlayerStateKey,
				GestureSlotGrants = MoveTemp(GestureSlotGrants)]()
			{
				PendingGestureGrantPlayerStates.Remove(PlayerStateKey);
				ApplyLoadedGestures(
					WeakPlayerController.Get(),
					WeakPlayerState.Get(),
					GestureSlotGrants);
				CleanupLoadHandles();
			}));

	if (LoadHandle.IsValid())
	{
		PendingLoadHandles.Add(LoadHandle);
	}
	else if (PendingGestureGrantPlayerStates.Remove(PlayerStateKey) > 0)
	{
		UE_LOG(
			LogLobbyPreviewGrantService,
			Error,
			TEXT("Lobby preview gesture asynchronous load could not be started."));
	}
}

void ULobbyPreviewGrantService::ApplyLoadedGestures(
	APlayerController* PlayerController,
	APdPlayerState* PdPlayerState,
	const TArray<FLobbyPreviewGestureSlotGrant>& GestureSlotGrants)
{
	if (!PlayerController || !PdPlayerState || !PdPlayerState->HasAuthority())
	{
		return;
	}

	USkinComponent* SkinComponent = PdPlayerState->GetSkinComponent();
	const APdPlayer* LobbyPlayer = Cast<APdPlayer>(PlayerController->GetPawn());
	USkinEquipmentComponent* SkinEquipmentComponent =
		LobbyPlayer ? LobbyPlayer->GetSkinEquipmentComponent() : nullptr;
	if (!SkinComponent || !SkinEquipmentComponent)
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FLobbyPreviewGestureSlotGrant& GestureGrant : GestureSlotGrants)
	{
		const FGameplayTag SlotTag = ResolveLobbyGestureSlotTag(GestureGrant.GestureSlotIndex);
		if (!GestureGrant.SkinDefinitionId.IsValid() || !SlotTag.IsValid())
		{
			continue;
		}

		USkinDefinition* SkinDefinition =
			Cast<USkinDefinition>(AssetManager.GetPrimaryAssetObject(GestureGrant.SkinDefinitionId));
		if (!SkinDefinition)
		{
			UE_LOG(
				LogLobbyPreviewGrantService,
				Warning,
				TEXT("Lobby preview gesture could not be loaded. Skin=%s"),
				*GestureGrant.SkinDefinitionId.ToString());
			continue;
		}

		TArray<USkinDefinition*> SkinDefinitionsToGrant = { SkinDefinition };
		SkinComponent->AddSkinDefinitions(SkinDefinitionsToGrant);
		SkinEquipmentComponent->RequestEquipSkinDefinition(SkinDefinition, SlotTag);
	}
}

void ULobbyPreviewGrantService::GrantPandoras(APdPlayerState* PdPlayerState)
{
	if (!PdPlayerState || !PdPlayerState->HasAuthority())
	{
		return;
	}

	UPandoraComponent* PandoraComponent = PdPlayerState->GetPandoraComponent();
	UPandoraTreeComponent* PandoraTreeComponent = PdPlayerState->GetPandoraTreeComponent();
	if (!PandoraComponent || !PandoraTreeComponent)
	{
		return;
	}

	TArray<FPrimaryAssetId> PandoraDefinitionIds;
	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.GetPrimaryAssetIdList(
		FPrimaryAssetType(TEXT("PandoraDefinition")),
		PandoraDefinitionIds);
	if (PandoraDefinitionIds.IsEmpty())
	{
		return;
	}

	TWeakObjectPtr<APdPlayerState> WeakPlayerState(PdPlayerState);
	TSharedPtr<FStreamableHandle> LoadHandle = AssetManager.LoadPrimaryAssets(
		PandoraDefinitionIds,
		{},
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, WeakPlayerState, PandoraDefinitionIds]()
			{
				APdPlayerState* ResolvedPlayerState = WeakPlayerState.Get();
				UPandoraComponent* ResolvedPandoraComponent =
					ResolvedPlayerState ? ResolvedPlayerState->GetPandoraComponent() : nullptr;
				UPandoraTreeComponent* ResolvedPandoraTreeComponent =
					ResolvedPlayerState ? ResolvedPlayerState->GetPandoraTreeComponent() : nullptr;
				if (!ResolvedPlayerState
					|| !ResolvedPlayerState->HasAuthority()
					|| !ResolvedPandoraComponent
					|| !ResolvedPandoraTreeComponent)
				{
					CleanupLoadHandles();
					return;
				}

				const ULobbyPreviewDefinition* PreviewDefinition = GetPreviewDefinition();
				if (!PreviewDefinition)
				{
					CleanupLoadHandles();
					return;
				}

				TArray<FPrimaryAssetId> AllowedPandoraDefinitionIds;
				TArray<FName> OwnedPandoraNames;
				TArray<FGrantedPandora> GrantedPandoras;
				const int32 PreviewPandoraLevel = PreviewDefinition->GetPreviewPandoraLevel();
				UAssetManager& LoadedAssetManager = UAssetManager::Get();
				for (const FPrimaryAssetId& PandoraDefinitionId : PandoraDefinitionIds)
				{
					UPandoraDefinition* PandoraDefinition =
						Cast<UPandoraDefinition>(
							LoadedAssetManager.GetPrimaryAssetObject(PandoraDefinitionId));
					if (!IsValid(PandoraDefinition) || !IsPandoraAllowed(PandoraDefinition))
					{
						continue;
					}

					AllowedPandoraDefinitionIds.Add(PandoraDefinition->GetPrimaryAssetId());
					OwnedPandoraNames.AddUnique(PandoraDefinition->GetFName());
					GrantedPandoras.Add(FGrantedPandora(PandoraDefinition, PreviewPandoraLevel));
				}

				if (!AllowedPandoraDefinitionIds.IsEmpty())
				{
					ResolvedPandoraTreeComponent->SetOwnedPandoraNames(OwnedPandoraNames);
					ResolvedPandoraTreeComponent->InitializePandoraTree(GrantedPandoras, 0, false);
					ResolvedPandoraComponent->ActivatePandoras(AllowedPandoraDefinitionIds);
				}

				CleanupLoadHandles();
			}));

	if (LoadHandle.IsValid())
	{
		PendingLoadHandles.Add(LoadHandle);
	}
}

bool ULobbyPreviewGrantService::IsPandoraAllowed(
	const UPandoraDefinition* PandoraDefinition) const
{
	const ULobbyPreviewDefinition* PreviewDefinition = GetPreviewDefinition();
	return PandoraDefinition
		&& PreviewDefinition
		&& PreviewDefinition->IsPandoraKeyAllowed(PandoraDefinition->GetFName());
}

void ULobbyPreviewGrantService::CleanupLoadHandles()
{
	PendingLoadHandles.RemoveAll(
		[](const TSharedPtr<FStreamableHandle>& LoadHandle)
		{
			return !LoadHandle.IsValid() || LoadHandle->HasLoadCompleted();
		});
}

void ULobbyPreviewGrantService::CancelContentLoads()
{
	for (const TSharedPtr<FStreamableHandle>& LoadHandle : PendingLoadHandles)
	{
		if (LoadHandle.IsValid() && !LoadHandle->HasLoadCompleted())
		{
			LoadHandle->CancelHandle();
		}
	}

	PendingLoadHandles.Empty();
}

void ULobbyPreviewGrantService::ClearRetryTimer(APlayerController* PlayerController)
{
	ALobbyGameMode* GameMode = GetLobbyGameMode();
	if (!GameMode || !PlayerController)
	{
		return;
	}

	const TObjectKey<APlayerController> ControllerKey(PlayerController);
	if (FTimerHandle* RetryTimerHandle = PendingRetryTimers.Find(ControllerKey))
	{
		GameMode->GetWorldTimerManager().ClearTimer(*RetryTimerHandle);
		PendingRetryTimers.Remove(ControllerKey);
	}
}
