#include "Component/Experience/ExperienceGameplayLoadoutProvisioner.h"

#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraLoadoutTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceGameplayLoadoutProvisioner)

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

void UExperienceGameplayLoadoutProvisioner::ApplySettings(
	const FExperiencePlayerProvisioningSettings& InSettings)
{
	CancelGestureLoads();
	DefaultGameplayItemStackGrants =
		InSettings.DefaultGameplayItemStackGrants;
	DefaultGameplayItemGrantMaxAttempts =
		InSettings.DefaultGameplayItemGrantMaxAttempts;
	DefaultGameplayItemGrantRetryDelay =
		InSettings.DefaultGameplayItemGrantRetryDelay;
	DefaultGameplayGestureSlotGrants =
		InSettings.DefaultGameplayGestureSlotGrants;
	bShuttingDown = false;
}

void UExperienceGameplayLoadoutProvisioner::Shutdown()
{
	if (AExperienceGameMode* GameMode = GetExperienceGameMode())
	{
		if (UWorld* World = GameMode->GetWorld())
		{
			for (TPair<TObjectKey<AController>, FTimerHandle>& PendingTimer
				: PendingDefaultGameplayItemGrantTimers)
			{
				World->GetTimerManager().ClearTimer(PendingTimer.Value);
			}
		}
	}

	PendingDefaultGameplayItemGrantTimers.Reset();
	DefaultGameplayInitializedInventoriesByPlayerState.Reset();
	CancelGestureLoads();
	bShuttingDown = true;
}

void UExperienceGameplayLoadoutProvisioner::
ClearRuntimeStateForController(
	AController* Controller,
	APlayerState* PlayerState)
{
	if (!Controller)
	{
		return;
	}

	const TObjectKey<AController> ControllerKey(Controller);
	if (AExperienceGameMode* GameMode = GetExperienceGameMode())
	{
		if (UWorld* World = GameMode->GetWorld())
		{
			if (FTimerHandle* TimerHandle =
				PendingDefaultGameplayItemGrantTimers.Find(
					ControllerKey))
			{
				World->GetTimerManager().ClearTimer(*TimerHandle);
			}
		}
	}
	PendingDefaultGameplayItemGrantTimers.Remove(ControllerKey);
	PendingDefaultGameplayGestureControllers.Remove(ControllerKey);

	if (PlayerState)
	{
		DefaultGameplayInitializedInventoriesByPlayerState.Remove(
			TObjectKey<APlayerState>(PlayerState));
	}
}

void UExperienceGameplayLoadoutProvisioner::PrepareGameplayLoadout(
	APlayerController* NewPlayer,
	const bool bIsTrainingRoom)
{
	if (bShuttingDown || !NewPlayer)
	{
		return;
	}

	ClearLobbyPreviewContentForGameplay(NewPlayer, bIsTrainingRoom);
	ScheduleDefaultGameplayItems(NewPlayer, bIsTrainingRoom);
}

void UExperienceGameplayLoadoutProvisioner::
ClearLobbyPreviewContentForGameplay(
	APlayerController* NewPlayer,
	const bool bIsTrainingRoom)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !GameMode->HasAuthority()
		|| bIsTrainingRoom || !NewPlayer)
	{
		return;
	}

	APdPlayerState* PdPlayerState =
		NewPlayer->GetPlayerState<APdPlayerState>();
	if (!PdPlayerState)
	{
		return;
	}

	if (UPandoraComponent* PandoraComponent =
		PdPlayerState->GetPandoraComponent())
	{
		PandoraComponent->ClearAllPandoras();
	}

	if (PdPlayerState->GetPandoraTreeComponent())
	{
		GrantDefaultGameplayPandoras(NewPlayer);
	}
}

