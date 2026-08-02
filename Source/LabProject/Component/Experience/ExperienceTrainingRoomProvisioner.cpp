#include "Component/Experience/ExperienceTrainingRoomProvisioner.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Player/StatUpgradeComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Item/ItemInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceTrainingRoomProvisioner)

void UExperienceTrainingRoomProvisioner::ApplySettings(
	const FExperiencePlayerProvisioningSettings& InSettings)
{
	bGrantAllItemsInTrainingRoom =
		InSettings.bGrantAllItemsInTrainingRoom;
	TrainingRoomItemStackGrants =
		InSettings.TrainingRoomItemStackGrants;
	DefaultGameplayItemStackGrants =
		InSettings.DefaultGameplayItemStackGrants;
	TrainingRoomItemGrantMaxAttempts = FMath::Max(
		InSettings.DefaultGameplayItemGrantMaxAttempts,
		1);
	TrainingRoomItemGrantRetryDelay = FMath::Max(
		InSettings.DefaultGameplayItemGrantRetryDelay,
		0.01f);
	bInitializeStatusPointsInTrainingRoom =
		InSettings.bInitializeStatusPointsInTrainingRoom;
	TrainingRoomStatusPointValue =
		InSettings.TrainingRoomStatusPointValue;
	bInitializeSoulDustInTrainingRoom =
		InSettings.bInitializeSoulDustInTrainingRoom;
	TrainingRoomSoulDustValue =
		InSettings.TrainingRoomSoulDustValue;
	TrainingRoomMapNames = InSettings.TrainingRoomMapNames;
	bShuttingDown = false;
}

void UExperienceTrainingRoomProvisioner::Shutdown()
{
	if (AExperienceGameMode* GameMode = GetExperienceGameMode())
	{
		if (UWorld* World = GameMode->GetWorld())
		{
			for (TPair<TObjectKey<AController>, FTimerHandle>& TimerPair
				: PendingTrainingRoomItemGrantTimers)
			{
				World->GetTimerManager().ClearTimer(TimerPair.Value);
			}
		}
	}
	PendingTrainingRoomItemGrantTimers.Reset();
	TrainingRoomItemGrantPendingPlayerStates.Reset();
	TrainingRoomStatusInitializedPlayerStates.Reset();
	TrainingRoomSoulDustInitializedPlayerStates.Reset();
	bShuttingDown = true;
}

void UExperienceTrainingRoomProvisioner::
ClearRuntimeStateForController(
	AController* Controller,
	APlayerState* PlayerState)
{
	if (Controller)
	{
		const TObjectKey<AController> ControllerKey(Controller);
		if (FTimerHandle* TimerHandle =
			PendingTrainingRoomItemGrantTimers.Find(ControllerKey))
		{
			if (AExperienceGameMode* GameMode = GetExperienceGameMode())
			{
				if (UWorld* World = GameMode->GetWorld())
				{
					World->GetTimerManager().ClearTimer(*TimerHandle);
				}
			}
			PendingTrainingRoomItemGrantTimers.Remove(ControllerKey);
		}
	}

	if (!PlayerState)
	{
		return;
	}

	TrainingRoomItemGrantPendingPlayerStates.Remove(PlayerState);
	TrainingRoomStatusInitializedPlayerStates.Remove(PlayerState);
	TrainingRoomSoulDustInitializedPlayerStates.Remove(PlayerState);
}

