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
#include "Profile/PlayerProfileSubsystem.h"

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
			&ThisClass::EvaluateAndUnlockAchievements);
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
	bEvaluationPending = false;
	InFlightAchievementIds.Reset();
	LocallyUnlockedAchievementIds.Reset();
	InFlightWriteObjects.Reset();
	SteamAchievementProgressById.Reset();
	bAchievementsQueried = false;
	bAchievementQueryInFlight = false;
	bAchievementQueryCompleted = false;
	SteamAchievementStateChanged.Clear();
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

void UAchievementSubsystem::EvaluateAndUnlockAchievements()
{
	if (GetGameInstance()->IsDedicatedServerInstance()) return;

	const UAchievementDefinition* AchievementDefinition = ResolveAchievementDefinition();
	if (!AchievementDefinition)
	{
		bEvaluationPending = true;
		BeginAchievementDefinitionPreload();
		return;
	}
	bEvaluationPending = false;

	for (const FAchievementEntry& Achievement : AchievementDefinition->Achievements)
	{
		if (!Achievement.bEnabled)
		{
			continue;
		}

		const FString AchievementId = UAchievementDefinition::NormalizeAchievementId(Achievement.AchievementId);
		if (AchievementId.IsEmpty())
		{
			continue;
		}

		const int32 RequiredValue = FMath::Max(Achievement.RequiredValue, 1);
		if (CalculateAchievementProgressValue(Achievement) >= RequiredValue)
		{
			QueueUnlockAchievement(AchievementId);
		}
	}

	RequestSteamAchievementQuery();
	FlushPendingAchievementUnlocks();
}

