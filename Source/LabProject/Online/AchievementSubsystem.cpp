#include "Online/AchievementSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Engine/StreamableManager.h"
#include "SavedGameData/PdSaveGame.h"
#include "SavedGameData/PlayerProfileSubsystem.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

#include UE_INLINE_GENERATED_CPP_BY_NAME(AchievementSubsystem)

void UAchievementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UPlayerProfileSubsystem>();
	Collection.InitializeDependency<UContentDataSubsystem>();
	if (UPlayerProfileSubsystem* ProfileSubsystem =
		GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>())
	{
		ProfileProgressChangedHandle = ProfileSubsystem->OnProfileProgressChanged().AddUObject(
			this,
			&ThisClass::HandleProfileProgressChanged);
	}
	BeginAchievementDefinitionPreload();
}

void UAchievementSubsystem::Deinitialize()
{
	if (ProfileProgressChangedHandle.IsValid())
	{
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (UPlayerProfileSubsystem* ProfileSubsystem =
				GameInstance->GetSubsystem<UPlayerProfileSubsystem>())
			{
				ProfileSubsystem->OnProfileProgressChanged().Remove(ProfileProgressChangedHandle);
			}
		}
		ProfileProgressChangedHandle.Reset();
	}

	PendingAchievementIds.Reset();
	PendingEvaluationPlayerIds.Reset();
	InFlightAchievementIds.Reset();
	LocallyUnlockedAchievementIds.Reset();
	InFlightWriteObjects.Reset();
	bAchievementsQueried = false;
	bAchievementQueryInFlight = false;
	if (DefinitionPreloadHandle.IsValid())
	{
		DefinitionPreloadHandle->CancelHandle();
		DefinitionPreloadHandle->ReleaseHandle();
		DefinitionPreloadHandle.Reset();
	}
	if (PresentationPreloadHandle.IsValid())
	{
		PresentationPreloadHandle->CancelHandle();
		PresentationPreloadHandle->ReleaseHandle();
		PresentationPreloadHandle.Reset();
	}
	CachedAchievementDefinition = nullptr;

	Super::Deinitialize();
}

void UAchievementSubsystem::HandleProfileProgressChanged(const FString& PlayerId)
{
	EvaluateAndUnlockAchievementsForPlayerId(PlayerId);
}

void UAchievementSubsystem::EvaluateAndUnlockAchievementsForPlayerId(const FString& PlayerId)
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty())
	{
		return;
	}

	const UAchievementDefinition* AchievementDefinition = ResolveAchievementDefinition();
	if (!AchievementDefinition)
	{
		PendingEvaluationPlayerIds.Add(TrimmedPlayerId);
		BeginAchievementDefinitionPreload();
		return;
	}
	PendingEvaluationPlayerIds.Remove(TrimmedPlayerId);

	for (const FAchievementEntry& Achievement : AchievementDefinition->Achievements)
	{
		if (!Achievement.bEnabled)
		{
			continue;
		}

		const FString AchievementId = NormalizeAchievementId(Achievement.AchievementId);
		if (AchievementId.IsEmpty())
		{
			continue;
		}

		const int32 RequiredValue = FMath::Max(Achievement.RequiredValue, 1);
		if (CalculateAchievementProgressValue(TrimmedPlayerId, Achievement) >= RequiredValue)
		{
			QueueUnlockAchievement(AchievementId);
		}
	}

	EnsureAchievementsQueried();
	FlushPendingAchievementUnlocks();
}

int32 UAchievementSubsystem::CalculateAchievementProgressValue(
	const FString& PlayerId,
	const FAchievementEntry& Achievement) const
{
	const UPdSaveGame* SaveGame = ResolveSaveGame(PlayerId);
	if (!SaveGame)
	{
		return 0;
	}

	int32 MatchRecordKillCount = 0;
	int32 MatchRecordDeathCount = 0;
	int32 MatchRecordRewardGold = 0;
	int32 MatchRecordWinCount = 0;
	for (const FMatchRecord& MatchRecord : SaveGame->MatchRecords)
	{
		MatchRecordKillCount += FMath::Max(MatchRecord.KillCount, 0);
		MatchRecordDeathCount += FMath::Max(MatchRecord.DeathCount, 0);
		MatchRecordRewardGold += FMath::Max(MatchRecord.Reward, 0);
		if (MatchRecord.bWin)
		{
			++MatchRecordWinCount;
		}
	}

	switch (Achievement.Trigger)
	{
	case EAchievementTrigger::FirstLogin:
		return 1;
	case EAchievementTrigger::MatchPlayed:
		return FMath::Max(SaveGame->MatchPlayedCount, SaveGame->MatchRecords.Num());
	case EAchievementTrigger::WinCount:
		return FMath::Max(SaveGame->WinCount, MatchRecordWinCount);
	case EAchievementTrigger::KillCount:
		return FMath::Max(SaveGame->TotalKillCount, MatchRecordKillCount);
	case EAchievementTrigger::DeathCount:
		return FMath::Max(SaveGame->TotalDeathCount, MatchRecordDeathCount);
	case EAchievementTrigger::RewardGold:
		return FMath::Max3(
			SaveGame->TotalRewardGold,
			FMath::Max(SaveGame->Gold, 0),
			MatchRecordRewardGold);
	case EAchievementTrigger::PandoraUnlocked:
		return SaveGame->PlayerPandoraData.GrantedPandorasById.Num();
	case EAchievementTrigger::SkinUnlocked:
		return SaveGame->PlayerSkinData.GrantedSkinsById.Num();
	case EAchievementTrigger::ItemCollected:
		return FMath::Max(SaveGame->ItemCollectedCount, 0);
	default:
		return 0;
	}
}