bool UExperienceTrainingRoomProvisioner::IsTrainingRoomMap() const
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode)
	{
		return false;
	}
	const UWorld* World = GameMode->GetWorld();
	if (!World)
	{
		return false;
	}

	const FString CurrentLevelName =
		UGameplayStatics::GetCurrentLevelName(World, true);
	for (const FName TrainingRoomMapName : TrainingRoomMapNames)
	{
		if (!TrainingRoomMapName.IsNone()
			&& CurrentLevelName.Equals(
				TrainingRoomMapName.ToString(),
				ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

void UExperienceTrainingRoomProvisioner::PreparePlayerForGameplay(
	APlayerController* NewPlayer)
{
	if (bShuttingDown || !NewPlayer)
	{
		return;
	}

	GrantAllItemsForTrainingRoom(NewPlayer, INDEX_NONE);
	ScheduleTrainingRoomStatusPoints(NewPlayer);
}

int32 UExperienceTrainingRoomProvisioner::
GetInventoryItemQuantityByPrimaryAssetId(
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
		const UItemDefinition* ItemDefinition =
			IsValid(ItemInstance)
				? ItemInstance->ItemDefinition.Get()
				: nullptr;
		if (ItemDefinition
			&& ItemDefinition->GetPrimaryAssetId()
				== ItemDefinitionId)
		{
			TotalQuantity += FMath::Max(ItemInstance->Quantity, 0);
		}
	}

	return TotalQuantity;
}

void UExperienceTrainingRoomProvisioner::GrantAllItemsForTrainingRoom(
	APlayerController* NewPlayer,
	const int32 RemainingAttempts)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (bShuttingDown || !GameMode || !GameMode->HasAuthority()
		|| !NewPlayer || !bGrantAllItemsInTrainingRoom
		|| !IsTrainingRoomMap())
	{
		return;
	}
	const int32 AttemptsLeft = RemainingAttempts == INDEX_NONE
		? FMath::Max(TrainingRoomItemGrantMaxAttempts, 1)
		: RemainingAttempts;
	const TObjectKey<AController> ControllerKey(NewPlayer);
	if (AttemptsLeft <= 0)
	{
		if (APdPlayerState* PlayerState =
			NewPlayer->GetPlayerState<APdPlayerState>())
		{
			TrainingRoomItemGrantPendingPlayerStates.Remove(PlayerState);
		}
		PendingTrainingRoomItemGrantTimers.Remove(ControllerKey);
		UE_LOG(
			PdExperienceGameModeLog,
			Error,
			TEXT("Training-room items were not granted after %d attempts. Player=%s"),
			TrainingRoomItemGrantMaxAttempts,
			*GetNameSafe(NewPlayer));
		return;
	}

	APdPlayerState* PdPlayerState =
		NewPlayer ? NewPlayer->GetPlayerState<APdPlayerState>() : nullptr;
	UInventoryComponent* InventoryComponent =
		PdPlayerState
			? PdPlayerState->GetInventoryComponent()
			: nullptr;
	if (!InventoryComponent)
	{
		ScheduleTrainingRoomItems(NewPlayer, AttemptsLeft - 1);
		return;
	}

	// Do not issue the same grants while a previous batch still has either a
	// live streamable handle or a queued no-op completion delegate.
	bool bWasWaitingForItemLoads = false;
	if (TrainingRoomItemGrantPendingPlayerStates.Contains(PdPlayerState))
	{
		if (InventoryComponent->HasPendingItemLoads())
		{
			ScheduleTrainingRoomItems(NewPlayer, AttemptsLeft - 1);
			return;
		}
		bWasWaitingForItemLoads = true;
		TrainingRoomItemGrantPendingPlayerStates.Remove(PdPlayerState);
	}

	TSet<FPrimaryAssetId> ExistingItemDefinitionIds;
	for (const UItemInstance* ItemInstance
		: InventoryComponent->GetAllItems().Items)
	{
		const UItemDefinition* ItemDefinition =
			IsValid(ItemInstance)
				? ItemInstance->ItemDefinition.Get()
				: nullptr;
		if (ItemDefinition)
		{
			ExistingItemDefinitionIds.Add(
				ItemDefinition->GetPrimaryAssetId());
		}
	}

	TArray<FPrimaryAssetId> AllItemDefinitionIds;
	UAssetManager::Get().GetPrimaryAssetIdList(
		FPrimaryAssetType(TEXT("ItemDefinition")),
		AllItemDefinitionIds);
	if (AllItemDefinitionIds.IsEmpty())
	{
		ScheduleTrainingRoomItems(NewPlayer, AttemptsLeft - 1);
		return;
	}

	TSet<FPrimaryAssetId> TrainingStackGrantItemDefinitionIds;
	for (const FTrainingRoomItemStackGrant& StackGrant
		: TrainingRoomItemStackGrants)
	{
		if (StackGrant.ItemDefinitionId.IsValid()
			&& StackGrant.Quantity > 0)
		{
			TrainingStackGrantItemDefinitionIds.Add(
				StackGrant.ItemDefinitionId);
		}
	}

	TArray<FPrimaryAssetId> MissingItemDefinitionIds;
	for (const FPrimaryAssetId& ItemDefinitionId
		: AllItemDefinitionIds)
	{
		if (ItemDefinitionId.IsValid()
			&& !TrainingStackGrantItemDefinitionIds.Contains(
				ItemDefinitionId)
			&& !ExistingItemDefinitionIds.Contains(ItemDefinitionId))
		{
			MissingItemDefinitionIds.Add(ItemDefinitionId);
		}
	}

	bool bIssuedQuantityOrQuickSlotRequest = false;
	for (const FTrainingRoomItemStackGrant& StackGrant
		: TrainingRoomItemStackGrants)
	{
		if (!StackGrant.ItemDefinitionId.IsValid()
			|| StackGrant.Quantity <= 0)
		{
			continue;
		}

		const int32 CurrentQuantity =
			GetInventoryItemQuantityByPrimaryAssetId(
				InventoryComponent,
				StackGrant.ItemDefinitionId);
		const int32 MissingQuantity =
			FMath::Max(StackGrant.Quantity - CurrentQuantity, 0);

		const FGameplayItemStackGrant* DefaultQuickSlotGrant =
			DefaultGameplayItemStackGrants.FindByPredicate(
				[&StackGrant](
					const FGameplayItemStackGrant& GameplayGrant)
				{
					return GameplayGrant.ItemDefinitionId
							== StackGrant.ItemDefinitionId
						&& GameplayGrant.QuickSlotIndex >= 0
						&& GameplayGrant.QuickSlotIndex
							< UInventoryComponent::
								ConsumableQuickSlotCount;
				});
		if (DefaultQuickSlotGrant)
		{
			const UItemInstance* QuickSlotItem =
				InventoryComponent->GetConsumableQuickSlotItem(
					DefaultQuickSlotGrant->QuickSlotIndex);
			const UItemDefinition* QuickSlotDefinition =
				IsValid(QuickSlotItem)
					? QuickSlotItem->ItemDefinition.Get()
					: nullptr;
			const bool bQuickSlotMatches = QuickSlotDefinition
				&& QuickSlotDefinition->GetPrimaryAssetId()
					== StackGrant.ItemDefinitionId;
			if (MissingQuantity > 0 || !bQuickSlotMatches)
			{
				InventoryComponent
					->SetConsumableItemQuantityAndQuickSlotByPrimaryAssetId(
						StackGrant.ItemDefinitionId,
						FMath::Max(
							CurrentQuantity,
							StackGrant.Quantity),
						DefaultQuickSlotGrant->QuickSlotIndex);
				bIssuedQuantityOrQuickSlotRequest = true;
			}
			continue;
		}

		for (int32 Count = 0; Count < MissingQuantity; ++Count)
		{
			MissingItemDefinitionIds.Add(
				StackGrant.ItemDefinitionId);
		}
	}

	if (!MissingItemDefinitionIds.IsEmpty())
	{
		InventoryComponent->AddItemsByPrimaryAssetIds(
			MissingItemDefinitionIds);
	}

	const bool bIssuedItemRequest =
		bIssuedQuantityOrQuickSlotRequest
		|| !MissingItemDefinitionIds.IsEmpty();
	if (!bIssuedItemRequest)
	{
		TrainingRoomItemGrantPendingPlayerStates.Remove(PdPlayerState);
		if (FTimerHandle* ExistingTimer =
			PendingTrainingRoomItemGrantTimers.Find(ControllerKey))
		{
			if (UWorld* World = GameMode->GetWorld())
			{
				World->GetTimerManager().ClearTimer(*ExistingTimer);
			}
			PendingTrainingRoomItemGrantTimers.Remove(ControllerKey);
		}
		if (bWasWaitingForItemLoads)
		{
			UE_LOG(
				PdExperienceGameModeLog,
				Display,
				TEXT("Training-room inventory grant completed. Player=%s Items=%d"),
				*GetNameSafe(NewPlayer),
				InventoryComponent->GetAllItems().Items.Num());
		}
		return;
	}

	TrainingRoomItemGrantPendingPlayerStates.Add(PdPlayerState);
	ScheduleTrainingRoomItems(NewPlayer, AttemptsLeft - 1);
}

void UExperienceTrainingRoomProvisioner::ScheduleTrainingRoomItems(
	APlayerController* NewPlayer,
	const int32 RemainingAttempts)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (bShuttingDown || !GameMode || !GameMode->HasAuthority()
		|| !NewPlayer || !IsTrainingRoomMap())
	{
		return;
	}

	UWorld* World = GameMode->GetWorld();
	if (!World)
	{
		return;
	}

	const TObjectKey<AController> ControllerKey(NewPlayer);
	if (const FTimerHandle* ExistingTimer =
		PendingTrainingRoomItemGrantTimers.Find(ControllerKey);
		ExistingTimer
		&& World->GetTimerManager().IsTimerActive(*ExistingTimer))
	{
		return;
	}

	TWeakObjectPtr<APlayerController> WeakPlayer(NewPlayer);
	FTimerHandle RetryTimerHandle;
	World->GetTimerManager().SetTimer(
		RetryTimerHandle,
		FTimerDelegate::CreateWeakLambda(
			this,
			[this, WeakPlayer, ControllerKey, RemainingAttempts]()
			{
				PendingTrainingRoomItemGrantTimers.Remove(ControllerKey);
				if (APlayerController* PlayerController = WeakPlayer.Get())
				{
					GrantAllItemsForTrainingRoom(
						PlayerController,
						RemainingAttempts);
				}
			}),
		FMath::Max(TrainingRoomItemGrantRetryDelay, 0.01f),
		false);
	PendingTrainingRoomItemGrantTimers.Add(
		ControllerKey,
		RetryTimerHandle);
}

