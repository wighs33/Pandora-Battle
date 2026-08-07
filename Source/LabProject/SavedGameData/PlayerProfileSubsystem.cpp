#include "SavedGameData/PlayerProfileSubsystem.h"
#include "SavedGameData/PlayerProfilePolicy.h"

#include "Engine/AssetManager.h"
#include "Definition/Item/RewardDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Pandora/PandoraDefaultUnlockPolicy.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Skin/SkinDefaultUnlockPolicy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerProfileSubsystem)

using namespace PlayerProfilePolicy;

DEFINE_LOG_CATEGORY_STATIC(LogPlayerProfile, Log, All);

namespace
{
	int32 AddNonNegativeSaturated(const int32 CurrentValue, const int32 Amount)
	{
		return static_cast<int32>(FMath::Min<int64>(
			static_cast<int64>(FMath::Max(CurrentValue, 0))
				+ static_cast<int64>(FMath::Max(Amount, 0)),
			TNumericLimits<int32>::Max()));
	}
}

void UPlayerProfileSubsystem::AddMatchRecord(
	const FString& PlayerId,
	const FMatchRecord& MatchRecord,
	const bool bSaveImmediately)
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty())
	{
		return;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(TrimmedPlayerId);
	if (!IsValid(SaveGameObject))
	{
		return;
	}

	FMatchRecord SanitizedRecord = MatchRecord;
	SanitizedRecord.KillCount = FMath::Max(SanitizedRecord.KillCount, 0);
	SanitizedRecord.DeathCount = FMath::Max(SanitizedRecord.DeathCount, 0);
	SanitizedRecord.Reward = FMath::Max(SanitizedRecord.Reward, 0);

	SaveGameObject->MatchRecords.Add(SanitizedRecord);
	SaveGameObject->MatchPlayedCount =
		AddNonNegativeSaturated(SaveGameObject->MatchPlayedCount, 1);
	SaveGameObject->TotalKillCount =
		AddNonNegativeSaturated(SaveGameObject->TotalKillCount, SanitizedRecord.KillCount);
	SaveGameObject->TotalDeathCount =
		AddNonNegativeSaturated(SaveGameObject->TotalDeathCount, SanitizedRecord.DeathCount);
	SaveGameObject->TotalRewardGold =
		AddNonNegativeSaturated(SaveGameObject->TotalRewardGold, SanitizedRecord.Reward);
	if (SanitizedRecord.bWin)
	{
		SaveGameObject->WinCount =
			AddNonNegativeSaturated(SaveGameObject->WinCount, 1);
	}

	while (SaveGameObject->MatchRecords.Num() > MaxSavedMatchRecordCount)
	{
		SaveGameObject->MatchRecords.RemoveAt(0);
	}

	RequestProfileSave(TrimmedPlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(TrimmedPlayerId);
}

TArray<FMatchRecord> UPlayerProfileSubsystem::GetMatchRecords(const FString& PlayerId)
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty())
	{
		return {};
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(TrimmedPlayerId);
	return IsValid(SaveGameObject) ? SaveGameObject->MatchRecords : TArray<FMatchRecord>();
}

int32 UPlayerProfileSubsystem::GetWinCount(const FString& PlayerId)
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty())
	{
		return 0;
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(TrimmedPlayerId);
	return IsValid(SaveGameObject) ? FMath::Max(SaveGameObject->WinCount, 0) : 0;
}

int32 UPlayerProfileSubsystem::GetItemCollectedCount(const FString& PlayerId)
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty())
	{
		return 0;
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(TrimmedPlayerId);
	return IsValid(SaveGameObject) ? FMath::Max(SaveGameObject->ItemCollectedCount, 0) : 0;
}

int32 UPlayerProfileSubsystem::AddItemCollectedCount(
	const FString& PlayerId,
	const int32 Amount,
	const bool bSaveImmediately)
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty() || Amount <= 0)
	{
		return GetItemCollectedCount(TrimmedPlayerId);
	}

	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(TrimmedPlayerId);
	if (!IsValid(SaveGameObject))
	{
		return 0;
	}

	SaveGameObject->ItemCollectedCount =
		AddNonNegativeSaturated(SaveGameObject->ItemCollectedCount, Amount);

	RequestProfileSave(TrimmedPlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(TrimmedPlayerId);
	return SaveGameObject->ItemCollectedCount;
}