void UExperienceGameplayLoadoutProvisioner::GrantDefaultGameplayPandoras(
	APlayerController* NewPlayer)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !GameMode->HasAuthority() || !NewPlayer)
	{
		return;
	}

	APdPlayerState* PdPlayerState =
		NewPlayer->GetPlayerState<APdPlayerState>();
	UPandoraTreeComponent* PandoraTreeComponent =
		PdPlayerState
			? PdPlayerState->GetPandoraTreeComponent()
			: nullptr;
	if (!PandoraTreeComponent)
	{
		return;
	}

	UPdGameInstance* PdGameInstance =
		GameMode->GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{
		PandoraTreeComponent->SetOwnedPandoraNames(TArray<FName>());
		PandoraTreeComponent->InitializePandoraTree(
			TArray<FGrantedPandora>(),
			0,
			false);
		return;
	}

	TArray<FName> DefaultOwnedPandoraNames;
	TArray<FPrimaryAssetId> DefaultUnlockedPandoraIds;
	PdGameInstance->BuildDefaultUnlockedPandoras(
		DefaultOwnedPandoraNames,
		&DefaultUnlockedPandoraIds);
	PandoraTreeComponent->SetOwnedPandoraNames(
		DefaultOwnedPandoraNames);
	PandoraTreeComponent->InitializePandoraTree(
		TArray<FGrantedPandora>(),
		0,
		false);

	if (UPandoraComponent* PandoraComponent =
		PdPlayerState->GetPandoraComponent())
	{
		TMap<EEnum_Direction, FPrimaryAssetId>
			PandoraLoadoutByDirection;
		TMap<EEnum_Direction, FName>
			CachedPandoraNamesByDirection;
		if (PdGameInstance->TryGetCachedLobbyPandoraLoadoutForPlayerState(
			PdPlayerState,
			CachedPandoraNamesByDirection))
		{
			for (const TPair<EEnum_Direction, FName>& LoadoutPair
				: CachedPandoraNamesByDirection)
			{
				if (!PandoraLoadout::IsLoadoutDirection(LoadoutPair.Key)
					|| LoadoutPair.Value.IsNone())
				{
					continue;
				}

				if (const UPandoraDefinition* PandoraDefinition =
					PdGameInstance->GetPandoraDefinitionByName(
						LoadoutPair.Value))
				{
					PandoraLoadoutByDirection.Add(
						LoadoutPair.Key,
						PandoraDefinition->GetPrimaryAssetId());
				}
			}
		}

		PandoraComponent->ActivatePandorasWithLoadout(
			DefaultUnlockedPandoraIds,
			PandoraLoadoutByDirection);
	}
}