void UExperienceTrainingRoomProvisioner::
InitializeTrainingRoomStatusPoints(APlayerController* NewPlayer)
{
	APlayerState* PlayerState =
		NewPlayer ? NewPlayer->PlayerState : nullptr;
	GrantTrainingRoomStatusPointsForPlayerState(PlayerState);
	InitializeTrainingRoomSoulDust(PlayerState);
}

void UExperienceTrainingRoomProvisioner::
ScheduleTrainingRoomStatusPoints(
	APlayerController* NewPlayer,
	const int32 RemainingAttempts)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (bShuttingDown || !GameMode || !GameMode->HasAuthority()
		|| !IsTrainingRoomMap() || !NewPlayer)
	{
		return;
	}

	if (!bInitializeStatusPointsInTrainingRoom
		&& !bInitializeSoulDustInTrainingRoom)
	{
		return;
	}

	UWorld* World = GameMode->GetWorld();
	if (!World || RemainingAttempts <= 0)
	{
		InitializeTrainingRoomStatusPoints(NewPlayer);
		return;
	}

	TWeakObjectPtr<APlayerController> WeakPlayer(NewPlayer);
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateWeakLambda(
			this,
			[this, WeakPlayer, RemainingAttempts]()
			{
				if (bShuttingDown)
				{
					return;
				}

				APlayerController* PlayerController = WeakPlayer.Get();
				AExperienceGameMode* CurrentGameMode =
					GetExperienceGameMode();
				if (!CurrentGameMode || !PlayerController)
				{
					return;
				}

				InitializeTrainingRoomStatusPoints(PlayerController);

				const APdPlayerState* PdPlayerState =
					PlayerController->GetPlayerState<APdPlayerState>();
				const bool bStatusInitialized =
					!bInitializeStatusPointsInTrainingRoom
					|| (PdPlayerState
						&& TrainingRoomStatusInitializedPlayerStates
							.Contains(PdPlayerState));
				const bool bSoulDustInitialized =
					!bInitializeSoulDustInTrainingRoom
					|| (PdPlayerState
						&& TrainingRoomSoulDustInitializedPlayerStates
							.Contains(PdPlayerState));
				if (!bStatusInitialized || !bSoulDustInitialized)
				{
					ScheduleTrainingRoomStatusPoints(
						PlayerController,
						RemainingAttempts - 1);
				}
			}));
}

