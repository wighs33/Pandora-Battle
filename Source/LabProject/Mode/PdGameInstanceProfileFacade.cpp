#include "Mode/PdGameInstance.h"

#include "SavedGameData/PlayerProfileSubsystem.h"

void UPdGameInstance::LoadGame(const FString& PlayerId)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		ProfileSubsystem->LoadGame(PlayerId);
	}
}

void UPdGameInstance::SaveGame(const FString& PlayerId)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		ProfileSubsystem->SaveGame(PlayerId);
	}
}

UPdSaveGame* UPdGameInstance::GetOrCreateSaveGame(const FString& PlayerId)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GetOrCreateSaveGame(PlayerId);
	}

	return nullptr;
}

FString UPdGameInstance::ResolveSavePlayerId(
	const APlayerController* PlayerController,
	const APlayerState* PlayerState) const
{
	if (const UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->ResolveSavePlayerId(PlayerController, PlayerState);
	}

	return FString();
}

FString UPdGameInstance::GetLocalClientSavePlayerId() const
{
	if (const UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GetLocalClientSavePlayerId();
	}

	return TEXT("LocalProfile");
}

void UPdGameInstance::SetPreferredSavePlayerId(const FString& PlayerId)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		ProfileSubsystem->SetPreferredSavePlayerId(PlayerId);
	}
}

FString UPdGameInstance::GetPreferredSavePlayerId() const
{
	if (const UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GetPreferredSavePlayerId();
	}

	return FString();
}

void UPdGameInstance::AddMatchRecord(
	const FString& PlayerId,
	const FMatchRecord& MatchRecord,
	const bool bSaveImmediately)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		ProfileSubsystem->AddMatchRecord(PlayerId, MatchRecord, bSaveImmediately);
	}
}

TArray<FMatchRecord> UPdGameInstance::GetMatchRecords(const FString& PlayerId)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GetMatchRecords(PlayerId);
	}

	return {};
}

int32 UPdGameInstance::GetWinCount(const FString& PlayerId)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GetWinCount(PlayerId);
	}

	return 0;
}

int32 UPdGameInstance::GetItemCollectedCount(const FString& PlayerId)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GetItemCollectedCount(PlayerId);
	}

	return 0;
}

int32 UPdGameInstance::AddItemCollectedCount(
	const FString& PlayerId,
	const int32 Amount,
	const bool bSaveImmediately)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->AddItemCollectedCount(
			PlayerId,
			Amount,
			bSaveImmediately);
	}

	return 0;
}

int32 UPdGameInstance::GetGold(const FString& PlayerId)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GetGold(PlayerId);
	}

	return 0;
}

int32 UPdGameInstance::SetGold(
	const FString& PlayerId,
	const int32 NewGold,
	const bool bSaveImmediately)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->SetGold(PlayerId, NewGold, bSaveImmediately);
	}

	return 0;
}

int32 UPdGameInstance::AddGold(
	const FString& PlayerId,
	const int32 Amount,
	const bool bSaveImmediately)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->AddGold(PlayerId, Amount, bSaveImmediately);
	}

	return 0;
}

bool UPdGameInstance::SpendGold(
	const FString& PlayerId,
	const int32 Amount,
	const bool bSaveImmediately)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->SpendGold(PlayerId, Amount, bSaveImmediately);
	}

	return false;
}

int32 UPdGameInstance::GrantGameVictoryGoldReward(
	const FString& PlayerId,
	URewardDefinition* RewardDefinition,
	const bool bSaveImmediately)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GrantGameVictoryGoldReward(
			PlayerId,
			RewardDefinition,
			bSaveImmediately);
	}

	return 0;
}

bool UPdGameInstance::ResetShopSaveData(
	const FString& PlayerId,
	const bool bSaveImmediately)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->ResetShopSaveData(PlayerId, bSaveImmediately);
	}

	return false;
}

bool UPdGameInstance::IsPandoraGranted(
	const FString& PlayerId,
	UPandoraDefinition* PandoraDefinition)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->IsPandoraGranted(PlayerId, PandoraDefinition);
	}

	return false;
}

int32 UPdGameInstance::GetGrantedPandoraLevel(
	const FString& PlayerId,
	UPandoraDefinition* PandoraDefinition)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GetGrantedPandoraLevel(
			PlayerId,
			PandoraDefinition);
	}

	return 0;
}

bool UPdGameInstance::GrantPandoraToSave(
	const FString& PlayerId,
	UPandoraDefinition* PandoraDefinition,
	const int32 StartingLevel,
	const bool bSaveImmediately)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GrantPandoraToSave(
			PlayerId,
			PandoraDefinition,
			StartingLevel,
			bSaveImmediately);
	}

	return false;
}

bool UPdGameInstance::TryPurchasePandoraWithGold(
	const FString& PlayerId,
	UPandoraDefinition* PandoraDefinition,
	const int32 GoldCost,
	const int32 StartingLevel,
	int32& OutRemainingGold,
	const bool bSaveImmediately)
{
	OutRemainingGold = 0;
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->TryPurchasePandoraWithGold(
			PlayerId,
			PandoraDefinition,
			GoldCost,
			StartingLevel,
			OutRemainingGold,
			bSaveImmediately);
	}

	return false;
}

bool UPdGameInstance::IsSkinGranted(
	const FString& PlayerId,
	USkinDefinition* SkinDefinition)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->IsSkinGranted(PlayerId, SkinDefinition);
	}

	return false;
}

bool UPdGameInstance::GrantSkinToSave(
	const FString& PlayerId,
	USkinDefinition* SkinDefinition,
	const bool bSaveImmediately)
{
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->GrantSkinToSave(
			PlayerId,
			SkinDefinition,
			bSaveImmediately);
	}

	return false;
}

bool UPdGameInstance::TryPurchaseSkinWithGold(
	const FString& PlayerId,
	USkinDefinition* SkinDefinition,
	const int32 GoldCost,
	int32& OutRemainingGold,
	const bool bSaveImmediately)
{
	OutRemainingGold = 0;
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetSubsystem<UPlayerProfileSubsystem>())
	{
		return ProfileSubsystem->TryPurchaseSkinWithGold(
			PlayerId,
			SkinDefinition,
			GoldCost,
			OutRemainingGold,
			bSaveImmediately);
	}

	return false;
}
