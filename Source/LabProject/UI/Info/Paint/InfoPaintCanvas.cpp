#include "UI/Info/Paint/InfoPaintCanvas.h"

#include "Character/PdPlayer.h"
#include "Component/Player/PaintCanvas/PaintCanvasComponent.h"
#include "Components/Image.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Mode/PdHUD.h"
#include "UI/Info/InfoWidget.h"
#include "UI/Info/Paint/PaintCanvasWidget.h"
#include "UI/Info/Paint/InfoPaintPreviewRenderer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoPaintCanvas)

namespace
{
	const FName CanvasTextureParameterName(TEXT("CanvasTexture"));
}

void UInfoPaintCanvas::Initialize(
	UInfoWidget* InOwnerWidget,
	UPaintCanvasWidget* InPaintCanvasWidget)
{
	OwnerWidget = InOwnerWidget;
	PaintCanvasWidget = InPaintCanvasWidget;

	const APlayerController* PlayerController = OwnerWidget
		? OwnerWidget->GetOwningPlayer()
		: nullptr;
	const APdHUD* HUD = PlayerController
		? Cast<APdHUD>(PlayerController->GetHUD())
		: nullptr;
	const UWidgetClassDefinition* WidgetDefinition = HUD
		? HUD->GetWidgetClassDefinition()
		: nullptr;
	OpaqueCanvasDisplayMaterial = WidgetDefinition
		? WidgetDefinition->GetSkinWidgetSettings().PaintCanvasDisplayMaterial.Get()
		: nullptr;
}

