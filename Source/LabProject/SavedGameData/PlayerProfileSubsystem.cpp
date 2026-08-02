#include "SavedGameData/PlayerProfileSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Definition/Item/RewardDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Pandora/PandoraDefaultUnlockPolicy.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "Skin/SkinDefaultUnlockPolicy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerProfileSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogPlayerProfile, Log, All);

namespace
{
	constexpr int32 MaxSavedMatchRecordCount = 5;

	bool IsSteamSubsystemName(const FName SubsystemName)
	{
		return SubsystemName.ToString().Equals(TEXT("STEAM"), ESearchCase::IgnoreCase);
	}

	const FString& GetLocalProfileSaveSlotName()
	{
		static const FString LocalProfileSaveSlotName(TEXT("LocalProfile"));
		return LocalProfileSaveSlotName;
	}

	bool IsCurrentLocalOnlineProfileId(const FString& PlayerId)
	{
		IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
		if (!OnlineSubsystem
			|| !IsSteamSubsystemName(OnlineSubsystem->GetSubsystemName()))
		{
			return false;
		}

		const IOnlineIdentityPtr IdentityInterface =
			OnlineSubsystem->GetIdentityInterface();
		const FUniqueNetIdPtr LocalUserId = IdentityInterface.IsValid()
			? IdentityInterface->GetUniquePlayerId(0)
			: nullptr;
		return LocalUserId.IsValid()
			&& PlayerId.Equals(
				LocalUserId->ToString(),
				ESearchCase::CaseSensitive);
	}

	const FPrimaryAssetType& GetPandoraDefinitionAssetType()
	{
		static const FPrimaryAssetType AssetType(TEXT("PandoraDefinition"));
		return AssetType;
	}

	const FPrimaryAssetType& GetSkinDefinitionAssetType()
	{
		static const FPrimaryAssetType AssetType(TEXT("SkinDefinition"));
		return AssetType;
	}

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

	FPrimaryAssetId MakeAssetIdFromLegacyName(
		const FPrimaryAssetType& AssetType,
		const FName AssetName)
	{
		return AssetName.IsNone()
			? FPrimaryAssetId()
			: ResolveRedirectedAssetId(FPrimaryAssetId(AssetType, AssetName));
	}

	FPrimaryAssetId ResolvePandoraSaveId(const UPandoraDefinition* PandoraDefinition)
	{
		return PandoraDefinition
			? ResolveRedirectedAssetId(PandoraDefinition->GetPrimaryAssetId())
			: FPrimaryAssetId();
	}

	FPrimaryAssetId ResolveSkinSaveId(const USkinDefinition* SkinDefinition)
	{
		return SkinDefinition
			? ResolveRedirectedAssetId(SkinDefinition->GetPrimaryAssetId())
			: FPrimaryAssetId();
	}

