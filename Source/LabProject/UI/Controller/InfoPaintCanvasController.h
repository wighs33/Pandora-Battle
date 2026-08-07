#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InfoPaintCanvasController.generated.h"

class UInfoWidget;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPaintCanvasWidget;
class APdPlayer;

/** Connects the Info screen's paint-canvas widget to the player's shared paint render target. */
UCLASS()
class LABPROJECT_API UInfoPaintCanvasController : public UObject
{
	GENERATED_BODY()

public:
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
	APdPlayer* GetPlayerCharacter() const;
	bool PaintAtScreenPosition(const FVector2D& ScreenSpacePosition) const;
	bool TryGetDrawLocation(const FVector2D& ScreenSpacePosition, FVector2D& OutDrawLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> OwnerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UPaintCanvasWidget> PaintCanvasWidget;

	/** UI-only material that displays the paint RGB while deliberately ignoring render-target alpha. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OpaqueCanvasDisplayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OpaqueCanvasDisplayMaterialInstance;

	bool bIsDrawing = false;
};
