#include "Profile/PlayerProfileSubsystem.h"
#include "Profile/PlayerProfileSaveGame.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlayerProfile, Log, All);

namespace
{
	const FString LocalProfileSlot(TEXT("LocalProfile"));
	const FString BackupSlot(TEXT("LocalProfile__ProfileBackup"));
	const FString ShutdownSlot(TEXT("LocalProfile__ProfileShutdown"));
	constexpr double SaveDebounceSeconds = 0.75;
	constexpr float SaveTickerIntervalSeconds = 0.10f;
	constexpr double SaveRetryDelaySeconds = 2.0;
	constexpr int32 MaxSaveRetryAttempts = 3;
}

void UPlayerProfileSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsDeinitializing = false;
}

void UPlayerProfileSubsystem::Deinitialize()
{
	bIsDeinitializing = true;
	FTSTicker::RemoveTicker(SaveTickerHandle);
	SaveTickerHandle.Reset();
	FlushPendingSaveToShutdownSlot();
	bProfileDirty = false;
	Super::Deinitialize();
}

bool UPlayerProfileSubsystem::LoadProfile()
{
	if (!GetOrCreateProfile()) return false;
	ProfileProgressChanged.Broadcast();
	return true;
}

void UPlayerProfileSubsystem::SaveProfile()
{
	if (GetOrCreateProfile()) RequestProfileSave(true);
}

UPdSaveGame* UPlayerProfileSubsystem::GetOrCreateProfile()
{
	if (GetGameInstance()->IsDedicatedServerInstance()) return nullptr;
	if (ProfileSaveGame)
	{
		if (EnsureDefaultUnlockedSkins(*ProfileSaveGame)) RequestProfileSave(false);
		return ProfileSaveGame;
	}

	bool bRecoveredFromFallback = false;
	ProfileSaveGame = LoadBestAvailableSaveGame(bRecoveredFromFallback);
	const bool bCreatedNewProfile = !ProfileSaveGame;
	if (bCreatedNewProfile) ProfileSaveGame = CreateConfiguredSaveGameObject();
	if (!ProfileSaveGame) return nullptr;

	const bool bDefaultsChanged = EnsureDefaultUnlockedSkins(*ProfileSaveGame);
	if (bCreatedNewProfile || bRecoveredFromFallback || bDefaultsChanged)
		RequestProfileSave(bCreatedNewProfile || bRecoveredFromFallback);
	return ProfileSaveGame;
}

UPdSaveGame* UPlayerProfileSubsystem::LoadBestAvailableSaveGame(bool& bOutRecoveredFromFallback) const
{
	UPdSaveGame* BestProfile = nullptr;
	FString BestSlot;
	for (const FString& Slot : {LocalProfileSlot, BackupSlot, ShutdownSlot})
	{
		if (!UGameplayStatics::DoesSaveGameExist(Slot, 0)) continue;
		UProfileSaveEnvelope* Envelope = Cast<UProfileSaveEnvelope>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		UPdSaveGame* Candidate = Envelope ? Envelope->DecodeProfile() : nullptr;
		if (!Candidate)
		{
			UE_LOG(LogPlayerProfile, Warning, TEXT("Rejected invalid profile slot '%s'."), *Slot);
			continue;
		}
		if (!BestProfile || Candidate->SaveRevision > BestProfile->SaveRevision)
		{
			BestProfile = Candidate;
			BestSlot = Slot;
		}
		else if (Candidate->SaveRevision == BestProfile->SaveRevision && Candidate->SaveId != BestProfile->SaveId)
		{
			UE_LOG(LogPlayerProfile, Warning, TEXT("Profile '%s' has a conflicting SaveId at revision %lld; keeping '%s'."), *Slot, Candidate->SaveRevision, *BestSlot);
		}
	}
	bOutRecoveredFromFallback = BestProfile && BestSlot != LocalProfileSlot;
	if (BestProfile)
	{
		UE_LOG(LogPlayerProfile, Log, TEXT("Loaded profile from '%s', revision %lld, recovery=%d."), *BestSlot, BestProfile->SaveRevision, bOutRecoveredFromFallback);
	}
	else
	{
		UE_LOG(LogPlayerProfile, Warning, TEXT("No valid profile found; creating a new LocalProfile."));
	}
	return BestProfile;
}

void UPlayerProfileSubsystem::RequestProfileSave(bool bSaveImmediately)
{
	if (bIsDeinitializing || !ProfileSaveGame) return;
	if (ProfileSaveGame->SaveRevision == TNumericLimits<int64>::Max())
	{
		UE_LOG(LogPlayerProfile, Error, TEXT("Profile revision is exhausted; refusing to save."));
		return;
	}
	ProfileSaveGame->SaveRevision = FMath::Max<int64>(ProfileSaveGame->SaveRevision, 0) + 1;
	bProfileDirty = true;
	SaveRetryCount = 0;
	SaveDeadline = FPlatformTime::Seconds() + (bSaveImmediately ? 0.0 : SaveDebounceSeconds);
	EnsureSaveTicker();
}