	bool EnsureDefaultUnlockedSkins(UPdSaveGame& SaveGame)
	{
		bool bChanged = false;
		for (const FName DefaultSkinName : SkinDefaultUnlockPolicy::GetDefaultUnlockedSkinNames())
		{
			if (DefaultSkinName.IsNone())
			{
				continue;
			}

			const FPrimaryAssetId DefaultSkinId =
				MakeAssetIdFromLegacyName(GetSkinDefinitionAssetType(), DefaultSkinName);
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

	bool HasNonDefaultGrantedSkin(const UPdSaveGame* SaveGame)
	{
		if (!SaveGame)
		{
			return false;
		}

		for (const TPair<FPrimaryAssetId, int32>& SkinPair : SaveGame->PlayerSkinData.GrantedSkinsById)
		{
			if (SkinPair.Value > 0
				&& !SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinName(SkinPair.Key.PrimaryAssetName))
			{
				return true;
			}
		}
		return false;
	}

	bool AreMatchRecordsEquivalent(const FPdMatchRecord& A, const FPdMatchRecord& B)
	{
		return A.bWin == B.bWin
			&& A.KillCount == B.KillCount
			&& A.DeathCount == B.DeathCount
			&& A.Reward == B.Reward;
	}

	bool HasProfileProgress(const UPdSaveGame* SaveGame)
	{
		return SaveGame
			&& (SaveGame->Gold > 0
				|| SaveGame->MatchPlayedCount > 0
				|| SaveGame->WinCount > 0
				|| SaveGame->TotalKillCount > 0
				|| SaveGame->TotalDeathCount > 0
				|| SaveGame->TotalRewardGold > 0
				|| SaveGame->ItemCollectedCount > 0
				|| !SaveGame->PlayerPandoraData.GrantedPandorasById.IsEmpty()
				|| SaveGame->PlayerPandoraData.SelectedPandoraId.IsValid()
				|| !SaveGame->PlayerPandoraData.PandoraLoadoutByDirectionId.IsEmpty()
				|| HasNonDefaultGrantedSkin(SaveGame)
				|| !SaveGame->MatchRecords.IsEmpty());
	}

	void MergeProfileSaveData(UPdSaveGame& Target, const UPdSaveGame& Source)
	{
		Target.Gold = FMath::Max(Target.Gold, Source.Gold);
		Target.MatchPlayedCount = FMath::Max(Target.MatchPlayedCount, Source.MatchPlayedCount);
		Target.WinCount = FMath::Max(Target.WinCount, Source.WinCount);
		Target.TotalKillCount = FMath::Max(Target.TotalKillCount, Source.TotalKillCount);
		Target.TotalDeathCount = FMath::Max(Target.TotalDeathCount, Source.TotalDeathCount);
		Target.TotalRewardGold = FMath::Max(Target.TotalRewardGold, Source.TotalRewardGold);
		Target.ItemCollectedCount = FMath::Max(Target.ItemCollectedCount, Source.ItemCollectedCount);

		for (const TPair<FPrimaryAssetId, int32>& PandoraPair : Source.PlayerPandoraData.GrantedPandorasById)
		{
			const FPrimaryAssetId PandoraId = ResolveRedirectedAssetId(PandoraPair.Key);
			if (!PandoraId.IsValid())
			{
				continue;
			}

			int32& TargetLevel = Target.PlayerPandoraData.GrantedPandorasById.FindOrAdd(PandoraId);
			TargetLevel = FMath::Max(TargetLevel, PandoraPair.Value);
		}

		if (!Target.PlayerPandoraData.SelectedPandoraId.IsValid())
		{
			Target.PlayerPandoraData.SelectedPandoraId =
				ResolveRedirectedAssetId(Source.PlayerPandoraData.SelectedPandoraId);
		}

		for (const TPair<FName, FPrimaryAssetId>& LoadoutPair :
			Source.PlayerPandoraData.PandoraLoadoutByDirectionId)
		{
			const FPrimaryAssetId PandoraId = ResolveRedirectedAssetId(LoadoutPair.Value);
			if (!LoadoutPair.Key.IsNone() && PandoraId.IsValid())
			{
				Target.PlayerPandoraData.PandoraLoadoutByDirectionId.FindOrAdd(LoadoutPair.Key) = PandoraId;
			}
		}

		for (const TPair<FPrimaryAssetId, int32>& SkinPair : Source.PlayerSkinData.GrantedSkinsById)
		{
			const FPrimaryAssetId SkinId = ResolveRedirectedAssetId(SkinPair.Key);
			if (!SkinId.IsValid())
			{
				continue;
			}

			int32& TargetValue = Target.PlayerSkinData.GrantedSkinsById.FindOrAdd(SkinId);
			TargetValue = FMath::Max(TargetValue, SkinPair.Value);
		}

		for (const FPdMatchRecord& SourceRecord : Source.MatchRecords)
		{
			const bool bAlreadyAdded = Target.MatchRecords.ContainsByPredicate(
				[&SourceRecord](const FPdMatchRecord& TargetRecord)
				{
					return AreMatchRecordsEquivalent(TargetRecord, SourceRecord);
				});
			if (!bAlreadyAdded)
			{
				Target.MatchRecords.Add(SourceRecord);
			}
		}

		while (Target.MatchRecords.Num() > MaxSavedMatchRecordCount)
		{
			Target.MatchRecords.RemoveAt(0);
		}
	}
}

UPlayerProfileSubsystem::UPlayerProfileSubsystem()
{
	Settings.SaveGameClass = UPdSaveGame::StaticClass();
}

void UPlayerProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsDeinitializing = false;
}

void UPlayerProfileSubsystem::ApplySettings(
	const FPdPlayerProfilePersistenceSettings& InSettings)
{
	Settings = InSettings;
	if (!Settings.SaveGameClass)
	{
		Settings.SaveGameClass = UPdSaveGame::StaticClass();
	}
}

void UPlayerProfileSubsystem::Deinitialize()
{
	bIsDeinitializing = true;
	if (SaveTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(SaveTickerHandle);
		SaveTickerHandle.Reset();
	}

	FlushPendingSavesToShutdownSlots();
	DirtyPlayerIds.Reset();
	SaveDeadlinesByPlayerId.Reset();

	Super::Deinitialize();
}

void UPlayerProfileSubsystem::LoadGame(const FString& PlayerId)
{
	if (GetOrCreateSaveGame(PlayerId))
	{
		NotifyProfileProgressChanged(PlayerId);
	}
}

void UPlayerProfileSubsystem::SaveGame(const FString& PlayerId)
{
	if (PlayerId.IsEmpty())
	{
		return;
	}

	RequestProfileSave(PlayerId, true);
}

UPdSaveGame* UPlayerProfileSubsystem::GetOrCreateSaveGame(const FString& PlayerId)
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty())
	{
		return nullptr;
	}

	if (UPdSaveGame* ExistingSaveGame = SavedGameByPlayerId.FindRef(TrimmedPlayerId))
	{
		bool bSchemaChanged = false;
		if (!UpgradeSaveGameSchema(*ExistingSaveGame, TrimmedPlayerId, bSchemaChanged))
		{
			return nullptr;
		}

		if (EnsureDefaultUnlockedSkins(*ExistingSaveGame) || bSchemaChanged)
		{
			RequestProfileSave(TrimmedPlayerId, false);
		}
		TryImportLocalProfileSave(TrimmedPlayerId, ExistingSaveGame);
		return ExistingSaveGame;
	}

	bool bRecoveredFromFallback = false;
	bool bRequiresStorageMigration = false;
	UPdSaveGame* SaveGameObject =
		LoadBestAvailableSaveGame(
			TrimmedPlayerId,
			bRecoveredFromFallback,
			bRequiresStorageMigration);
	if (!SaveGameObject)
	{
		SaveGameObject = CreateConfiguredSaveGameObject();
	}

	if (IsValid(SaveGameObject))
	{
		bool bSchemaChanged = false;
		if (!UpgradeSaveGameSchema(*SaveGameObject, TrimmedPlayerId, bSchemaChanged))
		{
			return nullptr;
		}

		const bool bDefaultsChanged = EnsureDefaultUnlockedSkins(*SaveGameObject);
		SavedGameByPlayerId.Add(TrimmedPlayerId, SaveGameObject);
		TryImportLocalProfileSave(TrimmedPlayerId, SaveGameObject);
		if (bRecoveredFromFallback
			|| bRequiresStorageMigration
			|| bSchemaChanged
			|| bDefaultsChanged)
		{
			RequestProfileSave(
				TrimmedPlayerId,
				bRecoveredFromFallback || bRequiresStorageMigration);
		}
	}

	return SaveGameObject;
}

