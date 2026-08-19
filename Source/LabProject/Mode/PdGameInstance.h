#pragma once

#include "CoreMinimal.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Common/Enum_Direction.h"
#include "Engine/GameInstance.h"
#include "GameplayTagContainer.h"
#include "Mode/PdLobbyRuntimeTypes.h"
#include "SavedGameData/PdSaveGame.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "UI/GameResultTypes.h"
#include "UObject/PrimaryAssetId.h"
#include "PdGameInstance.generated.h"

class APlayerController;
class APlayerState;
class UPandoraDefinition;
class URewardDefinition;
class UMaterialInterface;
class USkinDefinition;
class UTexture2D;
class ULobbyRuntimeSubsystem;
class UPlayerProfileSubsystem;
struct FPlayerMatchIdentity;

UCLASS(BlueprintType, Blueprintable, Config=Game)
class LABPROJECT_API UPdGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void OnStart() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintCallable, Category = "!Save")
	void LoadGame(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Save")
	void SaveGame(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Save")
	UPdSaveGame* GetOrCreateSaveGame(const FString& PlayerId);

	UFUNCTION(BlueprintPure, Category = "!Save")
	FString ResolveSavePlayerId(const APlayerController* PlayerController, const APlayerState* PlayerState) const;

	UFUNCTION(BlueprintPure, Category = "!Save")
	FString GetLocalClientSavePlayerId() const;

	UFUNCTION(BlueprintCallable, Category = "!Save")
	void SetPreferredSavePlayerId(const FString& PlayerId);

	UFUNCTION()
	FString GetPreferredSavePlayerId() const;

	UFUNCTION(BlueprintCallable, Category = "!Record")
	void AddMatchRecord(const FString& PlayerId, const FMatchRecord& MatchRecord, bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category = "!Record")
	TArray<FMatchRecord> GetMatchRecords(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Record")
	int32 GetWinCount(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Achievement")
	int32 GetItemCollectedCount(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Achievement")
	int32 AddItemCollectedCount(const FString& PlayerId, int32 Amount, bool bSaveImmediately = true);

	UFUNCTION(BlueprintPure, Category = "!Achievement")
	FName GetSelectedAchievementId(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Achievement")
	bool SetSelectedAchievementId(
		const FString& PlayerId,
		FName AchievementId,
		bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category = "!Currency|Gold")
	int32 GetGold(const FString& PlayerId);

	UFUNCTION(BlueprintCallable, Category = "!Currency|Gold")
	int32 SetGold(const FString& PlayerId, int32 NewGold, bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category = "!Currency|Gold")
	int32 AddGold(const FString& PlayerId, int32 Amount, bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category = "!Currency|Gold")
	bool SpendGold(const FString& PlayerId, int32 Amount, bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category = "!Currency|Gold")
	int32 GrantGameVictoryGoldReward(const FString& PlayerId, URewardDefinition* RewardDefinition, bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category = "!Shop|Debug")
	bool ResetShopSaveData(const FString& PlayerId, bool bSaveImmediately = true);

	UFUNCTION(BlueprintPure, Category = "!Pandora|Shop")
	bool IsPandoraGranted(const FString& PlayerId, UPandoraDefinition* PandoraDefinition);

	UFUNCTION(BlueprintPure, Category = "!Pandora|Shop")
	int32 GetGrantedPandoraLevel(const FString& PlayerId, UPandoraDefinition* PandoraDefinition);

	UFUNCTION(BlueprintCallable, Category = "!Pandora|Shop")
	bool GrantPandoraToSave(
		const FString& PlayerId,
		UPandoraDefinition* PandoraDefinition,
		int32 StartingLevel = 1,
		bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category = "!Pandora|Shop")
	bool TryPurchasePandoraWithGold(
		const FString& PlayerId,
		UPandoraDefinition* PandoraDefinition,
		int32 GoldCost,
		int32 StartingLevel,
		int32& OutRemainingGold,
		bool bSaveImmediately = true);

	UFUNCTION(BlueprintPure, Category = "!Skin|Shop")
	bool IsSkinGranted(const FString& PlayerId, USkinDefinition* SkinDefinition);

	UFUNCTION(BlueprintCallable, Category = "!Skin|Shop")
	bool GrantSkinToSave(
		const FString& PlayerId,
		USkinDefinition* SkinDefinition,
		bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category = "!Skin|Shop")
	bool TryPurchaseSkinWithGold(
		const FString& PlayerId,
		USkinDefinition* SkinDefinition,
		int32 GoldCost,
		int32& OutRemainingGold,
		bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category = "!Skill")
	void LoadSkillDataAssetsToMemory();

	UFUNCTION(BlueprintCallable, Category = "!Pandora")
	void LoadPandoraDataAssetsToMemory();

	UFUNCTION(BlueprintCallable, Category = "!Skin")
	void LoadSkinDataAssetsToMemory();

	UFUNCTION(BlueprintPure, Category = "!Skill")
	USkillDefinition* GetSkillDataAssetByName(FName SkillName) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora")
	UPandoraDefinition* GetPandoraDefinitionByName(FName PandoraName) const;

	UFUNCTION(BlueprintPure, Category = "!Skin")
	USkinDefinition* GetSkinDefinitionByName(FName SkinName) const;

	UFUNCTION(BlueprintCallable, Category = "!Pandora", meta = (AutoCreateRefTerm = "GrantedPandorasByName"))
	void BuildGrantedPandorasFromNames(const TMap<FName, int32>& GrantedPandorasByName, TArray<FGrantedPandora>& OutGrantedPandoras) const;

	UFUNCTION(BlueprintCallable, Category = "!Skin", meta = (AutoCreateRefTerm = "GrantedSkinsByName"))
	void BuildGrantedSkinDefinitionsFromNames(const TMap<FName, int32>& GrantedSkinsByName, TArray<USkinDefinition*>& OutSkinDefinitions) const;

	UFUNCTION(BlueprintCallable, Category = "!Lobby")
	void SetLobbyGameConfig(FName MapKey, const FString& TravelMapName, int32 MaxPlayerCount, int32 MaxBotCount);

	UFUNCTION()
	FName GetLobbySelectedMapKey() const;

	UFUNCTION()
	FString GetLobbyTravelMapName() const;

	UFUNCTION()
	int32 GetLobbyMaxPlayerCount() const;

	UFUNCTION()
	int32 GetLobbyMaxBotCount() const;

	UFUNCTION(BlueprintCallable, Category = "!Audio")
	void PlayBgmForContext(EBgmContext BgmContext);

	UFUNCTION(BlueprintCallable, Category = "!Audio")
	void RestoreWorldBgm();

	UFUNCTION(BlueprintCallable, Category = "!Audio")
	void StopBgm();

	void ResetCachedPlayerMatchIdentities();
	void CachePlayerMatchIdentityForPlayerState(const APlayerState* PlayerState, const FPlayerMatchIdentity& MatchIdentity);
	bool TryGetCachedPlayerMatchIdentityForPlayerState(const APlayerState* PlayerState, FPlayerMatchIdentity& OutMatchIdentity) const;
	FText ResolveDefaultPlayerNickname(const APlayerController* PlayerController, const APlayerState* PlayerState, int32 FallbackIndex) const;
	void ResetCachedLobbyEquippedSkinSlots();
	void CacheLobbyEquippedSkinSlotsForPlayerState(
		const APlayerState* PlayerState,
		const TMap<FGameplayTag, FName>& EquippedSkinNamesBySlot);
	bool TryGetCachedLobbyEquippedSkinSlotsForPlayerState(
		const APlayerState* PlayerState,
		TMap<FGameplayTag, FName>& OutEquippedSkinNamesBySlot) const;
	void ResetCachedLobbyPandoraLoadouts();
	void CacheLobbyPandoraLoadoutForPlayerState(
		const APlayerState* PlayerState,
		const TMap<EEnum_Direction, FName>& PandoraNamesByDirection);
	bool TryGetCachedLobbyPandoraLoadoutForPlayerState(
		const APlayerState* PlayerState,
		TMap<EEnum_Direction, FName>& OutPandoraNamesByDirection) const;
	void ResetLocalLobbyPaintCanvasCache();
	void CacheLocalLobbyPaintCanvasStroke(UTexture2D* BrushTexture, double BrushSize, const FVector2D& DrawLocation);
	void CacheLocalLobbyPaintCanvasFaceDecal(
		UMaterialInterface* FaceDecalMaterial,
		FName AttachSocketName,
		const FTransform& FaceDecalTransformOffset,
		FVector FaceDecalSize,
		FName TextureParameterName);
	bool ConsumeLocalLobbyPaintCanvasFaceDecalCache(FLobbyPaintCanvasFaceDecalCache& OutFaceDecalCache);
	void SetPendingTitleGameResult(const FGameResultPresentationData& GameResultData);
	void ClearPendingTitleGameResult();
	bool ConsumePendingTitleGameResult(FGameResultPresentationData& OutGameResultData);
	bool HasPendingTitleGameResult() const;
};
