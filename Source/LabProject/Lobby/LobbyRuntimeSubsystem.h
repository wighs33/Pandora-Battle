#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "GameplayTagContainer.h"
#include "Mode/PdLobbyRuntimeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/GameResultTypes.h"
#include "UObject/PrimaryAssetId.h"
#include "LobbyRuntimeSubsystem.generated.h"

class APlayerController;
class APlayerState;
class UMaterialInterface;
class ULevelDefinition;
class UMatchRuleDefinition;
class UTexture2D;
class UUiSubsystem;
class FWidgetContentBundleLease;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class ELobbyContentPreloadResult : uint8
{
	NotStarted,
	Loading,
	Success,
	Failed,
	MissingAssets
};

UCLASS()
class LABPROJECT_API ULobbyRuntimeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void BeginLobbyEntryContentPreload();
	/** Releases lobby-only UI/data after the game screen has taken ownership. */
	void ReleaseLobbyEntryContentPreload();
	bool IsLobbyEntryContentReady() const;
	void BeginGameEntryContentPreload();
	void CancelGameEntryContentPreload();
	bool IsGameEntryContentReady() const
	{
		return GameEntryContentPreloadResult
			== ELobbyContentPreloadResult::Success;
	}
	ELobbyContentPreloadResult GetGameEntryContentPreloadResult() const
	{
		return GameEntryContentPreloadResult;
	}
	const TArray<FPrimaryAssetId>& GetMissingGameEntryPrimaryAssetIds() const
	{
		return MissingGameEntryPrimaryAssetIds;
	}
	static void GetGameEntryPrimaryAssetIds(
		TArray<FPrimaryAssetId>& OutAssetIds);
	const ULevelDefinition* GetLoadedLevelDefinition() const
	{
		return bLevelDefinitionReady ? LoadedLevelDefinition.Get() : nullptr;
	}
	const UMatchRuleDefinition* GetLoadedLobbyMatchRuleDefinition() const;

	void SetLobbyGameConfig(FName MapKey, const FString& TravelMapName, int32 MaxPlayerCount, int32 MaxBotCount);
	void SetLobbyRuntimeConfig(const FLobbyRuntimeConfig& InLobbyRuntimeConfig);

	const FLobbyRuntimeConfig& GetLobbyRuntimeConfig() const { return LobbyRuntimeConfig; }
	FName GetLobbySelectedMapKey() const { return LobbyRuntimeConfig.SelectedMapKey; }
	FString GetLobbyTravelMapName() const { return LobbyRuntimeConfig.TravelMapName; }
	int32 GetLobbyMaxPlayerCount() const { return LobbyRuntimeConfig.MaxPlayerCount; }
	int32 GetLobbyMaxBotCount() const { return LobbyRuntimeConfig.MaxBotCount; }

	void ResetCachedPlayerMatchIdentities();
	void CachePlayerMatchIdentityForPlayerState(const APlayerState* PlayerState, const FPlayerMatchIdentity& MatchIdentity);
	bool TryGetCachedPlayerMatchIdentityForPlayerState(const APlayerState* PlayerState, FPlayerMatchIdentity& OutMatchIdentity) const;
	FText ResolveDefaultPlayerNickname(
		const APlayerController* PlayerController,
		const APlayerState* PlayerState,
		int32 FallbackIndex) const;

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
	bool HasPendingTitleGameResult() const { return bHasPendingTitleGameResult; }

private:
	void HandleLevelDefinitionPreloadComplete();
	void HandleLobbyDataAssetsPreloadComplete();
	void HandleGameEntryContentPreloadComplete(uint32 RequestGeneration);
	void ReleaseGameEntryContentPreload();
	void SetGameEntryContentPreloadResult(
		ELobbyContentPreloadResult Result,
		TArray<FPrimaryAssetId> MissingAssetIds = {});
	static void FindUnregisteredGameEntryAssets(
		const TArray<FPrimaryAssetId>& AssetIds,
		TArray<FPrimaryAssetId>& OutMissingAssetIds);
	static void FindUnresolvedGameEntryAssets(
		const TArray<FPrimaryAssetId>& AssetIds,
		TArray<FPrimaryAssetId>& OutMissingAssetIds);
	bool IsLocalPlayerWidgetContentReady() const;
	TArray<FString> MakeLobbyPlayerCacheKeys(const APlayerState* PlayerState) const;

	UPROPERTY(Transient)
	TObjectPtr<ULevelDefinition> LoadedLevelDefinition;

	TSharedPtr<FStreamableHandle> LevelDefinitionPreloadHandle;
	TSharedPtr<FStreamableHandle> LobbyDataAssetsPreloadHandle;
	TMap<TWeakObjectPtr<UUiSubsystem>, TSharedPtr<FWidgetContentBundleLease>>
		LobbyWidgetBundleLeases;
	bool bLevelDefinitionPreloadPending = false;
	bool bLevelDefinitionReady = false;
	bool bLobbyDataAssetsPreloadPending = false;
	bool bLobbyDataAssetsReady = false;

	TSharedPtr<FStreamableHandle> GameEntryContentPreloadHandle;
	uint32 GameEntryContentRequestGeneration = 0;
	ELobbyContentPreloadResult GameEntryContentPreloadResult =
		ELobbyContentPreloadResult::NotStarted;
	TArray<FPrimaryAssetId> MissingGameEntryPrimaryAssetIds;

	UPROPERTY(Transient)
	FLobbyRuntimeConfig LobbyRuntimeConfig;

	TMap<FString, FPlayerMatchIdentity> CachedPlayerMatchIdentitiesByPlayerKey;
	TMap<FString, TMap<FGameplayTag, FName>> CachedLobbyEquippedSkinNamesByPlayerKey;
	TMap<FString, TMap<EEnum_Direction, FName>> CachedLobbyPandoraNamesByPlayerKey;

	UPROPERTY(Transient)
	FLobbyPaintCanvasFaceDecalCache LocalLobbyPaintCanvasFaceDecalCache;

	UPROPERTY(Transient)
	FGameResultPresentationData PendingTitleGameResult;

	UPROPERTY(Transient)
	bool bHasPendingTitleGameResult = false;
};