UPdSaveGame* UPlayerProfileSubsystem::LoadBestAvailableSaveGame(
	const FString& PlayerId,
	bool& bOutRecoveredFromFallback,
	bool& bOutRequiresStorageMigration) const
{
	bOutRecoveredFromFallback = false;
	bOutRequiresStorageMigration = false;

	const TArray<FString> CandidateSlotNames = {
		PlayerId,
		GetBackupSlotName(PlayerId),
		GetShutdownSlotName(PlayerId)
	};

	UPdSaveGame* BestSaveGame = nullptr;
	FString BestSlotName;
	bool bBestRequiresStorageMigration = false;
	for (const FString& CandidateSlotName : CandidateSlotNames)
	{
		if (!UGameplayStatics::DoesSaveGameExist(CandidateSlotName, 0))
		{
			continue;
		}

		USaveGame* StoredSaveGame =
			UGameplayStatics::LoadGameFromSlot(CandidateSlotName, 0);
		UPdSaveGame* CandidateSaveGame = nullptr;
		bool bCandidateRequiresStorageMigration = false;
		if (UPdProfileSaveEnvelope* Envelope =
			Cast<UPdProfileSaveEnvelope>(StoredSaveGame))
		{
			CandidateSaveGame = Envelope->DecodeProfile(PlayerId);
		}
		else
		{
			CandidateSaveGame = Cast<UPdSaveGame>(StoredSaveGame);
			bCandidateRequiresStorageMigration = CandidateSaveGame != nullptr;
		}

		if (!CandidateSaveGame)
		{
			UE_LOG(
				LogPlayerProfile,
				Warning,
				TEXT("Profile save slot '%s' exists but could not be loaded."),
				*CandidateSlotName);
			continue;
		}

		if (!BestSaveGame || CandidateSaveGame->SaveRevision > BestSaveGame->SaveRevision)
		{
			BestSaveGame = CandidateSaveGame;
			BestSlotName = CandidateSlotName;
			bBestRequiresStorageMigration =
				bCandidateRequiresStorageMigration;
		}
	}

	bOutRecoveredFromFallback =
		BestSaveGame && !BestSlotName.Equals(PlayerId, ESearchCase::CaseSensitive);
	bOutRequiresStorageMigration =
		BestSaveGame && bBestRequiresStorageMigration;

	return BestSaveGame;
}