const UAchievementDefinition* UAchievementSubsystem::GetAchievementDefinition()
{
	const UAchievementDefinition* AchievementDefinition = ResolveAchievementDefinition();
	if (AchievementDefinition)
	{
		BeginAchievementPresentationPreload();
	}
	return AchievementDefinition;
}

const UAchievementDefinition* UAchievementSubsystem::ResolveAchievementDefinition()
{
	if (CachedAchievementDefinition)
	{
		return CachedAchievementDefinition;
	}

	const TSoftObjectPtr<UAchievementDefinition>& AchievementData =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().Achievement;

	CachedAchievementDefinition = AchievementData.Get();
	if (!CachedAchievementDefinition && AchievementData.IsNull() == false)
	{
		CachedAchievementDefinition = Cast<UAchievementDefinition>(
			AchievementData.ToSoftObjectPath().ResolveObject());
	}
	return CachedAchievementDefinition;
}

void UAchievementSubsystem::BeginAchievementDefinitionPreload()
{
	if (CachedAchievementDefinition || DefinitionPreloadHandle.IsValid())
	{
		return;
	}

	const FSoftObjectPath AchievementPath =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
			.Achievement.ToSoftObjectPath();
	if (AchievementPath.IsNull())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	DefinitionPreloadHandle = ContentSubsystem->PreloadSoftObjectPathsAsync(
		{AchievementPath},
		FSimpleDelegate::CreateUObject(
			this,
			&ThisClass::HandleAchievementDefinitionContentReady));
}

void UAchievementSubsystem::BeginAchievementPresentationPreload()
{
	if (PresentationPreloadHandle.IsValid())
	{
		return;
	}

	const UAchievementDefinition* AchievementDefinition = ResolveAchievementDefinition();
	if (!AchievementDefinition)
	{
		return;
	}

	TArray<FSoftObjectPath> PresentationPaths;
	for (const FAchievementEntry& Achievement : AchievementDefinition->Achievements)
	{
		if (!Achievement.bEnabled || Achievement.UnlockedIcon.IsNull())
		{
			continue;
		}

		PresentationPaths.AddUnique(Achievement.UnlockedIcon.ToSoftObjectPath());
	}

	if (PresentationPaths.IsEmpty())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	PresentationPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(PresentationPaths);
}

void UAchievementSubsystem::HandleAchievementDefinitionContentReady()
{
	if (DefinitionPreloadHandle.IsValid())
	{
		DefinitionPreloadHandle->ReleaseHandle();
		DefinitionPreloadHandle.Reset();
	}
	CachedAchievementDefinition = nullptr;
	if (!ResolveAchievementDefinition())
	{
		return;
	}
	BeginAchievementPresentationPreload();

	TArray<FString> PlayerIdsToEvaluate = PendingEvaluationPlayerIds.Array();
	PendingEvaluationPlayerIds.Reset();
	for (const FString& PlayerId : PlayerIdsToEvaluate)
	{
		EvaluateAndUnlockAchievementsForPlayerId(PlayerId);
	}
}

UPdSaveGame* UAchievementSubsystem::ResolveSaveGame(const FString& PlayerId) const
{
	FString TrimmedPlayerId = PlayerId;
	TrimmedPlayerId.TrimStartAndEndInline();
	if (TrimmedPlayerId.IsEmpty())
	{
		return nullptr;
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UPlayerProfileSubsystem* ProfileSubsystem = GameInstance->GetSubsystem<UPlayerProfileSubsystem>())
		{
			return ProfileSubsystem->GetOrCreateSaveGame(TrimmedPlayerId);
		}
	}

	return nullptr;
}