void UPlayerProfileSubsystem::EnsureSaveTicker()
{
	if (!bIsDeinitializing && !SaveTickerHandle.IsValid())
		SaveTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickPendingSaves), SaveTickerIntervalSeconds);
}

bool UPlayerProfileSubsystem::TickPendingSaves(float)
{
	if (!bIsDeinitializing && bProfileDirty && !bSaveInFlight && SaveDeadline <= FPlatformTime::Seconds()) BeginAsyncSave();
	if (bIsDeinitializing || !bProfileDirty || SaveDeadline == TNumericLimits<double>::Max())
	{
		SaveTickerHandle.Reset();
		return false;
	}
	return true;
}

void UPlayerProfileSubsystem::BeginAsyncSave()
{
	if (!ProfileSaveGame || bSaveInFlight) return;
	SaveSnapshot = UProfileSaveEnvelope::CreateFromProfile(ProfileSaveGame, this);
	if (!SaveSnapshot)
	{
		UE_LOG(LogPlayerProfile, Error, TEXT("Failed to prepare profile snapshot."));
		ScheduleSaveRetry();
		return;
	}
	bProfileDirty = false;
	bSaveInFlight = true;
	UGameplayStatics::AsyncSaveGameToSlot(SaveSnapshot, LocalProfileSlot, 0,
		FAsyncSaveGameToSlotDelegate::CreateWeakLambda(this, [this](const FString&, int32, bool bSucceeded)
		{
			HandlePrimarySaveCompleted(bSucceeded);
		}));
}

void UPlayerProfileSubsystem::HandlePrimarySaveCompleted(bool bSucceeded)
{
	if (bIsDeinitializing) return;
	if (!bSucceeded)
	{
		UE_LOG(LogPlayerProfile, Error, TEXT("Failed to save primary profile."));
		FinishAsyncSave();
		ScheduleSaveRetry();
		return;
	}
	UGameplayStatics::AsyncSaveGameToSlot(SaveSnapshot, BackupSlot, 0,
		FAsyncSaveGameToSlotDelegate::CreateWeakLambda(this, [this](const FString&, int32, bool bBackupSucceeded)
		{
			HandleBackupSaveCompleted(bBackupSucceeded);
		}));
}

void UPlayerProfileSubsystem::HandleBackupSaveCompleted(bool bSucceeded)
{
	if (bIsDeinitializing) return;
	if (!bSucceeded)
	{
		UE_LOG(LogPlayerProfile, Warning, TEXT("Primary profile saved, but backup write failed."));
		FinishAsyncSave();
		ScheduleSaveRetry();
		return;
	}
	UGameplayStatics::DeleteGameInSlot(ShutdownSlot, 0);
	SaveRetryCount = 0;
	FinishAsyncSave();
}

void UPlayerProfileSubsystem::ScheduleSaveRetry()
{
	bProfileDirty = true;
	++SaveRetryCount;
	SaveDeadline = SaveRetryCount <= MaxSaveRetryAttempts ? FPlatformTime::Seconds() + SaveRetryDelaySeconds : TNumericLimits<double>::Max();
	if (SaveRetryCount > MaxSaveRetryAttempts)
		UE_LOG(LogPlayerProfile, Error, TEXT("Profile save retries exhausted; keeping live data for shutdown recovery."));
	EnsureSaveTicker();
}

void UPlayerProfileSubsystem::FinishAsyncSave()
{
	bSaveInFlight = false;
	SaveSnapshot = nullptr;
	if (bProfileDirty) EnsureSaveTicker();
}

void UPlayerProfileSubsystem::FlushPendingSaveToShutdownSlot()
{
	if ((!bProfileDirty && !bSaveInFlight) || !ProfileSaveGame) return;
	UProfileSaveEnvelope* Snapshot = UProfileSaveEnvelope::CreateFromProfile(ProfileSaveGame, this);
	if (!Snapshot || !UGameplayStatics::SaveGameToSlot(Snapshot, ShutdownSlot, 0))
		UE_LOG(LogPlayerProfile, Error, TEXT("Failed to flush live profile to shutdown recovery slot."));
}

UPdSaveGame* UPlayerProfileSubsystem::CreateConfiguredSaveGameObject() const
{
	UPdSaveGame* Profile = NewObject<UPdSaveGame>();
	Profile->SaveId = FGuid::NewGuid();
	return Profile;
}