bool UPlayerProfileSubsystem::UpgradeSaveGameSchema(
	UPdSaveGame& SaveGameObject,
	const FString& SourceSlotName,
	bool& bOutChanged) const
{
	bOutChanged = false;
	if (SaveGameObject.SaveSchemaVersion > PdSaveGameSchema::Current)
	{
		UE_LOG(
			LogPlayerProfile,
			Error,
			TEXT("Profile '%s' uses unsupported future schema %d (current %d). "
				"The save will remain untouched."),
			*SourceSlotName,
			SaveGameObject.SaveSchemaVersion,
			PdSaveGameSchema::Current);
		return false;
	}

	if (!SaveGameObject.SaveId.IsValid())
	{
		SaveGameObject.SaveId = FGuid::NewGuid();
		bOutChanged = true;
	}
	SaveGameObject.SaveRevision = FMath::Max<int64>(SaveGameObject.SaveRevision, 0);

	FPlayerPandoraData& PandoraData = SaveGameObject.PlayerPandoraData;
	for (const TPair<FName, int32>& LegacyPair : PandoraData.GrantedPandorasByName)
	{
		const FPrimaryAssetId PandoraId =
			MakeAssetIdFromLegacyName(GetPandoraDefinitionAssetType(), LegacyPair.Key);
		if (!PandoraId.IsValid())
		{
			continue;
		}

		int32& GrantedLevel = PandoraData.GrantedPandorasById.FindOrAdd(PandoraId);
		GrantedLevel = FMath::Max(GrantedLevel, FMath::Max(LegacyPair.Value, 1));
		bOutChanged = true;
	}

	if (!PandoraData.SelectedPandoraName.IsNone())
	{
		const FPrimaryAssetId SelectedPandoraId =
			MakeAssetIdFromLegacyName(
				GetPandoraDefinitionAssetType(),
				PandoraData.SelectedPandoraName);
		if (SelectedPandoraId.IsValid() && !PandoraData.SelectedPandoraId.IsValid())
		{
			PandoraData.SelectedPandoraId = SelectedPandoraId;
		}
		bOutChanged = true;
	}

	for (const TPair<FName, FName>& LegacyPair : PandoraData.PandoraLoadoutByDirection)
	{
		const FPrimaryAssetId PandoraId =
			MakeAssetIdFromLegacyName(GetPandoraDefinitionAssetType(), LegacyPair.Value);
		if (!LegacyPair.Key.IsNone() && PandoraId.IsValid())
		{
			PandoraData.PandoraLoadoutByDirectionId.FindOrAdd(LegacyPair.Key) = PandoraId;
		}
		bOutChanged = true;
	}

	FPlayerSkinData& SkinData = SaveGameObject.PlayerSkinData;
	for (const TPair<FName, int32>& LegacyPair : SkinData.GrantedSkinsByName)
	{
		const FPrimaryAssetId SkinId =
			MakeAssetIdFromLegacyName(GetSkinDefinitionAssetType(), LegacyPair.Key);
		if (!SkinId.IsValid())
		{
			continue;
		}

		int32& GrantedValue = SkinData.GrantedSkinsById.FindOrAdd(SkinId);
		GrantedValue = FMath::Max(GrantedValue, FMath::Max(LegacyPair.Value, 1));
		bOutChanged = true;
	}

	const int32 OriginalPandoraCount = PandoraData.GrantedPandorasById.Num();
	TMap<FPrimaryAssetId, int32> RedirectedPandoras;
	for (const TPair<FPrimaryAssetId, int32>& PandoraPair : PandoraData.GrantedPandorasById)
	{
		const FPrimaryAssetId PandoraId = ResolveRedirectedAssetId(PandoraPair.Key);
		if (PandoraId.IsValid())
		{
			int32& GrantedLevel = RedirectedPandoras.FindOrAdd(PandoraId);
			GrantedLevel = FMath::Max(GrantedLevel, FMath::Max(PandoraPair.Value, 1));
			bOutChanged |= PandoraId != PandoraPair.Key || PandoraPair.Value < 1;
		}
	}
	bOutChanged |= RedirectedPandoras.Num() != OriginalPandoraCount;
	PandoraData.GrantedPandorasById = MoveTemp(RedirectedPandoras);

	const FPrimaryAssetId RedirectedSelectedPandoraId =
		ResolveRedirectedAssetId(PandoraData.SelectedPandoraId);
	if (RedirectedSelectedPandoraId != PandoraData.SelectedPandoraId)
	{
		PandoraData.SelectedPandoraId = RedirectedSelectedPandoraId;
		bOutChanged = true;
	}

	const int32 OriginalLoadoutCount = PandoraData.PandoraLoadoutByDirectionId.Num();
	TMap<FName, FPrimaryAssetId> RedirectedPandoraLoadout;
	for (const TPair<FName, FPrimaryAssetId>& LoadoutPair : PandoraData.PandoraLoadoutByDirectionId)
	{
		const FPrimaryAssetId PandoraId = ResolveRedirectedAssetId(LoadoutPair.Value);
		if (!LoadoutPair.Key.IsNone() && PandoraId.IsValid())
		{
			RedirectedPandoraLoadout.Add(LoadoutPair.Key, PandoraId);
			bOutChanged |= PandoraId != LoadoutPair.Value;
		}
	}
	bOutChanged |= RedirectedPandoraLoadout.Num() != OriginalLoadoutCount;
	PandoraData.PandoraLoadoutByDirectionId = MoveTemp(RedirectedPandoraLoadout);

	const int32 OriginalSkinCount = SkinData.GrantedSkinsById.Num();
	TMap<FPrimaryAssetId, int32> RedirectedSkins;
	for (const TPair<FPrimaryAssetId, int32>& SkinPair : SkinData.GrantedSkinsById)
	{
		const FPrimaryAssetId SkinId = ResolveRedirectedAssetId(SkinPair.Key);
		if (SkinId.IsValid())
		{
			int32& GrantedValue = RedirectedSkins.FindOrAdd(SkinId);
			GrantedValue = FMath::Max(GrantedValue, FMath::Max(SkinPair.Value, 1));
			bOutChanged |= SkinId != SkinPair.Key || SkinPair.Value < 1;
		}
	}
	bOutChanged |= RedirectedSkins.Num() != OriginalSkinCount;
	SkinData.GrantedSkinsById = MoveTemp(RedirectedSkins);

	if (!PandoraData.GrantedPandorasByName.IsEmpty()
		|| !PandoraData.SelectedPandoraName.IsNone()
		|| !PandoraData.PandoraLoadoutByDirection.IsEmpty()
		|| !SkinData.GrantedSkinsByName.IsEmpty())
	{
		PandoraData.GrantedPandorasByName.Reset();
		PandoraData.SelectedPandoraName = NAME_None;
		PandoraData.PandoraLoadoutByDirection.Reset();
		SkinData.GrantedSkinsByName.Reset();
		bOutChanged = true;
	}

	if (SaveGameObject.SaveSchemaVersion != PdSaveGameSchema::Current)
	{
		SaveGameObject.SaveSchemaVersion = PdSaveGameSchema::Current;
		bOutChanged = true;
	}

	return true;
}