IOnlineSubsystem* UAchievementSubsystem::ResolveOnlineSubsystem() const
{
	if (const UWorld* World = GetWorld())
	{
		if (IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World))
		{
			return OnlineSubsystem;
		}
	}

	return IOnlineSubsystem::Get();
}

bool UAchievementSubsystem::IsSteamSubsystemActive() const
{
	const IOnlineSubsystem* OnlineSubsystem = ResolveOnlineSubsystem();
	return OnlineSubsystem
		&& OnlineSubsystem->GetSubsystemName().ToString().Equals(TEXT("STEAM"), ESearchCase::IgnoreCase);
}

bool UAchievementSubsystem::TryResolveLocalUniqueNetId(FUniqueNetIdPtr& OutUniqueNetId) const
{
	OutUniqueNetId.Reset();

	const IOnlineSubsystem* OnlineSubsystem = ResolveOnlineSubsystem();
	if (OnlineSubsystem)
	{
		const IOnlineIdentityPtr IdentityInterface = OnlineSubsystem->GetIdentityInterface();
		if (IdentityInterface.IsValid())
		{
			OutUniqueNetId = IdentityInterface->GetUniquePlayerId(0);
			if (OutUniqueNetId.IsValid())
			{
				return true;
			}
		}
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const ULocalPlayer* LocalPlayer = GameInstance->GetFirstGamePlayer())
		{
			const FUniqueNetIdRepl PreferredUniqueId = LocalPlayer->GetPreferredUniqueNetId();
			if (PreferredUniqueId.IsValid())
			{
				OutUniqueNetId = PreferredUniqueId.GetUniqueNetId();
				return OutUniqueNetId.IsValid();
			}
		}
	}

	return false;
}

bool UAchievementSubsystem::EnsureAchievementsQueried()
{
	if (bAchievementsQueried || bAchievementQueryInFlight)
	{
		return true;
	}

	if (!IsSteamSubsystemActive())
	{
		return false;
	}

	IOnlineSubsystem* OnlineSubsystem = ResolveOnlineSubsystem();
	if (!OnlineSubsystem)
	{
		return false;
	}

	IOnlineAchievementsPtr AchievementsInterface = OnlineSubsystem->GetAchievementsInterface();
	if (!AchievementsInterface.IsValid())
	{
		return false;
	}

	FUniqueNetIdPtr LocalUserId;
	if (!TryResolveLocalUniqueNetId(LocalUserId) || !LocalUserId.IsValid())
	{
		return false;
	}

	bAchievementQueryInFlight = true;
	AchievementsInterface->QueryAchievements(
		*LocalUserId,
		FOnQueryAchievementsCompleteDelegate::CreateUObject(this, &ThisClass::HandleAchievementsQueried));

	return true;
}

void UAchievementSubsystem::HandleAchievementsQueried(const FUniqueNetId& PlayerId, const bool bWasSuccessful)
{
	static_cast<void>(PlayerId);

	bAchievementQueryInFlight = false;
	bAchievementsQueried = bWasSuccessful;
	FlushPendingAchievementUnlocks();
}

void UAchievementSubsystem::QueueUnlockAchievement(FString AchievementId)
{
	AchievementId = NormalizeAchievementId(MoveTemp(AchievementId));
	if (AchievementId.IsEmpty()
		|| LocallyUnlockedAchievementIds.Contains(AchievementId)
		|| InFlightAchievementIds.Contains(AchievementId)
		|| IsAchievementAlreadyUnlocked(AchievementId))
	{
		return;
	}

	PendingAchievementIds.Add(MoveTemp(AchievementId));
}

void UAchievementSubsystem::FlushPendingAchievementUnlocks()
{
	if (PendingAchievementIds.IsEmpty())
	{
		return;
	}

	if (bAchievementQueryInFlight && !bAchievementsQueried)
	{
		return;
	}

	TArray<FString> AchievementIds = PendingAchievementIds.Array();
	for (const FString& AchievementId : AchievementIds)
	{
		if (AchievementId.IsEmpty())
		{
			PendingAchievementIds.Remove(AchievementId);
			continue;
		}

		if (LocallyUnlockedAchievementIds.Contains(AchievementId) || IsAchievementAlreadyUnlocked(AchievementId))
		{
			LocallyUnlockedAchievementIds.Add(AchievementId);
			PendingAchievementIds.Remove(AchievementId);
			continue;
		}

		if (bAchievementsQueried && WriteAchievementThroughOnlineSubsystem(AchievementId))
		{
			PendingAchievementIds.Remove(AchievementId);
			continue;
		}

		if (WriteAchievementThroughSteamApi(AchievementId))
		{
			LocallyUnlockedAchievementIds.Add(AchievementId);
			PendingAchievementIds.Remove(AchievementId);
		}
	}
}

