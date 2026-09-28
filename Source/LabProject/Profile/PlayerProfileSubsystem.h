#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Profile/PlayerProfileSaveData.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayerProfileSubsystem.generated.h"

class UPdSaveGame;
class UProfileSaveEnvelope;
class UPandoraDefinition;
class USkinDefinition;

// 저장 객체를 노출하지 않는 업적 조회용 값 복사본이다.
struct FPlayerProfileProgressSnapshot
{
	TArray<FMatchRecord> MatchRecords;
	int32 Gold = 0;
	int32 MatchPlayedCount = 0;
	int32 WinCount = 0;
	int32 TotalKillCount = 0;
	int32 TotalDeathCount = 0;
	int32 TotalRewardGold = 0;
	int32 ItemCollectedCount = 0;
	int32 GrantedPandoraCount = 0;
	int32 GrantedSkinCount = 0;
};

DECLARE_MULTICAST_DELEGATE(FOnPlayerProfileProgressChanged);

UCLASS()
class LABPROJECT_API UPlayerProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	bool LoadProfile();
	void SaveProfile();
	TArray<FMatchRecord> GetMatchRecords();
	FPlayerProfileProgressSnapshot GetProgressSnapshot();
	TArray<FPrimaryAssetId> GetOwnedSkinAssetIds();
	void AddMatchRecord(const FMatchRecord& MatchRecord, bool bSaveImmediately = true);
	int32 GetWinCount();
	int32 GetItemCollectedCount();
	int32 AddItemCollectedCount(int32 Amount, bool bSaveImmediately = true);
	FName GetSelectedAchievementId();
	bool SetSelectedAchievementId(FName AchievementId, bool bSaveImmediately = true);
	int32 GetGold();
	int32 SetGold(int32 NewGold, bool bSaveImmediately = true);
	int32 AddGold(int32 Amount, bool bSaveImmediately = true);
	bool SpendGold(int32 Amount, bool bSaveImmediately = true);
	// 골드와 구매한 Pandora/스킨만 초기화하고 기본 스킨과 경기 기록은 유지한다.
	bool ResetPurchasedProgress(bool bSaveImmediately = true);
	bool IsPandoraGranted(UPandoraDefinition* PandoraDefinition);
	int32 GetGrantedPandoraLevel(UPandoraDefinition* PandoraDefinition);
	bool GrantPandora(UPandoraDefinition* PandoraDefinition, int32 StartingLevel = 1, bool bSaveImmediately = true);
	bool TryPurchasePandoraWithGold(UPandoraDefinition* PandoraDefinition, int32 GoldCost, int32 StartingLevel, int32& OutRemainingGold, bool bSaveImmediately = true);
	bool IsSkinGranted(USkinDefinition* SkinDefinition);
	bool GrantSkin(USkinDefinition* SkinDefinition, bool bSaveImmediately = true);
	bool TryPurchaseSkinWithGold(USkinDefinition* SkinDefinition, int32 GoldCost, int32& OutRemainingGold, bool bSaveImmediately = true);
	FOnPlayerProfileProgressChanged& OnProfileProgressChanged() { return ProfileProgressChanged; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	bool TickPendingSaves(float DeltaTime);
	void HandlePrimarySaveCompleted(bool bSucceeded);
	void HandleBackupSaveCompleted(bool bSucceeded);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UPdSaveGame* GetOrCreateProfile();
	UPdSaveGame* CreateConfiguredSaveGameObject() const;
	UPdSaveGame* LoadBestAvailableSaveGame(bool& bOutRecoveredFromFallback) const;
	bool EnsureDefaultUnlockedSkins(UPdSaveGame& Profile);
	void RequestProfileSave(bool bSaveImmediately);
	void EnsureSaveTicker();
	void BeginAsyncSave();
	void FinishAsyncSave();
	void ScheduleSaveRetry();
	void FlushPendingSaveToShutdownSlot();

	UPROPERTY(Transient)
	TObjectPtr<UPdSaveGame> ProfileSaveGame;

	UPROPERTY(Transient)
	TObjectPtr<UProfileSaveEnvelope> SaveSnapshot;

	bool bProfileDirty = false;
	bool bSaveInFlight = false;
	double SaveDeadline = 0.0;
	int32 SaveRetryCount = 0;
	FTSTicker::FDelegateHandle SaveTickerHandle;
	bool bIsDeinitializing = false;
	FOnPlayerProfileProgressChanged ProfileProgressChanged;
};
