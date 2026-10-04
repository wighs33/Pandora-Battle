#include "Profile/PlayerProfileSubsystem.h"
#include "Profile/PlayerProfileSaveGame.h"

#include "Engine/AssetManager.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerProfileSubsystem)

namespace
{
	FPrimaryAssetId ResolveRedirectedAssetId(const FPrimaryAssetId& AssetId)
	{
		if (!AssetId.IsValid())
		{
			return FPrimaryAssetId();
		}

		if (const UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
		{
			const FPrimaryAssetId RedirectedId = AssetManager->GetRedirectedPrimaryAssetId(AssetId);
			if (RedirectedId.IsValid())
			{
				return RedirectedId;
			}
		}

		return AssetId;
	}

	FPrimaryAssetId ResolvePandoraSaveId(const UPandoraDefinition* PandoraDefinition)
	{
		return PandoraDefinition ? ResolveRedirectedAssetId(PandoraDefinition->GetPrimaryAssetId()) : FPrimaryAssetId();
	}

	FPrimaryAssetId ResolveSkinSaveId(const USkinDefinition* SkinDefinition)
	{
		return SkinDefinition ? ResolveRedirectedAssetId(SkinDefinition->GetPrimaryAssetId()) : FPrimaryAssetId();
	}

	int32 AddNonNegativeSaturated(const int32 CurrentValue, const int32 Amount)
	{
		return static_cast<int32>(FMath::Min<int64>(static_cast<int64>(FMath::Max(CurrentValue, 0))
			+ static_cast<int64>(FMath::Max(Amount, 0)), TNumericLimits<int32>::Max()));
	}
}

void UPlayerProfileSubsystem::AddMatchRecord(const FMatchRecord& MatchRecord, const bool bSaveImmediately)
{
	UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject))
	{
		return;
	}

	FMatchRecord SanitizedRecord = MatchRecord;
	SanitizedRecord.KillCount = FMath::Max(SanitizedRecord.KillCount, 0);
	SanitizedRecord.DeathCount = FMath::Max(SanitizedRecord.DeathCount, 0);
	SanitizedRecord.Reward = FMath::Max(SanitizedRecord.Reward, 0);

	SaveGameObject->MatchRecords.Add(SanitizedRecord);
	SaveGameObject->MatchPlayedCount = AddNonNegativeSaturated(SaveGameObject->MatchPlayedCount, 1);
	SaveGameObject->TotalKillCount = AddNonNegativeSaturated(SaveGameObject->TotalKillCount, SanitizedRecord.KillCount);
	SaveGameObject->TotalDeathCount =
		AddNonNegativeSaturated(SaveGameObject->TotalDeathCount, SanitizedRecord.DeathCount);
	SaveGameObject->TotalRewardGold = AddNonNegativeSaturated(SaveGameObject->TotalRewardGold, SanitizedRecord.Reward);
	if (SanitizedRecord.bWin)
	{
		SaveGameObject->WinCount = AddNonNegativeSaturated(SaveGameObject->WinCount, 1);
	}

	while (SaveGameObject->MatchRecords.Num() > PlayerProfileDataVersion::MaxMatchRecordCount)
	{
		SaveGameObject->MatchRecords.RemoveAt(0);
	}

	RequestProfileSave(bSaveImmediately);

	ProfileProgressChanged.Broadcast();
}

TArray<FMatchRecord> UPlayerProfileSubsystem::GetMatchRecords()
{
	const UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	return IsValid(SaveGameObject) ? SaveGameObject->MatchRecords : TArray<FMatchRecord>();
}

int32 UPlayerProfileSubsystem::GetWinCount()
{
	const UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	return IsValid(SaveGameObject) ? FMath::Max(SaveGameObject->WinCount, 0) : 0;
}

int32 UPlayerProfileSubsystem::GetItemCollectedCount()
{
	const UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	return IsValid(SaveGameObject) ? FMath::Max(SaveGameObject->ItemCollectedCount, 0) : 0;
}

