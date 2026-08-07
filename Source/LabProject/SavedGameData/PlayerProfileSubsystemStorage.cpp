#include "SavedGameData/PlayerProfileSubsystem.h"
#include "SavedGameData/PlayerProfilePolicy.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerProfile, Log, All);

using namespace PlayerProfilePolicy;

namespace
{
	constexpr double SaveDebounceSeconds = 0.75;
	constexpr float SaveTickerIntervalSeconds = 0.10f;
	constexpr double SaveRetryDelaySeconds = 2.0;
	constexpr int32 MaxSaveRetryAttempts = 3;
}

void UPlayerProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsDeinitializing = false;
	PreferredSavePlayerId = GetLocalProfileSaveSlotName();
	UE_LOG(
		LogPlayerProfile,
		Log,
		TEXT("[ProfileIdentity] Profile persistence uses the shared machine slot '%s'; online account IDs are ignored."),
		*PreferredSavePlayerId);
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

	if (GetOrCreateSaveGame(PlayerId))
	{
		RequestProfileSave(PlayerId, true);
	}
}

UPdSaveGame* UPlayerProfileSubsystem::GetOrCreateSaveGame(const FString& PlayerId)
{
	const FString TrimmedPlayerId = NormalizeProfilePlayerId(PlayerId);
	if (TrimmedPlayerId.IsEmpty())
	{
		UE_LOG(
			LogPlayerProfile,
			Warning,
			TEXT("[SaveGameLoad] Rejected an empty PlayerId (raw value='%s')."),
			*PlayerId);
		return nullptr;
	}

	if (UPdSaveGame* ExistingSaveGame = SavedGameByPlayerId.FindRef(TrimmedPlayerId))
	{
		UE_LOG(
			LogPlayerProfile,
			Verbose,
			TEXT("[SaveGameLoad] Using cached profile. PlayerId='%s', Object='%s', "
				"SaveId='%s', Revision=%lld, DataVersion=%d."),
			*TrimmedPlayerId,
			*GetNameSafe(ExistingSaveGame),
			*ExistingSaveGame->SaveId.ToString(),
			ExistingSaveGame->SaveRevision,
			ExistingSaveGame->ProfileDataVersion);

		if (EnsureDefaultUnlockedSkins(*ExistingSaveGame))
		{
			RequestProfileSave(TrimmedPlayerId, false);
		}
		return ExistingSaveGame;
	}

	bool bRecoveredFromFallback = false;
	UE_LOG(
		LogPlayerProfile,
		Log,
		TEXT("[SaveGameLoad] No cached profile. Scanning primary, backup, and shutdown slots for '%s'."),
		*TrimmedPlayerId);

	UPdSaveGame* SaveGameObject =
		LoadBestAvailableSaveGame(
			TrimmedPlayerId,
			bRecoveredFromFallback);
	const bool bCreatedNewProfile = !IsValid(SaveGameObject);
	if (!SaveGameObject)
	{
		UE_LOG(
			LogPlayerProfile,
			Warning,
			TEXT("[SaveGameLoad] No valid on-disk profile was found for '%s'. Creating a new profile."),
			*TrimmedPlayerId);
		SaveGameObject = CreateConfiguredSaveGameObject();
	}

	if (!IsValid(SaveGameObject))
	{
		UE_LOG(
			LogPlayerProfile,
			Error,
			TEXT("[SaveGameLoad] Failed to create a profile object for '%s'."),
			*TrimmedPlayerId);
		return nullptr;
	}

	const bool bDefaultsChanged = EnsureDefaultUnlockedSkins(*SaveGameObject);
	SavedGameByPlayerId.Add(TrimmedPlayerId, SaveGameObject);
	if (bCreatedNewProfile || bRecoveredFromFallback || bDefaultsChanged)
	{
		RequestProfileSave(
			TrimmedPlayerId,
			bCreatedNewProfile || bRecoveredFromFallback);
	}

	UE_LOG(
		LogPlayerProfile,
		Log,
		TEXT("[SaveGameLoad] Profile is ready. PlayerId='%s', Source=%s, "
			"RecoveredFromFallback=%s, DefaultsChanged=%s, SaveId='%s', Revision=%lld, DataVersion=%d."),
		*TrimmedPlayerId,
		bCreatedNewProfile ? TEXT("NewProfile") : TEXT("Disk"),
		bRecoveredFromFallback ? TEXT("true") : TEXT("false"),
		bDefaultsChanged ? TEXT("true") : TEXT("false"),
		*SaveGameObject->SaveId.ToString(),
		SaveGameObject->SaveRevision,
		SaveGameObject->ProfileDataVersion);

	return SaveGameObject;
}