void UInfoPaintCanvas::Shutdown()
{
	if (Preview) Preview->Shutdown();
	Preview = nullptr;
	CancelStroke();
	HideActiveCanvas();
	if (PaintCanvasWidget)
	{
		PaintCanvasWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	OpaqueCanvasDisplayMaterialInstance = nullptr;
	OpaqueCanvasDisplayMaterial = nullptr;
	PaintCanvasWidget = nullptr;
	OwnerWidget = nullptr;
}

bool UInfoPaintCanvas::SetVisible(const bool bVisible)
{
	CancelStroke();

	if (!bVisible)
	{
		if (Preview) Preview->Shutdown();
		if (PaintCanvasWidget)
		{
			PaintCanvasWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		return false;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	UPaintCanvasComponent* PaintCanvasComponent = PlayerCharacter
		? PlayerCharacter->GetPaintCanvasComponent()
		: nullptr;
	UTextureRenderTarget2D* RenderTarget = PaintCanvasComponent
		? PaintCanvasComponent->GetActivePaintCanvasRenderTarget()
		: nullptr;
	UImage* PaintCanvasImage = PaintCanvasWidget
		? PaintCanvasWidget->GetCanvasImage()
		: nullptr;
	if (!OpaqueCanvasDisplayMaterialInstance && OpaqueCanvasDisplayMaterial)
	{
		OpaqueCanvasDisplayMaterialInstance = UMaterialInstanceDynamic::Create(
			OpaqueCanvasDisplayMaterial,
			this);
	}

	if (!PaintCanvasWidget
		|| !PaintCanvasImage
		|| !RenderTarget
		|| !OpaqueCanvasDisplayMaterialInstance
		|| !PlayerCharacter->HasActivePaintCanvas())
	{
		if (PaintCanvasWidget)
		{
			PaintCanvasWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (PlayerCharacter)
		{
			PlayerCharacter->HidePaintCanvas();
		}
		return false;
	}

	OpaqueCanvasDisplayMaterialInstance->SetTextureParameterValue(
		CanvasTextureParameterName,
		RenderTarget);
	PaintCanvasImage->SetBrushFromMaterial(OpaqueCanvasDisplayMaterialInstance);
	if (UImage* Speech = PaintCanvasWidget->GetSpeechPreviewImage())
		Speech->SetBrushFromMaterial(OpaqueCanvasDisplayMaterialInstance);
	if (!Preview) Preview = NewObject<UInfoPaintPreviewRenderer>(this);
	const APdHUD* HUD = OwnerWidget->GetOwningPlayer()->GetHUD<APdHUD>();
	if (const UWidgetClassDefinition* Definition = HUD ? HUD->GetWidgetClassDefinition() : nullptr)
		Preview->Show(PlayerCharacter, PaintCanvasWidget, RenderTarget, Definition->GetSkinWidgetSettings());
	PaintCanvasImage->SetVisibility(ESlateVisibility::Visible);
	PaintCanvasWidget->SetIsEnabled(true);
	PaintCanvasWidget->SetVisibility(ESlateVisibility::Visible);
	return true;
}

bool UInfoPaintCanvas::BeginStroke(const FVector2D& ScreenSpacePosition)
{
	CancelStroke();
	bIsDrawing = PaintAtScreenPosition(ScreenSpacePosition);
	return bIsDrawing;
}

bool UInfoPaintCanvas::ContinueStroke(
	const FVector2D& ScreenSpacePosition,
	const bool bIsLeftMouseButtonDown)
{
	if (!bIsDrawing)
	{
		return false;
	}

	if (!bIsLeftMouseButtonDown || !PaintAtScreenPosition(ScreenSpacePosition))
	{
		CancelStroke();
		return false;
	}

	return true;
}

bool UInfoPaintCanvas::EndStroke(const FVector2D& ScreenSpacePosition)
{
	if (!bIsDrawing)
	{
		return false;
	}

	PaintAtScreenPosition(ScreenSpacePosition);
	CancelStroke();
	return true;
}

void UInfoPaintCanvas::CancelStroke()
{
	bIsDrawing = false;
	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	UPaintCanvasComponent* PaintCanvas = PlayerCharacter ? PlayerCharacter->GetPaintCanvasComponent() : nullptr;
	if (PaintCanvas)
	{
		PaintCanvas->ResetPaintStroke();
	}
}

bool UInfoPaintCanvas::ExportActiveCanvas()
{
	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return false;
	}

	return PlayerCharacter->ExportActivePaintCanvasToSpeechBubble();
}

bool UInfoPaintCanvas::ApplyActiveCanvasToFaceDecal()
{
	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return false;
	}

	FSkinWidgetSettings SkinSettings;
	const APlayerController* PlayerController = OwnerWidget ? OwnerWidget->GetOwningPlayer() : nullptr;
	const APdHUD* HUD = PlayerController ? Cast<APdHUD>(PlayerController->GetHUD()) : nullptr;
	if (const UWidgetClassDefinition* WidgetDefinition = HUD ? HUD->GetWidgetClassDefinition() : nullptr)
	{
		SkinSettings = WidgetDefinition->GetSkinWidgetSettings();
	}

	UMaterialInterface* FaceDecalMaterial = SkinSettings.PaintCanvasFaceDecalMaterial.Get();
	return FaceDecalMaterial
		&& PlayerCharacter->ApplyActivePaintCanvasToFaceDecal(
			FaceDecalMaterial,
			SkinSettings.PaintCanvasFaceDecalSocketName,
			SkinSettings.PaintCanvasFaceDecalTransformOffset,
			SkinSettings.PaintCanvasFaceDecalSize,
			SkinSettings.PaintCanvasFaceDecalTextureParameterName);
}

bool UInfoPaintCanvas::HasActiveCanvas() const
{
	const APdPlayer* PlayerCharacter = GetPlayerCharacter();
	return PlayerCharacter && PlayerCharacter->HasActivePaintCanvas();
}

void UInfoPaintCanvas::HideActiveCanvas()
{
	if (APdPlayer* PlayerCharacter = GetPlayerCharacter())
	{
		PlayerCharacter->HidePaintCanvas();
	}
}

APdPlayer* UInfoPaintCanvas::GetPlayerCharacter() const
{
	return OwnerWidget ? Cast<APdPlayer>(OwnerWidget->GetOwningPlayerPawn()) : nullptr;
}

bool UInfoPaintCanvas::PaintAtScreenPosition(const FVector2D& ScreenSpacePosition)
{
	FVector2D DrawLocation = FVector2D::ZeroVector;
	if (!TryGetDrawLocation(ScreenSpacePosition, DrawLocation))
	{
		return false;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	UPaintCanvasComponent* PaintCanvasComponent = PlayerCharacter
		? PlayerCharacter->GetPaintCanvasComponent()
		: nullptr;
	const bool bPainted = PaintCanvasComponent
		&& PaintCanvasComponent->PaintAtNormalizedLocation(DrawLocation);
	if (bPainted && Preview) Preview->Refresh();
	return bPainted;
}

bool UInfoPaintCanvas::TryGetDrawLocation(
	const FVector2D& ScreenSpacePosition,
	FVector2D& OutDrawLocation) const
{
	const UImage* PaintCanvasImage = PaintCanvasWidget
		? PaintCanvasWidget->GetCanvasImage()
		: nullptr;
	if (!PaintCanvasWidget
		|| !PaintCanvasImage
		|| PaintCanvasWidget->GetVisibility() == ESlateVisibility::Collapsed
		|| PaintCanvasWidget->GetVisibility() == ESlateVisibility::Hidden)
	{
		return false;
	}

	const FGeometry& CanvasGeometry = PaintCanvasImage->GetCachedGeometry();
	const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();
	if (CanvasSize.X <= UE_KINDA_SMALL_NUMBER || CanvasSize.Y <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector2D LocalPosition = CanvasGeometry.AbsoluteToLocal(ScreenSpacePosition);
	if (LocalPosition.X < 0.0
		|| LocalPosition.Y < 0.0
		|| LocalPosition.X > CanvasSize.X
		|| LocalPosition.Y > CanvasSize.Y)
	{
		return false;
	}

	OutDrawLocation = FVector2D(
		FMath::Clamp(LocalPosition.X / CanvasSize.X, 0.0, 1.0),
		FMath::Clamp(LocalPosition.Y / CanvasSize.Y, 0.0, 1.0));
	return true;
}