bool UExperienceGameplayLoadoutProvisioner::GrantDefaultGameplayItems(
	APlayerController* NewPlayer,
	const bool bIsTrainingRoom)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (bShuttingDown || !GameMode || !GameMode->HasAuthority()
		|| !NewPlayer)
	{
		return false;
	}

	if (bIsTrainingRoom)
	{
		return true;
	}

	APdPlayerState* PdPlayerState =
		NewPlayer->GetPlayerState<APdPlayerState>();
	UInventoryComponent* InventoryComponent =
		PdPlayerState
			? PdPlayerState->GetInventoryComponent()
			: nullptr;
	if (!PdPlayerState || !InventoryComponent)
	{
		return false;
	}

	const TObjectKey<APlayerState> PlayerStateKey(PdPlayerState);
	if (const TWeakObjectPtr<UInventoryComponent>* InitializedInventory =
		DefaultGameplayInitializedInventoriesByPlayerState.Find(
			PlayerStateKey);
		InitializedInventory
		&& InitializedInventory->Get() == InventoryComponent)
	{
		return true;
	}

	// Seamless travel can replace the lobby inventory after PostLogin, so the
	// concrete inventory component (not only PlayerState) is the idempotency key.
	DefaultGameplayInitializedInventoriesByPlayerState.Add(
		PlayerStateKey,
		InventoryComponent);
	InventoryComponent->ClearAllItems();

	for (const FGameplayItemStackGrant& StackGrant
		: DefaultGameplayItemStackGrants)
	{
		if (!StackGrant.ItemDefinitionId.IsValid()
			|| StackGrant.Quantity <= 0)
		{
			continue;
		}

		if (StackGrant.QuickSlotIndex >= 0
			&& StackGrant.QuickSlotIndex
				< UInventoryComponent::ConsumableQuickSlotCount)
		{
			InventoryComponent
				->SetConsumableItemQuantityAndQuickSlotByPrimaryAssetId(
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

	return true;
}

void UExperienceGameplayLoadoutProvisioner::ScheduleDefaultGameplayItems(
	APlayerController* NewPlayer,
	const bool bIsTrainingRoom,
	const int32 RemainingAttempts)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (bShuttingDown || !GameMode || !GameMode->HasAuthority()
		|| bIsTrainingRoom || !NewPlayer)
	{
		return;
	}

	const TObjectKey<AController> ControllerKey(NewPlayer);
	UWorld* World = GameMode->GetWorld();
	bool bCurrentInventoryWasAlreadyInitialized = false;
	if (APdPlayerState* PdPlayerState =
		NewPlayer->GetPlayerState<APdPlayerState>())
	{
		if (UInventoryComponent* InventoryComponent =
			PdPlayerState->GetInventoryComponent())
		{
			const TWeakObjectPtr<UInventoryComponent>* InitializedInventory =
				DefaultGameplayInitializedInventoriesByPlayerState.Find(
					TObjectKey<APlayerState>(PdPlayerState));
			bCurrentInventoryWasAlreadyInitialized =
				InitializedInventory
				&& InitializedInventory->Get() == InventoryComponent;
		}
	}

	if (GrantDefaultGameplayItems(NewPlayer, bIsTrainingRoom))
	{
		if (bCurrentInventoryWasAlreadyInitialized)
		{
			const FTimerHandle* ExistingTimerHandle =
				PendingDefaultGameplayItemGrantTimers.Find(
					ControllerKey);
			if (!World || !ExistingTimerHandle
				|| !World->GetTimerManager().IsTimerActive(
					*ExistingTimerHandle))
			{
				PendingDefaultGameplayItemGrantTimers.Remove(
					ControllerKey);
			}
			return;
		}

		if (World)
		{
			if (FTimerHandle* ExistingTimerHandle =
				PendingDefaultGameplayItemGrantTimers.Find(
					ControllerKey))
			{
				World->GetTimerManager().ClearTimer(
					*ExistingTimerHandle);
			}
		}
		PendingDefaultGameplayItemGrantTimers.Remove(ControllerKey);

		// Recheck after the first grant because seamless travel can swap the
		// lobby inventory component after the first gameplay callback.
		if (World)
		{
			TWeakObjectPtr<APlayerController> WeakPlayer(NewPlayer);
			FTimerHandle ValidationTimerHandle;
			const float RetryDelay =
				FMath::Max(
					DefaultGameplayItemGrantRetryDelay,
					0.01f);
			World->GetTimerManager().SetTimer(
				ValidationTimerHandle,
				FTimerDelegate::CreateWeakLambda(
					this,
					[this, WeakPlayer, ControllerKey, bIsTrainingRoom]()
					{
						PendingDefaultGameplayItemGrantTimers.Remove(
							ControllerKey);
						if (APlayerController* PlayerController =
							WeakPlayer.Get())
						{
							const UExperiencePlayerProvisioningComponent*
								Coordinator = GetTypedOuter<
									UExperiencePlayerProvisioningComponent>();
							ScheduleDefaultGameplayItems(
								PlayerController,
								Coordinator
									? Coordinator->IsTrainingRoomMap()
									: bIsTrainingRoom);
						}
					}),
				RetryDelay,
				false);
			PendingDefaultGameplayItemGrantTimers.Add(
				ControllerKey,
				ValidationTimerHandle);
		}
		return;
	}

	const int32 AttemptsLeft = RemainingAttempts == INDEX_NONE
		? FMath::Max(
			DefaultGameplayItemGrantMaxAttempts,
			1)
		: RemainingAttempts;
	if (!World || AttemptsLeft <= 0)
	{
		UE_LOG(
			PdExperienceGameModeLog,
			Warning,
			TEXT("Default gameplay items were not granted because the "
				"inventory component did not become ready. Player=%s"),
			*GetNameSafe(NewPlayer));
		PendingDefaultGameplayItemGrantTimers.Remove(ControllerKey);
		return;
	}

	if (const FTimerHandle* ExistingTimerHandle =
		PendingDefaultGameplayItemGrantTimers.Find(ControllerKey);
		ExistingTimerHandle
		&& World->GetTimerManager().IsTimerActive(*ExistingTimerHandle))
	{
		return;
	}

	TWeakObjectPtr<APlayerController> WeakPlayer(NewPlayer);
	FTimerHandle RetryTimerHandle;
	const float RetryDelay =
		FMath::Max(
			DefaultGameplayItemGrantRetryDelay,
			0.01f);
	World->GetTimerManager().SetTimer(
		RetryTimerHandle,
		FTimerDelegate::CreateWeakLambda(
			this,
			[this, WeakPlayer, ControllerKey, bIsTrainingRoom, AttemptsLeft]()
			{
				PendingDefaultGameplayItemGrantTimers.Remove(
					ControllerKey);
				if (APlayerController* PlayerController =
					WeakPlayer.Get())
				{
					const UExperiencePlayerProvisioningComponent*
						Coordinator = GetTypedOuter<
							UExperiencePlayerProvisioningComponent>();
					ScheduleDefaultGameplayItems(
						PlayerController,
						Coordinator
							? Coordinator->IsTrainingRoomMap()
							: bIsTrainingRoom,
						AttemptsLeft - 1);
				}
			}),
		RetryDelay,
		false);
	PendingDefaultGameplayItemGrantTimers.Add(
		ControllerKey,
		RetryTimerHandle);
}

void UExperienceGameplayLoadoutProvisioner::GrantDefaultGameplayGestures(
	APlayerController* NewPlayer)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (bShuttingDown || !GameMode || !GameMode->HasAuthority()
		|| !NewPlayer || DefaultGameplayGestureSlotGrants.IsEmpty())
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetId> GestureDefinitionIds;
	for (const FGameplayGestureSlotGrant& GestureGrant
		: DefaultGameplayGestureSlotGrants)
	{
		const FGameplayTag SlotTag =
			ResolveGestureSlotTag(GestureGrant.GestureSlotIndex);
		if (!GestureGrant.SkinDefinitionId.IsValid()
			|| !SlotTag.IsValid())
		{
			continue;
		}
		GestureDefinitionIds.AddUnique(GestureGrant.SkinDefinitionId);
	}

	if (GestureDefinitionIds.IsEmpty())
	{
		return;
	}

	const bool bAllGestureDefinitionsLoaded = !GestureDefinitionIds.ContainsByPredicate(
		[&AssetManager](const FPrimaryAssetId& GestureDefinitionId)
		{
			return !AssetManager.GetPrimaryAssetObject(GestureDefinitionId);
		});
	if (bAllGestureDefinitionsLoaded)
	{
		ApplyLoadedDefaultGameplayGestures(NewPlayer);
		return;
	}

	const TObjectKey<AController> ControllerKey(NewPlayer);
	if (PendingDefaultGameplayGestureControllers.Contains(ControllerKey))
	{
		return;
	}
	PendingDefaultGameplayGestureControllers.Add(ControllerKey);

	const TWeakObjectPtr<APlayerController> WeakPlayerController(NewPlayer);
	TSharedPtr<FStreamableHandle> LoadHandle = AssetManager.LoadPrimaryAssets(
		GestureDefinitionIds,
		{},
		FStreamableDelegate::CreateWeakLambda(
			this,
			[this, WeakPlayerController, ControllerKey]()
			{
				if (PendingDefaultGameplayGestureControllers.Remove(ControllerKey) > 0
					&& !bShuttingDown)
				{
					ApplyLoadedDefaultGameplayGestures(WeakPlayerController.Get());
				}
				CleanupGestureLoadHandles();
			}));

	if (LoadHandle.IsValid())
	{
		PendingGestureLoadHandles.Add(LoadHandle);
	}
	else if (PendingDefaultGameplayGestureControllers.Remove(ControllerKey) > 0)
	{
		UE_LOG(
			PdExperienceGameModeLog,
			Error,
			TEXT("Default gameplay gesture asynchronous load could not be started."));
	}
}