void UPlayerProfileSubsystem::RequestProfileSave(
	const FString& PlayerId,
	const bool bSaveImmediately)
{
	if (bIsDeinitializing)
	{
		return;
	}

	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	UPdSaveGame* SaveGameObject = SavedGameByPlayerId.FindRef(TrimmedPlayerId);
	if (TrimmedPlayerId.IsEmpty() || !IsValid(SaveGameObject))
	{
		return;
	}

	SaveGameObject->SaveSchemaVersion = PdSaveGameSchema::Current;
	SaveGameObject->SaveRevision = FMath::Max<int64>(SaveGameObject->SaveRevision, 0) + 1;
	DirtyPlayerIds.Add(TrimmedPlayerId);
	SaveRetryCountsByPlayerId.Remove(TrimmedPlayerId);
	SaveDeadlinesByPlayerId.Add(
		TrimmedPlayerId,
		FPlatformTime::Seconds()
			+ (bSaveImmediately ? 0.0 : FMath::Max(Settings.SaveDebounceSeconds, 0.05f)));
	EnsureSaveTicker();
}

void UPlayerProfileSubsystem::EnsureSaveTicker()
{
	if (bIsDeinitializing || SaveTickerHandle.IsValid())
	{
		return;
	}

	SaveTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickPendingSaves),
		FMath::Max(Settings.SaveTickerIntervalSeconds, 0.05f));
}

bool UPlayerProfileSubsystem::TickPendingSaves(float)
{
	if (bIsDeinitializing)
	{
		SaveTickerHandle.Reset();
		return false;
	}

	const double Now = FPlatformTime::Seconds();
	TArray<FString> ReadyPlayerIds;
	for (const FString& PlayerId : DirtyPlayerIds)
	{
		const double Deadline = SaveDeadlinesByPlayerId.FindRef(PlayerId);
		if (Deadline <= Now && !SaveInFlightPlayerIds.Contains(PlayerId))
		{
			ReadyPlayerIds.Add(PlayerId);
		}
	}

	for (const FString& PlayerId : ReadyPlayerIds)
	{
		BeginAsyncSave(PlayerId);
	}

	if (DirtyPlayerIds.IsEmpty())
	{
		SaveTickerHandle.Reset();
		return false;
	}

	return true;
}

