#include "Component/Player/ControllerProfileSyncComponent.h"

#include "Component/Skin/SkinComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/World.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "SavedGameData/PdSaveGame.h"
#include "Skin/SkinDefaultUnlockPolicy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerProfileSyncComponent)

namespace
{
	const FPrimaryAssetType SkinDefinitionAssetType(TEXT("SkinDefinition"));
	constexpr float LocalCosmeticProfileSyncInterval = 0.50f;
	constexpr int32 LocalCosmeticProfileSyncMaxAttempts = 5;
	constexpr int32 MaxClientSyncedSkinNameCount = 512;
	constexpr double RemoteSkinSyncMinInterval = 0.20;
}

UControllerProfileSyncComponent::UControllerProfileSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UControllerProfileSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LocalCosmeticProfileSyncTimerHandle);
	}
	LocalCosmeticProfileSyncAttemptCount = 0;
	LastRemoteSkinSyncRequestTime = -1.0;
	Super::EndPlay(EndPlayReason);
}

void UControllerProfileSyncComponent::ScheduleLocalCosmeticProfileSync()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller
		|| (!Controller->HasAuthority() && !Controller->IsLocalController()))
	{
		return;
	}

	LocalCosmeticProfileSyncAttemptCount = 0;
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(LocalCosmeticProfileSyncTimerHandle);
		TimerManager.SetTimerForNextTick(
			this,
			&ThisClass::PushLocalCosmeticProfileToServer);
		TimerManager.SetTimer(
			LocalCosmeticProfileSyncTimerHandle,
			this,
			&ThisClass::PushLocalCosmeticProfileToServer,
			LocalCosmeticProfileSyncInterval,
			true);
	}
}

void UControllerProfileSyncComponent::ApplyGameVictoryGoldReward(
	const FString& PlayerId,
	const int32 GoldReward) const
{
	if (GoldReward <= 0)
	{
		return;
	}

	APdPlayerController* Controller = GetPdController();
	UPdGameInstance* PdGameInstance =
		Controller ? Controller->GetGameInstance<UPdGameInstance>() : nullptr;
	if (!PdGameInstance)
	{
		return;
	}

	const FString RewardPlayerId = ResolveRewardPlayerId(PlayerId);
	if (RewardPlayerId.IsEmpty())
	{
		return;
	}

	PdGameInstance->AddGold(RewardPlayerId, GoldReward, false);
	PdGameInstance->SetPreferredSavePlayerId(RewardPlayerId);
	PdGameInstance->SaveGame(RewardPlayerId);
}

void UControllerProfileSyncComponent::ApplyCollectedItemCount(
	const FString& PlayerId,
	const int32 ItemCount) const
{
	if (ItemCount <= 0)
	{
		return;
	}

	APdPlayerController* Controller = GetPdController();
	UPdGameInstance* PdGameInstance =
		Controller ? Controller->GetGameInstance<UPdGameInstance>() : nullptr;
	if (!PdGameInstance)
	{
		return;
	}

	const FString RewardPlayerId = ResolveRewardPlayerId(PlayerId);
	if (RewardPlayerId.IsEmpty())
	{
		return;
	}

	PdGameInstance->AddItemCollectedCount(RewardPlayerId, ItemCount, false);
	PdGameInstance->SetPreferredSavePlayerId(RewardPlayerId);
	PdGameInstance->SaveGame(RewardPlayerId);
}

void UControllerProfileSyncComponent::ApplySubmittedLocalCosmeticProfileOnServer(
	const TArray<FName>& OwnedSkinNames)
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->HasAuthority())
	{
		return;
	}

	if (OwnedSkinNames.Num() > MaxClientSyncedSkinNameCount)
	{
		UE_LOG(
			PdPlayerControllerLog,
			VeryVerbose,
			TEXT("Ignored oversized local cosmetic profile. Player=%s Count=%d Limit=%d"),
			*GetNameSafe(Controller->PlayerState),
			OwnedSkinNames.Num(),
			MaxClientSyncedSkinNameCount);
		return;
	}

	const bool bRemoteClaim = !Controller->IsLocalController();
	if (bRemoteClaim && !TryConsumeRemoteSkinSyncRequest())
	{
		return;
	}

	GrantDefaultSkinEntitlementsOnServer();

	APdPlayerState* PdPlayerState = Controller->GetPlayerState<APdPlayerState>();
	USkinComponent* SkinComponent = PdPlayerState ? PdPlayerState->GetSkinComponent() : nullptr;
	if (!SkinComponent)
	{
		return;
	}

	UPdGameInstance* PdGameInstance = Controller->GetGameInstance<UPdGameInstance>();
	const UContentDataSubsystem* ContentDataSubsystem =
		PdGameInstance ? PdGameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentDataSubsystem)
	{
		return;
	}

	TSet<FName> UniqueSkinNames;
	TArray<FPrimaryAssetId> OwnedSkinDefinitionIds;
	OwnedSkinDefinitionIds.Reserve(OwnedSkinNames.Num());

	for (const FName SkinName : OwnedSkinNames)
	{
		if (SkinName.IsNone() || UniqueSkinNames.Contains(SkinName))
		{
			continue;
		}

		UniqueSkinNames.Add(SkinName);
		const FPrimaryAssetId SkinDefinitionId =
			ContentDataSubsystem->GetSkinDefinitionIdByName(SkinName);
		if (SkinDefinitionId.IsValid()
			&& SkinDefinitionId.PrimaryAssetType == SkinDefinitionAssetType)
		{
			OwnedSkinDefinitionIds.AddUnique(SkinDefinitionId);
		}
	}

	if (!OwnedSkinDefinitionIds.IsEmpty())
	{
		SkinComponent->AddSkinsByPrimaryAssetIds(OwnedSkinDefinitionIds);
	}
}