int32 UPlayerProfileSubsystem::AddItemCollectedCount(const int32 Amount, const bool bSaveImmediately)
{
	if (Amount <= 0)
	{
		return GetItemCollectedCount();
	}

	UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject))
	{
		return 0;
	}

	SaveGameObject->ItemCollectedCount = AddNonNegativeSaturated(SaveGameObject->ItemCollectedCount, Amount);

	RequestProfileSave(bSaveImmediately);

	ProfileProgressChanged.Broadcast();
	return SaveGameObject->ItemCollectedCount;
}

FName UPlayerProfileSubsystem::GetSelectedAchievementId()
{
	const UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	return IsValid(SaveGameObject) ? SaveGameObject->SelectedAchievementId : NAME_None;
}

bool UPlayerProfileSubsystem::SetSelectedAchievementId(const FName AchievementId, const bool bSaveImmediately)
{
	UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	if (SaveGameObject->SelectedAchievementId == AchievementId)
	{
		return true;
	}

	SaveGameObject->SelectedAchievementId = AchievementId;
	RequestProfileSave(bSaveImmediately);
	return true;
}

int32 UPlayerProfileSubsystem::GetGold()
{
	const UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	return IsValid(SaveGameObject) ? SaveGameObject->Gold : 0;
}

int32 UPlayerProfileSubsystem::AddGold(const int32 Amount, const bool bSaveImmediately)
{
	if (Amount <= 0)
	{
		return GetGold();
	}

	UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject))
	{
		return 0;
	}

	SaveGameObject->Gold = AddNonNegativeSaturated(SaveGameObject->Gold, Amount);
	RequestProfileSave(bSaveImmediately);

	ProfileProgressChanged.Broadcast();
	return SaveGameObject->Gold;
}

bool UPlayerProfileSubsystem::SpendGold(const int32 Amount, const bool bSaveImmediately)
{
	if (Amount <= 0)
	{
		return true;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject) || SaveGameObject->Gold < Amount)
	{
		return false;
	}

	SaveGameObject->Gold -= Amount;
	RequestProfileSave(bSaveImmediately);

	ProfileProgressChanged.Broadcast();
	return true;
}

bool UPlayerProfileSubsystem::ResetPurchasedProgress(const bool bSaveImmediately)
{
	UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	SaveGameObject->Gold = 0;
	SaveGameObject->PlayerPandoraData.GrantedPandorasById.Reset();
	SaveGameObject->PlayerSkinData.GrantedSkinsById.Reset();
	EnsureDefaultUnlockedSkins(*SaveGameObject);

	RequestProfileSave(bSaveImmediately);

	ProfileProgressChanged.Broadcast();
	return true;
}