void UExperienceGameplayLoadoutProvisioner::ApplyLoadedDefaultGameplayGestures(
	APlayerController* NewPlayer)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (bShuttingDown || !GameMode || !GameMode->HasAuthority() || !NewPlayer)
	{
		return;
	}

	APdPlayerState* PdPlayerState = NewPlayer->GetPlayerState<APdPlayerState>();
	ACharacterBase* PlayerCharacter = Cast<ACharacterBase>(NewPlayer->GetPawn());
	USkinComponent* SkinComponent = PdPlayerState ? PdPlayerState->GetSkinComponent() : nullptr;
	USkinEquipmentComponent* SkinEquipmentComponent =
		PlayerCharacter ? PlayerCharacter->GetSkinEquipmentComponent() : nullptr;
	if (!SkinComponent || !SkinEquipmentComponent)
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FGameplayGestureSlotGrant& GestureGrant : DefaultGameplayGestureSlotGrants)
	{
		const FGameplayTag SlotTag = ResolveGestureSlotTag(GestureGrant.GestureSlotIndex);
		if (!GestureGrant.SkinDefinitionId.IsValid() || !SlotTag.IsValid())
		{
			continue;
		}

		USkinDefinition* SkinDefinition =
			Cast<USkinDefinition>(
				AssetManager.GetPrimaryAssetObject(
					GestureGrant.SkinDefinitionId));
		if (!SkinDefinition)
		{
			UE_LOG(
				PdExperienceGameModeLog,
				Warning,
				TEXT("Default gameplay gesture could not be loaded. "
					"Skin=%s"),
				*GestureGrant.SkinDefinitionId.ToString());
			continue;
		}

		TArray<USkinDefinition*> SkinDefinitionsToGrant =
			{ SkinDefinition };
		SkinComponent->AddSkinDefinitions(SkinDefinitionsToGrant);
		SkinEquipmentComponent->RequestEquipSkinDefinition(
			SkinDefinition,
			SlotTag);
	}
}

void UExperienceGameplayLoadoutProvisioner::CleanupGestureLoadHandles()
{
	PendingGestureLoadHandles.RemoveAll(
		[](const TSharedPtr<FStreamableHandle>& LoadHandle)
		{
			return !LoadHandle.IsValid() || LoadHandle->HasLoadCompleted();
		});
}

void UExperienceGameplayLoadoutProvisioner::CancelGestureLoads()
{
	for (const TSharedPtr<FStreamableHandle>& LoadHandle : PendingGestureLoadHandles)
	{
		if (LoadHandle.IsValid())
		{
			LoadHandle->CancelHandle();
			LoadHandle->ReleaseHandle();
		}
	}
	PendingGestureLoadHandles.Reset();
	PendingDefaultGameplayGestureControllers.Reset();
}

AExperienceGameMode*
UExperienceGameplayLoadoutProvisioner::GetExperienceGameMode() const
{
	const UExperiencePlayerProvisioningComponent* Coordinator =
		GetTypedOuter<UExperiencePlayerProvisioningComponent>();
	return Coordinator
		? Cast<AExperienceGameMode>(Coordinator->GetOwner())
		: nullptr;
}