void UExperienceTrainingRoomProvisioner::InitializeTrainingRoomSoulDust(
	APlayerState* PlayerState)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (bShuttingDown || !GameMode || !GameMode->HasAuthority()
		|| !bInitializeSoulDustInTrainingRoom
		|| !IsTrainingRoomMap())
	{
		return;
	}

	APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
	UPandoraTreeComponent* PandoraTreeComponent =
		PdPlayerState
			? PdPlayerState->GetPandoraTreeComponent()
			: nullptr;
	if (!PandoraTreeComponent)
	{
		return;
	}

	const int32 SoulDustValue =
		FMath::Max(TrainingRoomSoulDustValue, 0);
	const bool bAlreadyInitialized =
		TrainingRoomSoulDustInitializedPlayerStates.Contains(
			PdPlayerState);
	if (bAlreadyInitialized
		&& PandoraTreeComponent->GetSoulDust() == SoulDustValue)
	{
		return;
	}

	PandoraTreeComponent->SetSoulDust(SoulDustValue);
	TrainingRoomSoulDustInitializedPlayerStates.Add(PdPlayerState);
	PdPlayerState->ForceNetUpdate();
}

void UExperienceTrainingRoomProvisioner::
GrantTrainingRoomStatusPointsForPlayerState(APlayerState* PlayerState)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (bShuttingDown || !GameMode || !GameMode->HasAuthority()
		|| !bInitializeStatusPointsInTrainingRoom
		|| !IsTrainingRoomMap())
	{
		return;
	}

	APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
	UPdAbilitySystemComponent* ASC =
		PdPlayerState
			? PdPlayerState->GetPdAbilitySystemComponent()
			: nullptr;
	UStatUpgradeComponent* StatUpgradeComponent =
		PdPlayerState
			? PdPlayerState->GetStatUpgradeComponent()
			: nullptr;
	if (!ASC || !StatUpgradeComponent)
	{
		return;
	}

	const float PointValue =
		FMath::Max(TrainingRoomStatusPointValue, 0.0f);
	const bool bAlreadyGranted =
		TrainingRoomStatusInitializedPlayerStates.Contains(
			PdPlayerState);
	const bool bHasExpectedTrainingPoints =
		ASC->GetNumericAttribute(
			UBasicAttributeSet::GetOffensePointAttribute())
				>= PointValue - UE_KINDA_SMALL_NUMBER
		&& ASC->GetNumericAttribute(
			UBasicAttributeSet::GetDefensePointAttribute())
				>= PointValue - UE_KINDA_SMALL_NUMBER
		&& ASC->GetNumericAttribute(
			UBasicAttributeSet::GetResistancePointAttribute())
				>= PointValue - UE_KINDA_SMALL_NUMBER
		&& ASC->GetNumericAttribute(
			UBasicAttributeSet::GetPandoraForcePointAttribute())
				>= PointValue - UE_KINDA_SMALL_NUMBER
		&& ASC->GetNumericAttribute(
			UBasicAttributeSet::GetResourcePointAttribute())
				>= PointValue - UE_KINDA_SMALL_NUMBER
		&& ASC->GetNumericAttribute(
			UBasicAttributeSet::GetAgilityPointAttribute())
				>= PointValue - UE_KINDA_SMALL_NUMBER;
	if (bAlreadyGranted && bHasExpectedTrainingPoints)
	{
		return;
	}

	if (StatUpgradeComponent->GrantPointsToAllCategories(PointValue))
	{
		TrainingRoomStatusInitializedPlayerStates.Add(PdPlayerState);
	}
}

AExperienceGameMode*
UExperienceTrainingRoomProvisioner::GetExperienceGameMode() const
{
	const UExperiencePlayerProvisioningComponent* Coordinator =
		GetTypedOuter<UExperiencePlayerProvisioningComponent>();
	return Coordinator
		? Cast<AExperienceGameMode>(Coordinator->GetOwner())
		: nullptr;
}
