#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "SavedGameData/PdSaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayerProfileSubsystem.generated.h"

class APlayerController;
class APlayerState;
class UPandoraDefinition;
class UProfileSaveEnvelope;
class URewardDefinition;
class USkinDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerProfileProgressChanged, const FString&);

UCLASS()
class LABPROJECT_API UPlayerProfileSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void LoadGame(const FString& PlayerId);
	void SaveGame(const FString& PlayerId);
	UPdSaveGame* GetOrCreateSaveGame(const FString& PlayerId);

	FString ResolveSavePlayerId(
		const APlayerController* PlayerController,
		const APlayerState* PlayerState) const;
	FString GetLocalClientSavePlayerId() const;
	void SetPreferredSavePlayerId(const FString& PlayerId);
	const FString& GetPreferredSavePlayerId() const { return PreferredSavePlayerId; }

	void AddMatchRecord(const FString& PlayerId, const FMatchRecord& MatchRecord, bool bSaveImmediately = true);
	TArray<FMatchRecord> GetMatchRecords(const FString& PlayerId);
	int32 GetWinCount(const FString& PlayerId);
	int32 GetItemCollectedCount(const FString& PlayerId);
	int32 AddItemCollectedCount(const FString& PlayerId, int32 Amount, bool bSaveImmediately = true);
	FName GetSelectedAchievementId(const FString& PlayerId);
	bool SetSelectedAchievementId(
		const FString& PlayerId,
		FName AchievementId,
		bool bSaveImmediately = true);

	int32 GetGold(const FString& PlayerId);
	int32 SetGold(const FString& PlayerId, int32 NewGold, bool bSaveImmediately = true);
	int32 AddGold(const FString& PlayerId, int32 Amount, bool bSaveImmediately = true);
	bool SpendGold(const FString& PlayerId, int32 Amount, bool bSaveImmediately = true);
	int32 GrantGameVictoryGoldReward(
		const FString& PlayerId,
		URewardDefinition* RewardDefinition,
		bool bSaveImmediately = true);

	bool ResetShopSaveData(const FString& PlayerId, bool bSaveImmediately = true);
	bool IsPandoraGranted(const FString& PlayerId, UPandoraDefinition* PandoraDefinition);
	int32 GetGrantedPandoraLevel(const FString& PlayerId, UPandoraDefinition* PandoraDefinition);
	bool GrantPandoraToSave(
		const FString& PlayerId,
		UPandoraDefinition* PandoraDefinition,
		int32 StartingLevel = 1,
		bool bSaveImmediately = true);
	bool TryPurchasePandoraWithGold(
		const FString& PlayerId,
		UPandoraDefinition* PandoraDefinition,
		int32 GoldCost,
		int32 StartingLevel,
		int32& OutRemainingGold,
		bool bSaveImmediately = true);

	bool IsSkinGranted(const FString& PlayerId, USkinDefinition* SkinDefinition);
	bool GrantSkinToSave(
		const FString& PlayerId,
		USkinDefinition* SkinDefinition,
		bool bSaveImmediately = true);
	bool TryPurchaseSkinWithGold(
		const FString& PlayerId,
		USkinDefinition* SkinDefinition,
		int32 GoldCost,
		int32& OutRemainingGold,
		bool bSaveImmediately = true);

	FOnPlayerProfileProgressChanged& OnProfileProgressChanged() { return ProfileProgressChanged; }

private:
	UPdSaveGame* CreateConfiguredSaveGameObject() const;
	UPdSaveGame* LoadBestAvailableSaveGame(
		const FString& PlayerId,
		bool& bOutRecoveredFromFallback) const;
	void RequestProfileSave(const FString& PlayerId, bool bSaveImmediately);
	void EnsureSaveTicker();
	bool TickPendingSaves(float DeltaTime);
	void BeginAsyncSave(const FString& PlayerId);
	void HandlePrimarySaveCompleted(const FString& PlayerId, bool bSucceeded);
	void HandleBackupSaveCompleted(const FString& PlayerId, bool bSucceeded);
	void FinishAsyncSave(const FString& PlayerId);
	void FlushPendingSavesToShutdownSlots();
	void NotifyProfileProgressChanged(const FString& PlayerId);
	FString NormalizeProfilePlayerId(const FString& PlayerId) const;
	static FString GetBackupSlotName(const FString& PlayerId);
	static FString GetShutdownSlotName(const FString& PlayerId);

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UPdSaveGame>> SavedGameByPlayerId;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<USaveGame>> SaveSnapshotsByPlayerId;

	UPROPERTY(Transient)
	FString PreferredSavePlayerId;

	TSet<FString> DirtyPlayerIds;
	TSet<FString> SaveInFlightPlayerIds;
	TMap<FString, double> SaveDeadlinesByPlayerId;
	TMap<FString, int32> SaveRetryCountsByPlayerId;
	FTSTicker::FDelegateHandle SaveTickerHandle;
	bool bIsDeinitializing = false;

	FOnPlayerProfileProgressChanged ProfileProgressChanged;
};