bool UAchievementSubsystem::IsAchievementAlreadyUnlocked(const FString& AchievementId) const
{
	if (LocallyUnlockedAchievementIds.Contains(AchievementId))
	{
		return true;
	}

	if (!bAchievementsQueried)
	{
		return false;
	}

	const IOnlineSubsystem* OnlineSubsystem = ResolveOnlineSubsystem();
	if (!OnlineSubsystem)
	{
		return false;
	}

	IOnlineAchievementsPtr AchievementsInterface = OnlineSubsystem->GetAchievementsInterface();
	if (!AchievementsInterface.IsValid())
	{
		return false;
	}

	FUniqueNetIdPtr LocalUserId;
	if (!TryResolveLocalUniqueNetId(LocalUserId) || !LocalUserId.IsValid())
	{
		return false;
	}

	FOnlineAchievement CachedAchievement;
	return AchievementsInterface->GetCachedAchievement(*LocalUserId, AchievementId, CachedAchievement) == EOnlineCachedResult::Success
		&& CachedAchievement.Progress >= 100.0;
}

bool UAchievementSubsystem::WriteAchievementThroughOnlineSubsystem(const FString& AchievementId)
{
	if (AchievementId.IsEmpty() || InFlightAchievementIds.Contains(AchievementId))
	{
		return false;
	}

	IOnlineSubsystem* OnlineSubsystem = ResolveOnlineSubsystem();
	if (!OnlineSubsystem)
	{
		return false;
	}

	IOnlineAchievementsPtr AchievementsInterface = OnlineSubsystem->GetAchievementsInterface();
	if (!AchievementsInterface.IsValid())
	{
		return false;
	}

	FUniqueNetIdPtr LocalUserId;
	if (!TryResolveLocalUniqueNetId(LocalUserId) || !LocalUserId.IsValid())
	{
		return false;
	}

	FOnlineAchievementsWritePtr WriteObject = MakeShared<FOnlineAchievementsWrite, ESPMode::ThreadSafe>();
	WriteObject->SetFloatStat(AchievementId, 100.0f);

	FOnlineAchievementsWriteRef WriteObjectRef = WriteObject.ToSharedRef();
	InFlightAchievementIds.Add(AchievementId);
	InFlightWriteObjects.Add(AchievementId, WriteObject);

	AchievementsInterface->WriteAchievements(
		*LocalUserId,
		WriteObjectRef,
		FOnAchievementsWrittenDelegate::CreateUObject(
			this,
			&ThisClass::HandleAchievementWritten,
			AchievementId));

	return true;
}

void UAchievementSubsystem::HandleAchievementWritten(
	const FUniqueNetId& PlayerId,
	const bool bWasSuccessful,
	FString AchievementId)
{
	static_cast<void>(PlayerId);

	AchievementId = NormalizeAchievementId(MoveTemp(AchievementId));
	InFlightAchievementIds.Remove(AchievementId);
	InFlightWriteObjects.Remove(AchievementId);

	if (bWasSuccessful)
	{
		LocallyUnlockedAchievementIds.Add(AchievementId);
		return;
	}

	if (WriteAchievementThroughSteamApi(AchievementId))
	{
		LocallyUnlockedAchievementIds.Add(AchievementId);
	}
	else
	{
		PendingAchievementIds.Add(AchievementId);
	}
}

bool UAchievementSubsystem::WriteAchievementThroughSteamApi(const FString& AchievementId) const
{
	if (AchievementId.IsEmpty() || !SteamAPI_IsSteamRunning())
	{
		return false;
	}

	ISteamUserStats* SteamUserStatsInterface = SteamUserStats();
	if (!SteamUserStatsInterface)
	{
		return false;
	}

	bool bAlreadyUnlocked = false;
	if (SteamUserStatsInterface->GetAchievement(TCHAR_TO_UTF8(*AchievementId), &bAlreadyUnlocked) && bAlreadyUnlocked)
	{
		return true;
	}

	if (!SteamUserStatsInterface->SetAchievement(TCHAR_TO_UTF8(*AchievementId)))
	{
		return false;
	}

	return SteamUserStatsInterface->StoreStats();
}

FString UAchievementSubsystem::NormalizeAchievementId(FString AchievementId)
{
	AchievementId.TrimStartAndEndInline();
	return AchievementId;
}