int32 UAchievementSubsystem::CalculateAchievementProgressValue(const FAchievementEntry& Achievement) const
{
	UPlayerProfileSubsystem* ProfileSubsystem = GetGameInstance()->GetSubsystem<UPlayerProfileSubsystem>();
	if (!ProfileSubsystem)
	{
		return 0;
	}

	const FPlayerProfileProgressSnapshot Profile = ProfileSubsystem->GetProgressSnapshot();
	int32 MatchRecordKillCount = 0;
	int32 MatchRecordDeathCount = 0;
	int32 MatchRecordRewardGold = 0;
	int32 MatchRecordWinCount = 0;
	for (const FMatchRecord& MatchRecord : Profile.MatchRecords)
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
		return FMath::Max(Profile.MatchPlayedCount, Profile.MatchRecords.Num());
	case EAchievementTrigger::WinCount:
		return FMath::Max(Profile.WinCount, MatchRecordWinCount);
	case EAchievementTrigger::KillCount:
		return FMath::Max(Profile.TotalKillCount, MatchRecordKillCount);
	case EAchievementTrigger::DeathCount:
		return FMath::Max(Profile.TotalDeathCount, MatchRecordDeathCount);
	case EAchievementTrigger::RewardGold:
		return FMath::Max3(
			Profile.TotalRewardGold,
			FMath::Max(Profile.Gold, 0),
			MatchRecordRewardGold);
	case EAchievementTrigger::PandoraUnlocked:
		return Profile.GrantedPandoraCount;
	case EAchievementTrigger::SkinUnlocked:
		return Profile.GrantedSkinCount;
	case EAchievementTrigger::ItemCollected:
		return FMath::Max(Profile.ItemCollectedCount, 0);
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

bool UAchievementSubsystem::IsSteamAchievementKnown(
	const FString& AchievementId) const
{
	return bAchievementsQueried
		&& SteamAchievementProgressById.Contains(
			UAchievementDefinition::NormalizeAchievementId(AchievementId));
}

bool UAchievementSubsystem::IsSteamAchievementUnlocked(
	const FString& AchievementId) const
{
	return GetSteamAchievementProgress(AchievementId) >= 100.0;
}

double UAchievementSubsystem::GetSteamAchievementProgress(
	const FString& AchievementId) const
{
	if (!bAchievementsQueried)
	{
		return 0.0;
	}

	const double* Progress = SteamAchievementProgressById.Find(
		UAchievementDefinition::NormalizeAchievementId(AchievementId));
	return Progress ? *Progress : 0.0;
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

	if (bEvaluationPending) EvaluateAndUnlockAchievements();
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

IOnlineAchievementsPtr UAchievementSubsystem::ResolveAchievementsInterface() const
{
	const IOnlineSubsystem* OnlineSubsystem = ResolveOnlineSubsystem();
	return OnlineSubsystem ? OnlineSubsystem->GetAchievementsInterface() : nullptr;
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

bool UAchievementSubsystem::RequestSteamAchievementQuery()
{
	if (bAchievementsQueried || bAchievementQueryInFlight)
	{
		return true;
	}

	if (!IsSteamSubsystemActive())
	{
		return false;
	}

	const IOnlineAchievementsPtr AchievementsInterface = ResolveAchievementsInterface();
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
	bAchievementQueryCompleted = false;
	AchievementsInterface->QueryAchievements(
		*LocalUserId,
		FOnQueryAchievementsCompleteDelegate::CreateUObject(this, &ThisClass::HandleAchievementsQueried));

	return true;
}

void UAchievementSubsystem::HandleAchievementsQueried(const FUniqueNetId& PlayerId, const bool bWasSuccessful)
{
	bAchievementQueryInFlight = false;
	bAchievementsQueried = bWasSuccessful;
	bAchievementQueryCompleted = true;
	SteamAchievementProgressById.Reset();
	LocallyUnlockedAchievementIds.Reset();
	if (bWasSuccessful)
	{
		RebuildSteamAchievementSnapshot(PlayerId);
	}
	SteamAchievementStateChanged.Broadcast();
	FlushPendingAchievementUnlocks();
}

void UAchievementSubsystem::RebuildSteamAchievementSnapshot(
	const FUniqueNetId& PlayerId)
{
	const IOnlineAchievementsPtr AchievementsInterface = ResolveAchievementsInterface();
	if (!AchievementsInterface.IsValid())
	{
		bAchievementsQueried = false;
		return;
	}

	TArray<FOnlineAchievement> CachedAchievements;
	if (AchievementsInterface->GetCachedAchievements(
			PlayerId,
			CachedAchievements) != EOnlineCachedResult::Success)
	{
		bAchievementsQueried = false;
		return;
	}

	for (const FOnlineAchievement& Achievement : CachedAchievements)
	{
		const FString AchievementId = UAchievementDefinition::NormalizeAchievementId(Achievement.Id);
		if (AchievementId.IsEmpty())
		{
			continue;
		}

		const double Progress = FMath::Clamp(Achievement.Progress, 0.0, 100.0);
		SteamAchievementProgressById.Add(AchievementId, Progress);
		if (Progress >= 100.0)
		{
			LocallyUnlockedAchievementIds.Add(AchievementId);
		}
	}
}

void UAchievementSubsystem::RefreshSteamAchievementQuery()
{
	bAchievementsQueried = false;
	bAchievementQueryCompleted = false;
	SteamAchievementProgressById.Reset();
	RequestSteamAchievementQuery();
}

void UAchievementSubsystem::QueueUnlockAchievement(FString AchievementId)
{
	AchievementId = UAchievementDefinition::NormalizeAchievementId(MoveTemp(AchievementId));
	if (AchievementId.IsEmpty()
		|| LocallyUnlockedAchievementIds.Contains(AchievementId)
		|| InFlightAchievementIds.Contains(AchievementId)
		|| IsSteamAchievementUnlocked(AchievementId))
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

	if (!bAchievementsQueried)
	{
		if (!bAchievementQueryInFlight && !bAchievementQueryCompleted)
		{
			RequestSteamAchievementQuery();
		}
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

		if (LocallyUnlockedAchievementIds.Contains(AchievementId) || IsSteamAchievementUnlocked(AchievementId))
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
			RefreshSteamAchievementQuery();
			break;
		}
	}
}

bool UAchievementSubsystem::WriteAchievementThroughOnlineSubsystem(const FString& AchievementId)
{
	if (AchievementId.IsEmpty() || InFlightAchievementIds.Contains(AchievementId))
	{
		return false;
	}

	const IOnlineAchievementsPtr AchievementsInterface = ResolveAchievementsInterface();
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

	AchievementId = UAchievementDefinition::NormalizeAchievementId(MoveTemp(AchievementId));
	InFlightAchievementIds.Remove(AchievementId);
	InFlightWriteObjects.Remove(AchievementId);

	if (bWasSuccessful)
	{
		RefreshSteamAchievementQuery();
		return;
	}

	if (WriteAchievementThroughSteamApi(AchievementId))
	{
		LocallyUnlockedAchievementIds.Add(AchievementId);
		RefreshSteamAchievementQuery();
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
