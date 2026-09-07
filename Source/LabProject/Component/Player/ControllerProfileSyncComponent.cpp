#include "Component/Player/ControllerProfileSyncComponent.h"

#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "SavedGameData/PlayerProfileSubsystem.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Online/AchievementSubsystem.h"
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

	bool TryResolveCanonicalAchievementId(
		UGameInstance* GameInstance,
		const FName SubmittedAchievementId,
		FName& OutAchievementId)
	{
		OutAchievementId = NAME_None;
		if (SubmittedAchievementId.IsNone())
		{
			return true;
		}

		UAchievementSubsystem* AchievementSubsystem = GameInstance
			? GameInstance->GetSubsystem<UAchievementSubsystem>()
			: nullptr;
		const UAchievementDefinition* AchievementDefinition = AchievementSubsystem
			? AchievementSubsystem->GetAchievementDefinition()
			: nullptr;
		if (!AchievementDefinition)
		{
			return false;
		}

		for (const FAchievementEntry& Achievement : AchievementDefinition->Achievements)
		{
			FString CanonicalId = Achievement.AchievementId;
			CanonicalId.TrimStartAndEndInline();
			if (Achievement.bEnabled
				&& !CanonicalId.IsEmpty()
				&& FName(*CanonicalId) == SubmittedAchievementId)
			{
				OutAchievementId = FName(*CanonicalId);
				return true;
			}
		}

		return true;
	}
}

UControllerProfileSyncComponent::UControllerProfileSyncComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UControllerProfileSyncComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSteamAchievementStateChanged();
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
	BindSteamAchievementStateChanged();

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
	UPlayerProfileSubsystem* ProfileSubsystem =
		Controller ? UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(Controller->GetGameInstance()) : nullptr;
	if (!ProfileSubsystem)
	{
		return;
	}

	const FString RewardPlayerId = ResolveRewardPlayerId(PlayerId);
	if (RewardPlayerId.IsEmpty())
	{
		return;
	}

	ProfileSubsystem->AddGold(RewardPlayerId, GoldReward, false);
	ProfileSubsystem->SetPreferredSavePlayerId(RewardPlayerId);
	ProfileSubsystem->SaveGame(RewardPlayerId);
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
	UPlayerProfileSubsystem* ProfileSubsystem =
		Controller ? UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(Controller->GetGameInstance()) : nullptr;
	if (!ProfileSubsystem)
	{
		return;
	}

	const FString RewardPlayerId = ResolveRewardPlayerId(PlayerId);
	if (RewardPlayerId.IsEmpty())
	{
		return;
	}

	ProfileSubsystem->AddItemCollectedCount(RewardPlayerId, ItemCount, false);
	ProfileSubsystem->SetPreferredSavePlayerId(RewardPlayerId);
	ProfileSubsystem->SaveGame(RewardPlayerId);
}