void UPlayerProfileSubsystem::BeginAsyncSave(const FString& PlayerId)
{
	UPdSaveGame* LiveSaveGame = SavedGameByPlayerId.FindRef(PlayerId);
	if (!IsValid(LiveSaveGame) || SaveInFlightPlayerIds.Contains(PlayerId))
	{
		return;
	}

	UPdProfileSaveEnvelope* Snapshot =
		UPdProfileSaveEnvelope::CreateFromProfile(
			LiveSaveGame,
			PlayerId,
			this);
	if (!IsValid(Snapshot))
	{
		UE_LOG(
			LogPlayerProfile,
			Error,
			TEXT("Failed to prepare obfuscated profile '%s' for saving."),
			*PlayerId);

		int32& RetryCount = SaveRetryCountsByPlayerId.FindOrAdd(PlayerId);
		++RetryCount;
		SaveDeadlinesByPlayerId.Add(
			PlayerId,
			RetryCount <= FMath::Max(Settings.MaxSaveRetryAttempts, 0)
				? FPlatformTime::Seconds()
					+ FMath::Max(Settings.SaveRetryDelaySeconds, 0.1f)
				: TNumericLimits<double>::Max());
		return;
	}

	DirtyPlayerIds.Remove(PlayerId);
	SaveDeadlinesByPlayerId.Remove(PlayerId);
	SaveInFlightPlayerIds.Add(PlayerId);
	SaveSnapshotsByPlayerId.Add(PlayerId, Snapshot);

	UGameplayStatics::AsyncSaveGameToSlot(
		Snapshot,
		PlayerId,
		0,
		FAsyncSaveGameToSlotDelegate::CreateWeakLambda(
			this,
			[this, PlayerId](const FString&, const int32, const bool bSucceeded)
			{
				HandlePrimarySaveCompleted(PlayerId, bSucceeded);
			}));
}

void UPlayerProfileSubsystem::HandlePrimarySaveCompleted(
	const FString& PlayerId,
	const bool bSucceeded)
{
	if (bIsDeinitializing)
	{
		return;
	}

	if (!bSucceeded)
	{
		UE_LOG(LogPlayerProfile, Error, TEXT("Failed to save profile '%s'."), *PlayerId);
		FinishAsyncSave(PlayerId);

		int32& RetryCount = SaveRetryCountsByPlayerId.FindOrAdd(PlayerId);
		++RetryCount;
		if (RetryCount <= FMath::Max(Settings.MaxSaveRetryAttempts, 0))
		{
			DirtyPlayerIds.Add(PlayerId);
			SaveDeadlinesByPlayerId.Add(
				PlayerId,
				FPlatformTime::Seconds()
					+ FMath::Max(Settings.SaveRetryDelaySeconds, 0.1f));
			EnsureSaveTicker();
		}
		else
		{
			// Keep the newest in-memory state dirty so shutdown recovery can still preserve it.
			DirtyPlayerIds.Add(PlayerId);
			SaveDeadlinesByPlayerId.Add(PlayerId, TNumericLimits<double>::Max());
		}
		return;
	}

	USaveGame* Snapshot = SaveSnapshotsByPlayerId.FindRef(PlayerId);
	if (!IsValid(Snapshot))
	{
		FinishAsyncSave(PlayerId);
		return;
	}

	UGameplayStatics::AsyncSaveGameToSlot(
		Snapshot,
		GetBackupSlotName(PlayerId),
		0,
		FAsyncSaveGameToSlotDelegate::CreateWeakLambda(
			this,
			[this, PlayerId](const FString&, const int32, const bool bBackupSucceeded)
			{
				HandleBackupSaveCompleted(PlayerId, bBackupSucceeded);
			}));
}

void UPlayerProfileSubsystem::HandleBackupSaveCompleted(
	const FString& PlayerId,
	const bool bSucceeded)
{
	if (bIsDeinitializing)
	{
		return;
	}

	if (!bSucceeded)
	{
		UE_LOG(
			LogPlayerProfile,
			Warning,
			TEXT("Primary profile '%s' was saved, but its backup write failed."),
			*PlayerId);
	}
	else
	{
		UGameplayStatics::DeleteGameInSlot(GetShutdownSlotName(PlayerId), 0);
	}

	SaveRetryCountsByPlayerId.Remove(PlayerId);
	FinishAsyncSave(PlayerId);
}

void UPlayerProfileSubsystem::FinishAsyncSave(const FString& PlayerId)
{
	SaveInFlightPlayerIds.Remove(PlayerId);
	SaveSnapshotsByPlayerId.Remove(PlayerId);
	if (DirtyPlayerIds.Contains(PlayerId))
	{
		EnsureSaveTicker();
	}
}