int32 UPlayerProfileSubsystem::GetGold(const FString& PlayerId)
{
	if (PlayerId.IsEmpty())
	{
		return 0;
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	return IsValid(SaveGameObject) ? SaveGameObject->Gold : 0;
}

int32 UPlayerProfileSubsystem::SetGold(const FString& PlayerId, const int32 NewGold, const bool bSaveImmediately)
{
	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return 0;
	}

	SaveGameObject->Gold = FMath::Max(0, NewGold);
	RequestProfileSave(PlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(PlayerId);
	return SaveGameObject->Gold;
}

int32 UPlayerProfileSubsystem::AddGold(const FString& PlayerId, const int32 Amount, const bool bSaveImmediately)
{
	if (Amount <= 0)
	{
		return GetGold(PlayerId);
	}

	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return 0;
	}

	SaveGameObject->Gold = AddNonNegativeSaturated(SaveGameObject->Gold, Amount);
	RequestProfileSave(PlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(PlayerId);
	return SaveGameObject->Gold;
}

bool UPlayerProfileSubsystem::SpendGold(const FString& PlayerId, const int32 Amount, const bool bSaveImmediately)
{
	if (Amount <= 0)
	{
		return true;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject) || SaveGameObject->Gold < Amount)
	{
		return false;
	}

	SaveGameObject->Gold -= Amount;
	RequestProfileSave(PlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(PlayerId);
	return true;
}

int32 UPlayerProfileSubsystem::GrantGameVictoryGoldReward(
	const FString& PlayerId,
	URewardDefinition* RewardDefinition,
	const bool bSaveImmediately)
{
	if (!RewardDefinition)
	{
		return GetGold(PlayerId);
	}

	const int32 GoldReward = RewardDefinition->RollGameVictoryGoldReward();
	if (GoldReward <= 0)
	{
		return GetGold(PlayerId);
	}

	return AddGold(PlayerId, GoldReward, bSaveImmediately);
}

bool UPlayerProfileSubsystem::ResetShopSaveData(const FString& PlayerId, const bool bSaveImmediately)
{
	if (PlayerId.IsEmpty())
	{
		return false;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	SaveGameObject->Gold = 0;
	SaveGameObject->PlayerPandoraData.GrantedPandorasById.Reset();
	SaveGameObject->PlayerPandoraData.SelectedPandoraId = FPrimaryAssetId();
	SaveGameObject->PlayerPandoraData.PandoraLoadoutByDirectionId.Reset();
	SaveGameObject->PlayerSkinData.GrantedSkinsById.Reset();
	EnsureDefaultUnlockedSkins(*SaveGameObject);

	RequestProfileSave(PlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(PlayerId);
	return true;
}

bool UPlayerProfileSubsystem::IsPandoraGranted(const FString& PlayerId, UPandoraDefinition* PandoraDefinition)
{
	if (!PandoraDefinition)
	{
		return false;
	}

	const FPrimaryAssetId PandoraId = ResolvePandoraSaveId(PandoraDefinition);
	if (PandoraDefaultUnlockPolicy::IsDefaultUnlockedPandoraDefinition(PandoraDefinition))
	{
		return true;
	}

	if (PlayerId.IsEmpty())
	{
		return false;
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	return SaveGameObject->PlayerPandoraData.GrantedPandorasById.Contains(PandoraId);
}

int32 UPlayerProfileSubsystem::GetGrantedPandoraLevel(const FString& PlayerId, UPandoraDefinition* PandoraDefinition)
{
	if (!PandoraDefinition)
	{
		return 0;
	}

	const FPrimaryAssetId PandoraId = ResolvePandoraSaveId(PandoraDefinition);
	if (PandoraDefaultUnlockPolicy::IsDefaultUnlockedPandoraDefinition(PandoraDefinition))
	{
		return 0;
	}

	if (PlayerId.IsEmpty())
	{
		return 0;
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return 0;
	}

	const int32* SavedLevel =
		SaveGameObject->PlayerPandoraData.GrantedPandorasById.Find(PandoraId);
	return SavedLevel ? FMath::Max(*SavedLevel, 1) : 0;
}

bool UPlayerProfileSubsystem::GrantPandoraToSave(
	const FString& PlayerId,
	UPandoraDefinition* PandoraDefinition,
	const int32 StartingLevel,
	const bool bSaveImmediately)
{
	if (PlayerId.IsEmpty() || !PandoraDefinition)
	{
		return false;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	const FPrimaryAssetId PandoraId = ResolvePandoraSaveId(PandoraDefinition);
	if (!PandoraId.IsValid()
		|| PandoraDefaultUnlockPolicy::IsDefaultUnlockedPandoraDefinition(PandoraDefinition))
	{
		return false;
	}

	int32& GrantedLevel = SaveGameObject->PlayerPandoraData.GrantedPandorasById.FindOrAdd(PandoraId);
	GrantedLevel = FMath::Max(GrantedLevel, FMath::Max(StartingLevel, 1));

	RequestProfileSave(PlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(PlayerId);
	return true;
}

bool UPlayerProfileSubsystem::TryPurchasePandoraWithGold(
	const FString& PlayerId,
	UPandoraDefinition* PandoraDefinition,
	const int32 GoldCost,
	const int32 StartingLevel,
	int32& OutRemainingGold,
	const bool bSaveImmediately)
{
	OutRemainingGold = GetGold(PlayerId);
	if (PlayerId.IsEmpty() || !PandoraDefinition)
	{
		return false;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	const FPrimaryAssetId PandoraId = ResolvePandoraSaveId(PandoraDefinition);
	if (!PandoraId.IsValid()
		|| PandoraDefaultUnlockPolicy::IsDefaultUnlockedPandoraDefinition(PandoraDefinition))
	{
		return false;
	}

	if (SaveGameObject->PlayerPandoraData.GrantedPandorasById.Contains(PandoraId))
	{
		return false;
	}

	const int32 SanitizedGoldCost = FMath::Max(0, GoldCost);
	if (SaveGameObject->Gold < SanitizedGoldCost)
	{
		return false;
	}

	SaveGameObject->Gold -= SanitizedGoldCost;
	SaveGameObject->PlayerPandoraData.GrantedPandorasById.Add(
		PandoraId,
		FMath::Max(StartingLevel, 1));
	OutRemainingGold = SaveGameObject->Gold;

	RequestProfileSave(PlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(PlayerId);
	return true;
}

bool UPlayerProfileSubsystem::IsSkinGranted(const FString& PlayerId, USkinDefinition* SkinDefinition)
{
	if (!SkinDefinition)
	{
		return false;
	}

	const FPrimaryAssetId SkinId = ResolveSkinSaveId(SkinDefinition);
	if (SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinDefinition(SkinDefinition))
	{
		return true;
	}

	if (PlayerId.IsEmpty())
	{
		return false;
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	return SaveGameObject->PlayerSkinData.GrantedSkinsById.Contains(SkinId);
}

bool UPlayerProfileSubsystem::GrantSkinToSave(
	const FString& PlayerId,
	USkinDefinition* SkinDefinition,
	const bool bSaveImmediately)
{
	if (PlayerId.IsEmpty() || !SkinDefinition)
	{
		return false;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	const FPrimaryAssetId SkinId = ResolveSkinSaveId(SkinDefinition);
	if (!SkinId.IsValid()
		|| SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinDefinition(SkinDefinition))
	{
		return false;
	}

	SaveGameObject->PlayerSkinData.GrantedSkinsById.FindOrAdd(SkinId) = 1;

	RequestProfileSave(PlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(PlayerId);
	return true;
}

bool UPlayerProfileSubsystem::TryPurchaseSkinWithGold(
	const FString& PlayerId,
	USkinDefinition* SkinDefinition,
	const int32 GoldCost,
	int32& OutRemainingGold,
	const bool bSaveImmediately)
{
	OutRemainingGold = GetGold(PlayerId);
	if (PlayerId.IsEmpty() || !SkinDefinition)
	{
		return false;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(PlayerId);
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	const FPrimaryAssetId SkinId = ResolveSkinSaveId(SkinDefinition);
	if (!SkinId.IsValid()
		|| SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinDefinition(SkinDefinition)
		|| SaveGameObject->PlayerSkinData.GrantedSkinsById.Contains(SkinId))
	{
		return false;
	}

	const int32 SanitizedGoldCost = FMath::Max(0, GoldCost);
	if (SaveGameObject->Gold < SanitizedGoldCost)
	{
		return false;
	}

	SaveGameObject->Gold -= SanitizedGoldCost;
	SaveGameObject->PlayerSkinData.GrantedSkinsById.Add(SkinId, 1);
	OutRemainingGold = SaveGameObject->Gold;

	RequestProfileSave(PlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(PlayerId);
	return true;
}

void UPlayerProfileSubsystem::NotifyProfileProgressChanged(const FString& PlayerId)
{
	const FString NormalizedPlayerId = NormalizeProfilePlayerId(PlayerId);
	if (!NormalizedPlayerId.IsEmpty())
	{
		ProfileProgressChanged.Broadcast(NormalizedPlayerId);
	}
}

UPdSaveGame* UPlayerProfileSubsystem::CreateConfiguredSaveGameObject() const
{
	UPdSaveGame* SaveGameObject =
		Cast<UPdSaveGame>(UGameplayStatics::CreateSaveGameObject(
			UPdSaveGame::StaticClass()));
	if (SaveGameObject)
	{
		SaveGameObject->ProfileDataVersion = PdProfileSaveData::Current;
		SaveGameObject->SaveId = FGuid::NewGuid();
		SaveGameObject->SaveRevision = 0;
	}
	return SaveGameObject;
}
