#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "Common/GameResultTypes.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "GameplayTagContainer.h"
#include "Mode/PdLobbyRuntimeTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/PrimaryAssetId.h"
#include "LobbyRuntimeSubsystem.generated.h"

class APlayerController;
class APlayerState;
class UMaterialInterface;
class ULevelDefinition;
class UMatchRuleDefinition;
class UTexture2D;
class UUiSubsystem;
class FContentLease;
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
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	void BeginLobbyEntryContentPreload();
	/** Releases lobby-only UI/data after the game screen has taken ownership. */
	void ReleaseLobbyEntryContentPreload();
	bool IsLobbyEntryContentLoading() const;
	void BeginGameEntryContentPreload();
	void ReleaseGameEntryContentPreload();

	ELobbyContentPreloadResult GetGameEntryContentPreloadResult() const
	{
		return GameEntryContentPreloadResult;
	}

	const ULevelDefinition* GetLoadedLevelDefinition() const
	{
		return bLevelDefinitionReady ? LoadedLevelDefinition.Get() : nullptr;
	}
	const UMatchRuleDefinition* GetLoadedLobbyMatchRuleDefinition() const;

	// 경기를 마치고 로비로 돌아왔을 때 호스트가 고른 맵을 다시 선택하도록 맵 키를 보관한다.
	void SetLobbySelectedMapKey(FName MapKey) { LobbySelectedMapKey = MapKey; }
	FName GetLobbySelectedMapKey() const { return LobbySelectedMapKey; }

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
	void CacheLocalLobbyPaintCanvasStroke(
		UTexture2D* BrushTexture, double BrushSize, const FVector2D& DrawLocation, bool bStartsNewStroke = true);
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
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleLevelDefinitionPreloadComplete();
	void HandleGameEntryContentPreloadComplete(uint32 RequestGeneration);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	static void GetGameEntryPrimaryAssetIds(TArray<FPrimaryAssetId>& OutAssetIds);
	void SetGameEntryContentPreloadResult(
		ELobbyContentPreloadResult Result,
		TArray<FPrimaryAssetId> MissingAssetIds = {});
	static void FindUnregisteredGameEntryAssets(
		const TArray<FPrimaryAssetId>& AssetIds,
		TArray<FPrimaryAssetId>& OutMissingAssetIds);
	static void FindUnresolvedGameEntryAssets(
		const TArray<FPrimaryAssetId>& AssetIds,
		TArray<FPrimaryAssetId>& OutMissingAssetIds);
	TArray<FString> MakeLobbyPlayerCacheKeys(const APlayerState* PlayerState) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULevelDefinition> LoadedLevelDefinition;

	TSharedPtr<FContentLease> LevelDefinitionPreloadLease;
	TMap<TWeakObjectPtr<UUiSubsystem>, TSharedPtr<FContentLease>>
		LobbyContentLeases;
	bool bLevelDefinitionPreloadPending = false;
	bool bLevelDefinitionReady = false;

	TSharedPtr<FStreamableHandle> GameEntryContentPreloadHandle;
	uint32 GameEntryContentRequestGeneration = 0;
	ELobbyContentPreloadResult GameEntryContentPreloadResult =
		ELobbyContentPreloadResult::NotStarted;

	FName LobbySelectedMapKey;

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