void UPlayerProfileSubsystem::FlushPendingSavesToShutdownSlots()
{
	TSet<FString> PendingPlayerIds = DirtyPlayerIds;
	PendingPlayerIds.Append(SaveInFlightPlayerIds);

	for (const FString& PlayerId : PendingPlayerIds)
	{
		UPdSaveGame* SaveGameObject = SavedGameByPlayerId.FindRef(PlayerId);
		if (!IsValid(SaveGameObject))
		{
			continue;
		}

		UPdProfileSaveEnvelope* ShutdownSnapshot =
			UPdProfileSaveEnvelope::CreateFromProfile(
				SaveGameObject,
				PlayerId,
				this);
		if (!IsValid(ShutdownSnapshot))
		{
			UE_LOG(
				LogPlayerProfile,
				Error,
				TEXT("Failed to prepare pending profile '%s' for shutdown recovery."),
				*PlayerId);
			continue;
		}

		if (!UGameplayStatics::SaveGameToSlot(
			ShutdownSnapshot,
			GetShutdownSlotName(PlayerId),
			0))
		{
			UE_LOG(
				LogPlayerProfile,
				Error,
				TEXT("Failed to flush pending profile '%s' to its shutdown recovery slot."),
				*PlayerId);
		}
	}
}

FString UPlayerProfileSubsystem::GetBackupSlotName(const FString& PlayerId)
{
	return PlayerId + TEXT("__ProfileBackup");
}

FString UPlayerProfileSubsystem::GetShutdownSlotName(const FString& PlayerId)
{
	return PlayerId + TEXT("__ProfileShutdown");
}

FString UPlayerProfileSubsystem::ResolveSavePlayerId(
	const APlayerController* PlayerController,
	const APlayerState* PlayerState) const
{
	const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	const FName SubsystemName = OnlineSubsystem ? OnlineSubsystem->GetSubsystemName() : NAME_None;
	const bool bShouldUseOnlineIdentity = OnlineSubsystem && IsSteamSubsystemName(SubsystemName);

	if (bShouldUseOnlineIdentity)
	{
		if (PlayerState)
		{
			const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();
			if (UniqueId.IsValid())
			{
				if (const FUniqueNetIdPtr UniqueNetId = UniqueId.GetUniqueNetId(); UniqueNetId.IsValid())
				{
					return UniqueNetId->ToString();
				}
			}
		}

		if (PlayerController && PlayerController->IsLocalController())
		{
			if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
			{
				const FUniqueNetIdRepl PreferredUniqueId = LocalPlayer->GetPreferredUniqueNetId();
				if (PreferredUniqueId.IsValid())
				{
					if (const FUniqueNetIdPtr PreferredNetId = PreferredUniqueId.GetUniqueNetId(); PreferredNetId.IsValid())
					{
						return PreferredNetId->ToString();
					}
				}
			}
		}
	}

	if (PlayerController && PlayerController->IsLocalController())
	{
		if (!PreferredSavePlayerId.IsEmpty())
		{
			return PreferredSavePlayerId;
		}

		return GetLocalClientSavePlayerId();
	}

	return FString();
}

FString UPlayerProfileSubsystem::GetLocalClientSavePlayerId() const
{
	return GetLocalProfileSaveSlotName();
}

void UPlayerProfileSubsystem::SetPreferredSavePlayerId(const FString& PlayerId)
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty())
	{
		return;
	}

	PreferredSavePlayerId = TrimmedPlayerId;
}

void UPlayerProfileSubsystem::AddMatchRecord(
	const FString& PlayerId,
	const FPdMatchRecord& MatchRecord,
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

	FPdMatchRecord SanitizedRecord = MatchRecord;
	SanitizedRecord.KillCount = FMath::Max(SanitizedRecord.KillCount, 0);
	SanitizedRecord.DeathCount = FMath::Max(SanitizedRecord.DeathCount, 0);
	SanitizedRecord.Reward = FMath::Max(SanitizedRecord.Reward, 0);

	SaveGameObject->MatchRecords.Add(SanitizedRecord);
	SaveGameObject->MatchPlayedCount = FMath::Max(SaveGameObject->MatchPlayedCount, 0) + 1;
	SaveGameObject->TotalKillCount += SanitizedRecord.KillCount;
	SaveGameObject->TotalDeathCount += SanitizedRecord.DeathCount;
	SaveGameObject->TotalRewardGold += SanitizedRecord.Reward;
	if (SanitizedRecord.bWin)
	{
		SaveGameObject->WinCount = FMath::Max(SaveGameObject->WinCount, 0) + 1;
	}

	while (SaveGameObject->MatchRecords.Num() > MaxSavedMatchRecordCount)
	{
		SaveGameObject->MatchRecords.RemoveAt(0);
	}

	RequestProfileSave(TrimmedPlayerId, bSaveImmediately);

	NotifyProfileProgressChanged(TrimmedPlayerId);
}

