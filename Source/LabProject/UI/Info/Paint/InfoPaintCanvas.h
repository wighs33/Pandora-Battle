#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InfoPaintCanvas.generated.h"

class UInfoWidget;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPaintCanvasWidget;
class APdPlayer;
class UInfoPaintPreviewRenderer;

/** Info 화면의 페인트 캔버스 위젯을 플레이어의 공용 페인트 렌더 타깃에 연결한다. */
UCLASS()
class LABPROJECT_API UInfoPaintCanvas : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(
		UInfoWidget* InOwnerWidget,
		UPaintCanvasWidget* InPaintCanvasWidget);
	void Shutdown();

	bool SetVisible(bool bVisible);
	bool BeginStroke(const FVector2D& ScreenSpacePosition);
	bool ContinueStroke(const FVector2D& ScreenSpacePosition, bool bIsLeftMouseButtonDown);
	bool EndStroke(const FVector2D& ScreenSpacePosition);
	void CancelStroke();
	bool ExportActiveCanvas();
	bool ApplyActiveCanvasToFaceDecal();
	bool HasActiveCanvas() const;
	void HideActiveCanvas();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	APdPlayer* GetPlayerCharacter() const;
	bool PaintAtScreenPosition(const FVector2D& ScreenSpacePosition);
	bool TryGetDrawLocation(const FVector2D& ScreenSpacePosition, FVector2D& OutDrawLocation) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> OwnerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPaintCanvasWidget> PaintCanvasWidget;

	/** 칠한 RGB를 보여 주되 렌더 타깃의 알파는 일부러 무시하는 UI 전용 머티리얼. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OpaqueCanvasDisplayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OpaqueCanvasDisplayMaterialInstance;

	bool bIsDrawing = false;
	UPROPERTY(Transient)
	TObjectPtr<UInfoPaintPreviewRenderer> Preview;
};