UPdSaveGame* UPlayerProfileSubsystem::LoadBestAvailableSaveGame(
	const FString& PlayerId,
	bool& bOutRecoveredFromFallback) const
{
	bOutRecoveredFromFallback = false;

	const TArray<FString> CandidateSlotNames = {
		PlayerId,
		GetBackupSlotName(PlayerId),
		GetShutdownSlotName(PlayerId)
	};

	UPdSaveGame* BestSaveGame = nullptr;
	FString BestSlotName;
	for (const FString& CandidateSlotName : CandidateSlotNames)
	{
		const bool bSlotExists =
			UGameplayStatics::DoesSaveGameExist(CandidateSlotName, 0);
		UE_LOG(
			LogPlayerProfile,
			Log,
			TEXT("[SaveGameLoad] Checking slot '%s' (UserIndex=0): Exists=%s."),
			*CandidateSlotName,
			bSlotExists ? TEXT("true") : TEXT("false"));

		if (!bSlotExists)
		{
			continue;
		}

		USaveGame* StoredSaveGame =
			UGameplayStatics::LoadGameFromSlot(CandidateSlotName, 0);
		UE_LOG(
			LogPlayerProfile,
			Log,
			TEXT("[SaveGameLoad] Slot '%s' deserialized to Object='%s', Class='%s'."),
			*CandidateSlotName,
			*GetNameSafe(StoredSaveGame),
			StoredSaveGame ? *GetNameSafe(StoredSaveGame->GetClass()) : TEXT("None"));

		if (!StoredSaveGame
			|| StoredSaveGame->GetClass() != UProfileSaveEnvelope::StaticClass())
		{
			UE_LOG(
				LogPlayerProfile,
				Warning,
				TEXT("[SaveGameLoad] Rejected slot '%s': expected the current UProfileSaveEnvelope format, got '%s'."),
				*CandidateSlotName,
				StoredSaveGame ? *GetNameSafe(StoredSaveGame->GetClass()) : TEXT("None"));
			continue;
		}

		UProfileSaveEnvelope* Envelope =
			CastChecked<UProfileSaveEnvelope>(StoredSaveGame);
		UE_LOG(
			LogPlayerProfile,
			Log,
			TEXT("[SaveGameLoad] Decoding envelope in slot '%s': StorageVersion=%d, PayloadBytes=%d, HeaderSaveId='%s', HeaderRevision=%lld."),
			*CandidateSlotName,
			Envelope->GetStorageFormatVersion(),
			Envelope->GetObfuscatedPayloadSize(),
			*Envelope->GetProfileSaveId().ToString(),
			Envelope->GetProfileRevision());
		UPdSaveGame* CandidateSaveGame = Envelope->DecodeProfile(PlayerId);

		if (!CandidateSaveGame)
		{
			UE_LOG(
				LogPlayerProfile,
				Warning,
				TEXT("Profile save slot '%s' exists but could not be loaded."),
				*CandidateSlotName);
			continue;
		}

		UE_LOG(
			LogPlayerProfile,
			Log,
			TEXT("[SaveGameLoad] Valid candidate '%s': SaveId='%s', Revision=%lld, "
				"DataVersion=%d, Gold=%d, Pandoras=%d, Skins=%d, Records=%d."),
			*CandidateSlotName,
			*CandidateSaveGame->SaveId.ToString(),
			CandidateSaveGame->SaveRevision,
			CandidateSaveGame->ProfileDataVersion,
			CandidateSaveGame->Gold,
			CandidateSaveGame->PlayerPandoraData.GrantedPandorasById.Num(),
			CandidateSaveGame->PlayerSkinData.GrantedSkinsById.Num(),
			CandidateSaveGame->MatchRecords.Num());

		if (!BestSaveGame || CandidateSaveGame->SaveRevision > BestSaveGame->SaveRevision)
		{
			UE_LOG(
				LogPlayerProfile,
				Log,
				TEXT("[SaveGameLoad] Slot '%s' is now the best candidate (Revision=%lld)."),
				*CandidateSlotName,
				CandidateSaveGame->SaveRevision);
			BestSaveGame = CandidateSaveGame;
			BestSlotName = CandidateSlotName;
		}
		else if (CandidateSaveGame->SaveRevision == BestSaveGame->SaveRevision
			&& CandidateSaveGame->SaveId != BestSaveGame->SaveId)
		{
			UE_LOG(
				LogPlayerProfile,
				Warning,
				TEXT("[SaveGameLoad] Slot '%s' has Revision=%lld but a different SaveId than the preferred slot '%s'. Keeping the earlier slot by priority."),
				*CandidateSlotName,
				CandidateSaveGame->SaveRevision,
				*BestSlotName);
		}
	}

	bOutRecoveredFromFallback =
		BestSaveGame && !BestSlotName.Equals(PlayerId, ESearchCase::CaseSensitive);

	if (BestSaveGame)
	{
		UE_LOG(
			LogPlayerProfile,
			Log,
			TEXT("[SaveGameLoad] Selected slot '%s' for PlayerId='%s' (Revision=%lld, Fallback=%s)."),
			*BestSlotName,
			*PlayerId,
			BestSaveGame->SaveRevision,
			bOutRecoveredFromFallback ? TEXT("true") : TEXT("false"));
	}
	else
	{
		UE_LOG(
			LogPlayerProfile,
			Warning,
			TEXT("[SaveGameLoad] No loadable slot was found for PlayerId='%s'."),
			*PlayerId);
	}

	return BestSaveGame;
}