TArray<FPdMatchRecord> UPlayerProfileSubsystem::GetMatchRecords(const FString& PlayerId)
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty())
	{
		return {};
	}

	const UPdSaveGame* SaveGameObject = GetOrCreateSaveGame(TrimmedPlayerId);
	return IsValid(SaveGameObject) ? SaveGameObject->MatchRecords : TArray<FPdMatchRecord>();
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

	SaveGameObject->ItemCollectedCount = FMath::Max(SaveGameObject->ItemCollectedCount, 0) + Amount;

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

	SaveGameObject->Gold = FMath::Max(0, SaveGameObject->Gold + Amount);
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
	SaveGameObject->PlayerPandoraData.GrantedPandorasByName.Reset();
	SaveGameObject->PlayerPandoraData.SelectedPandoraName = NAME_None;
	SaveGameObject->PlayerPandoraData.PandoraLoadoutByDirection.Reset();
	SaveGameObject->PlayerSkinData.GrantedSkinsById.Reset();
	SaveGameObject->PlayerSkinData.GrantedSkinsByName.Reset();
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
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (!TrimmedPlayerId.IsEmpty())
	{
		ProfileProgressChanged.Broadcast(TrimmedPlayerId);
	}
}

void UPlayerProfileSubsystem::TryImportLocalProfileSave(const FString& TargetPlayerId, UPdSaveGame* TargetSaveGame)
{
	if (!IsValid(TargetSaveGame)
		|| TargetPlayerId.IsEmpty()
		|| TargetPlayerId.Equals(GetLocalProfileSaveSlotName(), ESearchCase::CaseSensitive)
		|| !IsCurrentLocalOnlineProfileId(TargetPlayerId)
		|| TargetSaveGame->ImportedLegacySaveSlots.Contains(GetLocalProfileSaveSlotName()))
	{
		return;
	}

	UPdSaveGame* LocalProfileSaveGame = SavedGameByPlayerId.FindRef(GetLocalProfileSaveSlotName());
	if (!IsValid(LocalProfileSaveGame))
	{
		bool bRecoveredFromFallback = false;
		bool bRequiresStorageMigration = false;
		LocalProfileSaveGame =
			LoadBestAvailableSaveGame(
				GetLocalProfileSaveSlotName(),
				bRecoveredFromFallback,
				bRequiresStorageMigration);
		if (IsValid(LocalProfileSaveGame))
		{
			bool bSchemaChanged = false;
			if (!UpgradeSaveGameSchema(
				*LocalProfileSaveGame,
				GetLocalProfileSaveSlotName(),
				bSchemaChanged))
			{
				LocalProfileSaveGame = nullptr;
			}
			else
			{
				const bool bDefaultsChanged = EnsureDefaultUnlockedSkins(*LocalProfileSaveGame);
				SavedGameByPlayerId.Add(GetLocalProfileSaveSlotName(), LocalProfileSaveGame);
				if (bRecoveredFromFallback
					|| bRequiresStorageMigration
					|| bSchemaChanged
					|| bDefaultsChanged)
				{
					RequestProfileSave(
						GetLocalProfileSaveSlotName(),
						bRecoveredFromFallback || bRequiresStorageMigration);
				}
			}
		}
	}

	if (!HasProfileProgress(LocalProfileSaveGame))
	{
		return;
	}

	MergeProfileSaveData(*TargetSaveGame, *LocalProfileSaveGame);
	TargetSaveGame->ImportedLegacySaveSlots.AddUnique(GetLocalProfileSaveSlotName());
	RequestProfileSave(TargetPlayerId, true);
}

UPdSaveGame* UPlayerProfileSubsystem::CreateConfiguredSaveGameObject() const
{
	UClass* ClassToCreate = Settings.SaveGameClass.Get();
	if (!ClassToCreate)
	{
		ClassToCreate = UPdSaveGame::StaticClass();
	}

	UPdSaveGame* SaveGameObject =
		Cast<UPdSaveGame>(UGameplayStatics::CreateSaveGameObject(ClassToCreate));
	if (SaveGameObject)
	{
		SaveGameObject->SaveSchemaVersion = PdSaveGameSchema::Current;
		SaveGameObject->SaveId = FGuid::NewGuid();
		SaveGameObject->SaveRevision = 0;
	}
	return SaveGameObject;
}
