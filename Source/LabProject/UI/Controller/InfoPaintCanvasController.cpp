#include "UI/Controller/InfoPaintCanvasController.h"

#include "Character/PdPlayer.h"
#include "Component/Player/PaintCanvasComponent.h"
#include "Components/Image.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Mode/PdHUD.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/PaintCanvasWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoPaintCanvasController)

namespace
{
	const FName CanvasTextureParameterName(TEXT("CanvasTexture"));
}

void UInfoPaintCanvasController::Initialize(
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

void UInfoPaintCanvasController::Shutdown()
{
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

bool UInfoPaintCanvasController::SetVisible(const bool bVisible)
{
	CancelStroke();

	if (!bVisible)
	{
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
	PaintCanvasImage->SetVisibility(ESlateVisibility::Visible);
	PaintCanvasWidget->SetIsEnabled(true);
	PaintCanvasWidget->SetVisibility(ESlateVisibility::Visible);
	return true;
}

bool UInfoPaintCanvasController::BeginStroke(const FVector2D& ScreenSpacePosition)
{
	bIsDrawing = PaintAtScreenPosition(ScreenSpacePosition);
	return bIsDrawing;
}

bool UInfoPaintCanvasController::ContinueStroke(
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

bool UInfoPaintCanvasController::EndStroke(const FVector2D& ScreenSpacePosition)
{
	if (!bIsDrawing)
	{
		return false;
	}

	PaintAtScreenPosition(ScreenSpacePosition);
	CancelStroke();
	return true;
}

void UInfoPaintCanvasController::CancelStroke()
{
	bIsDrawing = false;
}

bool UInfoPaintCanvasController::ExportActiveCanvas()
{
	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return false;
	}

	return PlayerCharacter->ExportActivePaintCanvasToSpeechBubble();
}

bool UInfoPaintCanvasController::ApplyActiveCanvasToFaceDecal()
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

bool UInfoPaintCanvasController::HasActiveCanvas() const
{
	const APdPlayer* PlayerCharacter = GetPlayerCharacter();
	return PlayerCharacter && PlayerCharacter->HasActivePaintCanvas();
}

void UInfoPaintCanvasController::HideActiveCanvas()
{
	if (APdPlayer* PlayerCharacter = GetPlayerCharacter())
	{
		PlayerCharacter->HidePaintCanvas();
	}
}

APdPlayer* UInfoPaintCanvasController::GetPlayerCharacter() const
{
	return OwnerWidget ? Cast<APdPlayer>(OwnerWidget->GetOwningPlayerPawn()) : nullptr;
}

bool UInfoPaintCanvasController::PaintAtScreenPosition(const FVector2D& ScreenSpacePosition) const
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
	return PaintCanvasComponent
		&& PaintCanvasComponent->PaintAtNormalizedLocation(DrawLocation);
}

bool UInfoPaintCanvasController::TryGetDrawLocation(
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