APdPlayerController* UControllerProfileSyncComponent::GetPdController() const
{
	return Cast<APdPlayerController>(GetOwner());
}

FString UControllerProfileSyncComponent::ResolveRewardPlayerId(
	const FString& FallbackPlayerId) const
{
	APdPlayerController* Controller = GetPdController();
	UPdGameInstance* PdGameInstance =
		Controller ? Controller->GetGameInstance<UPdGameInstance>() : nullptr;
	if (!Controller || !PdGameInstance)
	{
		return FString();
	}

	FString RewardPlayerId = PdGameInstance->GetPreferredSavePlayerId();
	RewardPlayerId.TrimStartAndEndInline();
	if (RewardPlayerId.IsEmpty())
	{
		RewardPlayerId = PdGameInstance->ResolveSavePlayerId(
			Controller,
			Controller->GetPlayerState<APdPlayerState>());
	}
	if (RewardPlayerId.IsEmpty())
	{
		RewardPlayerId = FallbackPlayerId;
		RewardPlayerId.TrimStartAndEndInline();
	}

	return RewardPlayerId;
}

void UControllerProfileSyncComponent::PushLocalCosmeticProfileToServer()
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller
		|| (!Controller->HasAuthority() && !Controller->IsLocalController()))
	{
		CompleteLocalCosmeticProfileSyncAttempt();
		return;
	}

	if (Controller->HasAuthority())
	{
		GrantDefaultSkinEntitlementsOnServer();
	}

	if (!Controller->IsLocalController())
	{
		CompleteLocalCosmeticProfileSyncAttempt();
		return;
	}

	UPdGameInstance* PdGameInstance = Controller->GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{
		CompleteLocalCosmeticProfileSyncAttempt();
		return;
	}

	const FString PlayerId = PdGameInstance->ResolveSavePlayerId(
		Controller,
		Controller->GetPlayerState<APdPlayerState>());
	UPdSaveGame* SaveGame = PdGameInstance->GetOrCreateSaveGame(PlayerId);
	if (!SaveGame)
	{
		CompleteLocalCosmeticProfileSyncAttempt();
		return;
	}

	TArray<FName> OwnedSkinNames;
	for (const TPair<FPrimaryAssetId, int32>& SkinPair :
		SaveGame->PlayerSkinData.GrantedSkinsById)
	{
		if (SkinPair.Key.IsValid()
			&& SkinPair.Key.PrimaryAssetType == SkinDefinitionAssetType
			&& SkinPair.Value > 0)
		{
			OwnedSkinNames.AddUnique(SkinPair.Key.PrimaryAssetName);
		}
	}
	OwnedSkinNames.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});

	if (OwnedSkinNames.Num() > MaxClientSyncedSkinNameCount)
	{
		OwnedSkinNames.SetNum(
			MaxClientSyncedSkinNameCount,
			EAllowShrinking::No);
	}

	if (Controller->HasAuthority())
	{
		ApplySubmittedLocalCosmeticProfileOnServer(OwnedSkinNames);
	}
	else
	{
		Controller->Server_SubmitLocalCosmeticProfile(OwnedSkinNames);
	}

	CompleteLocalCosmeticProfileSyncAttempt();
}

void UControllerProfileSyncComponent::GrantDefaultSkinEntitlementsOnServer() const
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->HasAuthority())
	{
		return;
	}

	APdPlayerState* PdPlayerState = Controller->GetPlayerState<APdPlayerState>();
	USkinComponent* SkinComponent = PdPlayerState ? PdPlayerState->GetSkinComponent() : nullptr;
	UPdGameInstance* PdGameInstance = Controller->GetGameInstance<UPdGameInstance>();
	const UContentDataSubsystem* ContentDataSubsystem =
		PdGameInstance ? PdGameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!SkinComponent || !ContentDataSubsystem)
	{
		return;
	}

	TArray<FPrimaryAssetId> DefaultSkinDefinitionIds;
	for (const FName DefaultSkinName :
		SkinDefaultUnlockPolicy::GetDefaultUnlockedSkinNames())
	{
		const FPrimaryAssetId SkinDefinitionId =
			ContentDataSubsystem->GetSkinDefinitionIdByName(DefaultSkinName);
		if (SkinDefinitionId.IsValid()
			&& SkinDefinitionId.PrimaryAssetType == SkinDefinitionAssetType)
		{
			DefaultSkinDefinitionIds.AddUnique(SkinDefinitionId);
		}
	}

	if (!DefaultSkinDefinitionIds.IsEmpty())
	{
		SkinComponent->AddSkinsByPrimaryAssetIds(DefaultSkinDefinitionIds);
	}
}

void UControllerProfileSyncComponent::CompleteLocalCosmeticProfileSyncAttempt()
{
	++LocalCosmeticProfileSyncAttemptCount;
	if (LocalCosmeticProfileSyncAttemptCount
		>= LocalCosmeticProfileSyncMaxAttempts)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(LocalCosmeticProfileSyncTimerHandle);
		}
	}
}

bool UControllerProfileSyncComponent::TryConsumeRemoteSkinSyncRequest()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const double CurrentTime = World->GetTimeSeconds();
	if (LastRemoteSkinSyncRequestTime >= 0.0
		&& CurrentTime - LastRemoteSkinSyncRequestTime
			< RemoteSkinSyncMinInterval)
	{
		return false;
	}

	LastRemoteSkinSyncRequestTime = CurrentTime;
	return true;
}
