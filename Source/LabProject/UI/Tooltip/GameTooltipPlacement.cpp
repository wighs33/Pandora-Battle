#include "UI/Tooltip/GameTooltipPlacement.h"

#include "Components/Widget.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Widgets/SWidget.h"

namespace GameTooltipPlacement
{
void ApplyToButton(UWidget* Widget)
{
	if (!Widget || !Widget->GetWorld() || !Widget->GetWorld()->IsGameWorld())
	{
		return;
	}

	const TSharedPtr<SWidget> SlateWidget = Widget->GetCachedWidget();
	if (!SlateWidget.IsValid())
	{
		return;
	}

	SlateWidget->EnableToolTipForceField(true);
	const TWeakObjectPtr<UWidget> WeakWidget(Widget);
	SlateWidget->SetToolTipForceFieldExpansion(
		TAttribute<TOptional<FSlateRect>>::CreateLambda([WeakWidget]() -> TOptional<FSlateRect>
		{
			const UWidget* Source = WeakWidget.Get();
			if (!Source || !FSlateApplication::IsInitialized())
			{
				return {};
			}

			const UGameSettingDefinition* Settings =
				UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(Source);
			if (!Settings || !Settings->bUseCustomMouseCursor || Settings->MouseCursorTexture.IsNull())
			{
				return {};
			}

			const FVector2D CursorPosition = FSlateApplication::Get().GetCursorPos();
			const FVector2D Size(FMath::Max(Settings->MouseCursorSize.X, 1.0),
				FMath::Max(Settings->MouseCursorSize.Y, 1.0));
			const FVector2D HotSpot(FMath::Clamp(Settings->MouseCursorHotSpot.X, 0.0, Size.X),
				FMath::Clamp(Settings->MouseCursorHotSpot.Y, 0.0, Size.Y));
			const IConsoleVariable* ScaleVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.SoftwareCursorScale"));
			const float CursorScale = ScaleVariable ? FMath::Max(ScaleVariable->GetFloat(), 0.01f) : 1.0f;

			// MouseCursorWidget puts its image inside a centered root twice the image size.
			// At the normal software cursor scale, the image begins at pointer - hotspot.
			// For overridden scales, exclude the full root envelope as well: Slate scales
			// its allocation separately from the image's fixed canvas offsets.
			FVector2D TopLeft = CursorPosition - HotSpot;
			FVector2D BottomRight = TopLeft + Size;
			if (!FMath::IsNearlyEqual(CursorScale, 1.0f))
			{
				const FVector2D Radius = Size * FMath::Max(CursorScale, 2.0f);
				TopLeft = CursorPosition - Radius;
				BottomRight = CursorPosition + Radius;
			}
			const float Gap = 12.0f * FMath::Max(FSlateApplication::Get().GetApplicationScale(), 1.0f);
			return FSlateRect(TopLeft.X - Gap, TopLeft.Y - Gap,
				BottomRight.X + Gap, BottomRight.Y + Gap);
		}));
}
}
