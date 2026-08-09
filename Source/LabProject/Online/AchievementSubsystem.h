#pragma once

#include "CoreMinimal.h"
#include "Definition/Online/AchievementDefinition.h"
#include "OnlineStats.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AchievementSubsystem.generated.h"

class IOnlineSubsystem;
class UPdSaveGame;
struct FStreamableHandle;

UCLASS()
class LABPROJECT_API UAchievementSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void EvaluateAndUnlockAchievementsForPlayerId(const FString& PlayerId);
	int32 CalculateAchievementProgressValue(const FString& PlayerId, const FAchievementEntry& Achievement) const;
	const UAchievementDefinition* GetAchievementDefinition();

private:
	const UAchievementDefinition* ResolveAchievementDefinition();
	void BeginAchievementDefinitionPreload();
	void BeginAchievementPresentationPreload();
	void HandleAchievementDefinitionContentReady();
	UPdSaveGame* ResolveSaveGame(const FString& PlayerId) const;
	IOnlineSubsystem* ResolveOnlineSubsystem() const;
	void HandleProfileProgressChanged(const FString& PlayerId);

	bool IsSteamSubsystemActive() const;
	bool TryResolveLocalUniqueNetId(FUniqueNetIdPtr& OutUniqueNetId) const;
	bool EnsureAchievementsQueried();
	void HandleAchievementsQueried(const FUniqueNetId& PlayerId, bool bWasSuccessful);

	void QueueUnlockAchievement(FString AchievementId);
	void FlushPendingAchievementUnlocks();
	bool IsAchievementAlreadyUnlocked(const FString& AchievementId) const;
	bool WriteAchievementThroughOnlineSubsystem(const FString& AchievementId);
	void HandleAchievementWritten(const FUniqueNetId& PlayerId, bool bWasSuccessful, FString AchievementId);
	bool WriteAchievementThroughSteamApi(const FString& AchievementId) const;

	static FString NormalizeAchievementId(FString AchievementId);

	TSet<FString> PendingAchievementIds;
	TSet<FString> PendingEvaluationPlayerIds;
	TSet<FString> InFlightAchievementIds;
	TSet<FString> LocallyUnlockedAchievementIds;
	TMap<FString, FOnlineAchievementsWritePtr> InFlightWriteObjects;

	bool bAchievementsQueried = false;
	bool bAchievementQueryInFlight = false;
	UPROPERTY(Transient)
	TObjectPtr<UAchievementDefinition> CachedAchievementDefinition;
	TSharedPtr<FStreamableHandle> DefinitionPreloadHandle;
	TSharedPtr<FStreamableHandle> PresentationPreloadHandle;
	FDelegateHandle ProfileProgressChangedHandle;
};
