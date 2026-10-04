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

			// MouseCursorWidget은 이미지를 이미지 크기 두 배의 가운데 정렬 루트 안에 둔다.
			// 기본 소프트웨어 커서 배율에서는 이미지가 포인터 - 핫스팟 위치에서 시작한다.
			// 배율을 바꾼 경우에는 루트 전체 영역도 함께 뺀다. Slate는 할당 영역의 배율을 이미지의
			// 고정 캔버스 오프셋과 따로 적용하기 때문이다.
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