bool UPlayerProfileSubsystem::IsPandoraGranted(const UPandoraDefinition* PandoraDefinition)
{
	if (!PandoraDefinition)
	{
		return false;
	}

	const FPrimaryAssetId PandoraId = ResolvePandoraSaveId(PandoraDefinition);
	const UDefaultProvisionDefinition* DefaultProvision = IsValid(PandoraDefinition)
		? UDefaultProvisionDefinition::ResolveDefaultDefinition() : nullptr;
	if (DefaultProvision && DefaultProvision->IsPandoraKeyGranted(PandoraDefinition->GetFName(), EDefaultProvisionMode::Gameplay))
	{
		return true;
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	return SaveGameObject->PlayerPandoraData.GrantedPandorasById.Contains(PandoraId);
}

bool UPlayerProfileSubsystem::TryPurchasePandoraWithGold(const UPandoraDefinition* PandoraDefinition,
	const int32 GoldCost, const int32 StartingLevel, int32& OutRemainingGold, const bool bSaveImmediately)
{
	OutRemainingGold = GetGold();
	if (!PandoraDefinition)
	{
		return false;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	const FPrimaryAssetId PandoraId = ResolvePandoraSaveId(PandoraDefinition);
	const UDefaultProvisionDefinition* DefaultProvision = PandoraId.IsValid() && IsValid(PandoraDefinition)
		? UDefaultProvisionDefinition::ResolveDefaultDefinition() : nullptr;
	if (!PandoraId.IsValid()
		|| (DefaultProvision && DefaultProvision->IsPandoraKeyGranted(PandoraDefinition->GetFName(), EDefaultProvisionMode::Gameplay)))
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
	SaveGameObject->PlayerPandoraData.GrantedPandorasById.Add(PandoraId, FMath::Max(StartingLevel, 1));
	OutRemainingGold = SaveGameObject->Gold;

	RequestProfileSave(bSaveImmediately);

	ProfileProgressChanged.Broadcast();
	return true;
}

bool UPlayerProfileSubsystem::IsSkinGranted(USkinDefinition* SkinDefinition)
{
	if (!SkinDefinition)
	{
		return false;
	}

	const FPrimaryAssetId SkinId = ResolveSkinSaveId(SkinDefinition);
	if (SkinDefinition->IsDefaultProfileSkin())
	{
		return true;
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	return SaveGameObject->PlayerSkinData.GrantedSkinsById.Contains(SkinId);
}

bool UPlayerProfileSubsystem::TryPurchaseSkinWithGold(USkinDefinition* SkinDefinition, const int32 GoldCost,
	int32& OutRemainingGold, const bool bSaveImmediately)
{
	OutRemainingGold = GetGold();
	if (!SkinDefinition)
	{
		return false;
	}

	UPdSaveGame* SaveGameObject = GetOrCreateProfile();
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	const FPrimaryAssetId SkinId = ResolveSkinSaveId(SkinDefinition);
	if (!SkinId.IsValid() || SkinDefinition->IsDefaultProfileSkin()
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

	RequestProfileSave(bSaveImmediately);

	ProfileProgressChanged.Broadcast();
	return true;
}

bool UPlayerProfileSubsystem::EnsureDefaultUnlockedSkins(UPdSaveGame& SaveGame)
{
	bool bChanged = false;
	TArray<FPrimaryAssetId> DefaultSkinIds;
	GetGameInstance()->GetSubsystem<UContentDataSubsystem>()->GetDefaultSkinDefinitionIds(DefaultSkinIds);
	for (const FPrimaryAssetId& SkinId : DefaultSkinIds)
	{
		const FPrimaryAssetId DefaultSkinId = ResolveRedirectedAssetId(SkinId);
		if (!DefaultSkinId.IsValid())
		{
			continue;
		}

		int32& GrantedValue = SaveGame.PlayerSkinData.GrantedSkinsById.FindOrAdd(DefaultSkinId);
		if (GrantedValue < 1)
		{
			GrantedValue = 1;
			bChanged = true;
		}
	}
	return bChanged;
}

FPlayerProfileProgressSnapshot UPlayerProfileSubsystem::GetProgressSnapshot()
{
	FPlayerProfileProgressSnapshot Result;
	if (const UPdSaveGame* Profile = GetOrCreateProfile())
	{
		Result.MatchRecords = Profile->MatchRecords;
		Result.Gold = Profile->Gold;
		Result.MatchPlayedCount = Profile->MatchPlayedCount;
		Result.WinCount = Profile->WinCount;
		Result.TotalKillCount = Profile->TotalKillCount;
		Result.TotalDeathCount = Profile->TotalDeathCount;
		Result.TotalRewardGold = Profile->TotalRewardGold;
		Result.ItemCollectedCount = Profile->ItemCollectedCount;
		Result.GrantedPandoraCount = Profile->PlayerPandoraData.GrantedPandorasById.Num();
		Result.GrantedSkinCount = Profile->PlayerSkinData.GrantedSkinsById.Num();
	}
	return Result;
}

TArray<FPrimaryAssetId> UPlayerProfileSubsystem::GetOwnedSkinAssetIds()
{
	TArray<FPrimaryAssetId> Result;
	if (const UPdSaveGame* Profile = GetOrCreateProfile())
	{
		for (const auto& Skin : Profile->PlayerSkinData.GrantedSkinsById)
			if (Skin.Value > 0) Result.Add(Skin.Key);
	}
	return Result;
}