void UPlayerProfileSubsystem::RequestProfileSave(
	const FString& PlayerId,
	const bool bSaveImmediately)
{
	if (bIsDeinitializing)
	{
		return;
	}

	const FString TrimmedPlayerId = NormalizeProfilePlayerId(PlayerId);
	UPdSaveGame* SaveGameObject = SavedGameByPlayerId.FindRef(TrimmedPlayerId);
	if (TrimmedPlayerId.IsEmpty() || !IsValid(SaveGameObject))
	{
		return;
	}

	if (SaveGameObject->SaveRevision == TNumericLimits<int64>::Max())
	{
		UE_LOG(
			LogPlayerProfile,
			Error,
			TEXT("[SaveGameWrite] Refused to increment the exhausted revision for profile '%s'."),
			*TrimmedPlayerId);
		return;
	}
	SaveGameObject->SaveRevision =
		FMath::Max<int64>(SaveGameObject->SaveRevision, 0) + 1;
	DirtyPlayerIds.Add(TrimmedPlayerId);
	SaveRetryCountsByPlayerId.Remove(TrimmedPlayerId);
	SaveDeadlinesByPlayerId.Add(
		TrimmedPlayerId,
		FPlatformTime::Seconds()
			+ (bSaveImmediately ? 0.0 : SaveDebounceSeconds));
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
		SaveTickerIntervalSeconds);
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

	UProfileSaveEnvelope* Snapshot =
		UProfileSaveEnvelope::CreateFromProfile(
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
			RetryCount <= MaxSaveRetryAttempts
				? FPlatformTime::Seconds()
					+ SaveRetryDelaySeconds
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
		if (RetryCount <= MaxSaveRetryAttempts)
		{
			DirtyPlayerIds.Add(PlayerId);
			SaveDeadlinesByPlayerId.Add(
				PlayerId,
				FPlatformTime::Seconds()
					+ SaveRetryDelaySeconds);
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

		FinishAsyncSave(PlayerId);
		int32& RetryCount = SaveRetryCountsByPlayerId.FindOrAdd(PlayerId);
		++RetryCount;
		DirtyPlayerIds.Add(PlayerId);
		SaveDeadlinesByPlayerId.Add(
			PlayerId,
			RetryCount <= MaxSaveRetryAttempts
				? FPlatformTime::Seconds()
					+ SaveRetryDelaySeconds
				: TNumericLimits<double>::Max());
		EnsureSaveTicker();
		return;
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

		UProfileSaveEnvelope* ShutdownSnapshot =
			UProfileSaveEnvelope::CreateFromProfile(
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
	static_cast<void>(PlayerState);

	if (PlayerController && PlayerController->IsLocalController())
	{
		return GetLocalClientSavePlayerId();
	}

	return FString();
}

FString UPlayerProfileSubsystem::NormalizeProfilePlayerId(const FString& PlayerId) const
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	return TrimmedPlayerId.IsEmpty()
		? FString()
		: GetLocalProfileSaveSlotName();
}

FString UPlayerProfileSubsystem::GetLocalClientSavePlayerId() const
{
	return GetLocalProfileSaveSlotName();
}

void UPlayerProfileSubsystem::SetPreferredSavePlayerId(const FString& PlayerId)
{
	const FString NormalizedPlayerId = NormalizeProfilePlayerId(PlayerId);
	if (NormalizedPlayerId.IsEmpty())
	{
		return;
	}

	PreferredSavePlayerId = NormalizedPlayerId;
}
