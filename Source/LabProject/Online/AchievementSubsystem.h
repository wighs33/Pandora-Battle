#pragma once

#include "CoreMinimal.h"
#include "Definition/Online/AchievementDefinition.h"
#include "OnlineStats.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AchievementSubsystem.generated.h"

class IOnlineSubsystem;
class UPdSaveGame;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE(FOnSteamAchievementStateChanged);

UCLASS()
class LABPROJECT_API UAchievementSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	void EvaluateAndUnlockAchievementsForPlayerId(const FString& PlayerId);
	int32 CalculateAchievementProgressValue(const FString& PlayerId, const FAchievementEntry& Achievement) const;
	const UAchievementDefinition* GetAchievementDefinition();
	bool RequestSteamAchievementQuery();
	bool IsSteamAchievementQueryComplete() const { return bAchievementQueryCompleted; }
	bool HasSteamAchievementData() const { return bAchievementsQueried; }
	bool IsSteamAchievementKnown(const FString& AchievementId) const;
	bool IsSteamAchievementUnlocked(const FString& AchievementId) const;
	double GetSteamAchievementProgress(const FString& AchievementId) const;

	// Event Handlers --------------------------------------------------------------------------------------------------
	FOnSteamAchievementStateChanged& OnSteamAchievementStateChanged()
	{
		return SteamAchievementStateChanged;
	}

private:
	void HandleAchievementDefinitionContentReady();
	void HandleProfileProgressChanged(const FString& PlayerId);
	void HandleAchievementsQueried(const FUniqueNetId& PlayerId, bool bWasSuccessful);
	void HandleAchievementWritten(const FUniqueNetId& PlayerId, bool bWasSuccessful, FString AchievementId);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	const UAchievementDefinition* ResolveAchievementDefinition();
	void BeginAchievementDefinitionPreload();
	void BeginAchievementPresentationPreload();
	UPdSaveGame* ResolveSaveGame(const FString& PlayerId) const;
	IOnlineSubsystem* ResolveOnlineSubsystem() const;

	bool IsSteamSubsystemActive() const;
	bool TryResolveLocalUniqueNetId(FUniqueNetIdPtr& OutUniqueNetId) const;
	bool EnsureAchievementsQueried();
	void RebuildSteamAchievementSnapshot(const FUniqueNetId& PlayerId);
	void RefreshSteamAchievementQuery();

	void QueueUnlockAchievement(FString AchievementId);
	void FlushPendingAchievementUnlocks();
	bool IsAchievementAlreadyUnlocked(const FString& AchievementId) const;
	bool WriteAchievementThroughOnlineSubsystem(const FString& AchievementId);
	bool WriteAchievementThroughSteamApi(const FString& AchievementId) const;

	static FString NormalizeAchievementId(FString AchievementId);

private:
	TSet<FString> PendingAchievementIds;
	TSet<FString> PendingEvaluationPlayerIds;
	TSet<FString> InFlightAchievementIds;
	TSet<FString> LocallyUnlockedAchievementIds;
	TMap<FString, FOnlineAchievementsWritePtr> InFlightWriteObjects;
	TMap<FString, double> SteamAchievementProgressById;

	bool bAchievementsQueried = false;
	bool bAchievementQueryInFlight = false;
	bool bAchievementQueryCompleted = false;
	FOnSteamAchievementStateChanged SteamAchievementStateChanged;
	UPROPERTY(Transient)
	TObjectPtr<UAchievementDefinition> CachedAchievementDefinition;
	TSharedPtr<FStreamableHandle> DefinitionPreloadHandle;
	TSharedPtr<FStreamableHandle> PresentationPreloadHandle;
	FDelegateHandle ProfileProgressChangedHandle;
};
