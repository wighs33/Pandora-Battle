#pragma once

#include "CoreMinimal.h"
#include "Common/GameResultTypes.h"
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
	/** 로비 정의 데이터를 불러오는 중인지. 로비 화면 콘텐츠의 로딩 여부는 UiSubsystem이 답한다. */
	bool IsLobbyEntryContentLoading() const;
	void BeginGameEntryContentPreload();
	/** 호스트가 경기 시작을 준비하는 동안 켜진다. 로딩 화면이 이 값을 대기 사유로 읽는다. */
	void SetGameStartPreparationPending(bool bPending) { bGameStartPreparationPending = bPending; }
	bool IsGameStartPreparationPending() const { return bGameStartPreparationPending; }
	void ReleaseGameEntryContentPreload();

	ELobbyContentPreloadResult GetGameEntryContentPreloadResult() const
	{
		return GameEntryContentPreloadResult;
	}

	/** 경기 진입 콘텐츠 로딩이 성공이나 실패로 끝나면 알린다. 시작 취소로 로딩을 내려놓을 때는 알리지 않는다. */
	FSimpleMulticastDelegate& OnGameEntryContentPreloadFinished()
	{
		return GameEntryContentPreloadFinished;
	}

	const ULevelDefinition* GetLoadedLevelDefinition() const
	{
		return bLevelDefinitionReady ? LoadedLevelDefinition.Get() : nullptr;
	}
	const UMatchRuleDefinition* GetLoadedLobbyMatchRuleDefinition() const;

	// 경기를 마치고 로비로 돌아왔을 때 호스트가 고른 맵을 다시 선택하도록 맵 키를 보관한다.
	void SetLobbySelectedMapKey(FName MapKey) { LobbySelectedMapKey = MapKey; }
	FName GetLobbySelectedMapKey() const { return LobbySelectedMapKey; }

	FText ResolveDefaultPlayerNickname(
		const APlayerController* PlayerController,
		const APlayerState* PlayerState,
		int32 FallbackIndex) const;

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

	/** 로비 진입 준비를 시작하거나 놓을 때 알린다. 로비 화면 콘텐츠는 UI가 이 알림을 받아 직접 붙잡고 놓는다. */
	FSimpleMulticastDelegate OnLobbyEntryContentPreloadRequested;
	FSimpleMulticastDelegate OnLobbyEntryContentReleased;

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

private:
	UPROPERTY(Transient)
	TObjectPtr<ULevelDefinition> LoadedLevelDefinition;

	TSharedPtr<FContentLease> LevelDefinitionPreloadLease;
	bool bLevelDefinitionPreloadPending = false;
	bool bLevelDefinitionReady = false;

	TSharedPtr<FStreamableHandle> GameEntryContentPreloadHandle;
	uint32 GameEntryContentRequestGeneration = 0;
	ELobbyContentPreloadResult GameEntryContentPreloadResult =
		ELobbyContentPreloadResult::NotStarted;
	FSimpleMulticastDelegate GameEntryContentPreloadFinished;

	FName LobbySelectedMapKey;

	UPROPERTY(Transient)
	FLobbyPaintCanvasFaceDecalCache LocalLobbyPaintCanvasFaceDecalCache;

	UPROPERTY(Transient)
	FGameResultPresentationData PendingTitleGameResult;

	UPROPERTY(Transient)
	bool bHasPendingTitleGameResult = false;

	bool bGameStartPreparationPending = false;
};