void UControllerProfileSyncComponent::ApplySubmittedLocalCosmeticProfileOnServer(
	const TArray<FName>& OwnedSkinNames,
	const FName SelectedAchievementId)
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
	UGameInstance* GameInstance = Controller->GetGameInstance();
	if (UPlayerMatchComponent* PlayerMatchComponent =
		PdPlayerState ? PdPlayerState->GetPlayerMatchComponent() : nullptr)
	{
		FName CanonicalAchievementId;
		if (TryResolveCanonicalAchievementId(
			GameInstance,
			SelectedAchievementId,
			CanonicalAchievementId))
		{
			PlayerMatchComponent->SetSelectedAchievementId(CanonicalAchievementId);
		}
	}

	USkinComponent* SkinComponent = PdPlayerState ? PdPlayerState->GetSkinComponent() : nullptr;
	if (!SkinComponent)
	{
		return;
	}

	const UContentDataSubsystem* ContentDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
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
	UPlayerProfileSubsystem* ProfileSubsystem =
		Controller ? UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(Controller->GetGameInstance()) : nullptr;
	if (!Controller || !ProfileSubsystem)
	{
		return FString();
	}

	FString RewardPlayerId = ProfileSubsystem->GetPreferredSavePlayerId();
	RewardPlayerId.TrimStartAndEndInline();
	if (RewardPlayerId.IsEmpty())
	{
		RewardPlayerId = ProfileSubsystem->ResolveSavePlayerId(
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

	UPlayerProfileSubsystem* ProfileSubsystem = UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(Controller->GetGameInstance());
	if (!ProfileSubsystem)
	{
		CompleteLocalCosmeticProfileSyncAttempt();
		return;
	}

	const FString PlayerId = ProfileSubsystem->ResolveSavePlayerId(
		Controller,
		Controller->GetPlayerState<APdPlayerState>());
	UPdSaveGame* SaveGame = ProfileSubsystem->GetOrCreateSaveGame(PlayerId);
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

	FName SteamValidatedAchievementId = NAME_None;
	UAchievementSubsystem* AchievementSubsystem =
		UGameInstance::GetSubsystem<UAchievementSubsystem>(Controller->GetGameInstance());
	if (AchievementSubsystem
		&& !AchievementSubsystem->IsSteamAchievementQueryComplete())
	{
		AchievementSubsystem->RequestSteamAchievementQuery();
	}

	if (!SaveGame->SelectedAchievementId.IsNone()
		&& AchievementSubsystem
		&& AchievementSubsystem->HasSteamAchievementData()
		&& AchievementSubsystem->IsSteamAchievementKnown(
			SaveGame->SelectedAchievementId.ToString())
		&& AchievementSubsystem->IsSteamAchievementUnlocked(
			SaveGame->SelectedAchievementId.ToString()))
	{
		SteamValidatedAchievementId = SaveGame->SelectedAchievementId;
	}
	else if (!SaveGame->SelectedAchievementId.IsNone()
		&& AchievementSubsystem
		&& AchievementSubsystem->HasSteamAchievementData())
	{
		ProfileSubsystem->SetSelectedAchievementId(PlayerId, NAME_None, true);
	}

	if (Controller->HasAuthority())
	{
		ApplySubmittedLocalCosmeticProfileOnServer(
			OwnedSkinNames,
			SteamValidatedAchievementId);
	}
	else
	{
		Controller->Server_SubmitLocalCosmeticProfile(
			OwnedSkinNames,
			SteamValidatedAchievementId);
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
	UGameInstance* GameInstance = Controller->GetGameInstance();
	const UContentDataSubsystem* ContentDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
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

void UControllerProfileSyncComponent::BindSteamAchievementStateChanged()
{
	if (SteamAchievementStateChangedHandle.IsValid())
	{
		return;
	}

	APdPlayerController* Controller = GetPdController();
	UGameInstance* GameInstance =
		Controller ? Controller->GetGameInstance() : nullptr;
	UAchievementSubsystem* AchievementSubsystem = GameInstance
		? GameInstance->GetSubsystem<UAchievementSubsystem>()
		: nullptr;
	if (!AchievementSubsystem)
	{
		return;
	}

	SteamAchievementStateChangedHandle =
		AchievementSubsystem->OnSteamAchievementStateChanged().AddUObject(
			this,
			&ThisClass::HandleSteamAchievementStateChanged);
}

void UControllerProfileSyncComponent::UnbindSteamAchievementStateChanged()
{
	APdPlayerController* Controller = GetPdController();
	UGameInstance* GameInstance =
		Controller ? Controller->GetGameInstance() : nullptr;
	UAchievementSubsystem* AchievementSubsystem = GameInstance
		? GameInstance->GetSubsystem<UAchievementSubsystem>()
		: nullptr;
	if (AchievementSubsystem && SteamAchievementStateChangedHandle.IsValid())
	{
		AchievementSubsystem->OnSteamAchievementStateChanged().Remove(
			SteamAchievementStateChangedHandle);
	}
	SteamAchievementStateChangedHandle.Reset();
}

void UControllerProfileSyncComponent::HandleSteamAchievementStateChanged()
{
	ScheduleLocalCosmeticProfileSync();
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
