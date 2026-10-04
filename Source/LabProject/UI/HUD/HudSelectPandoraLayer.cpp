#include "UI/HUD/HudSelectPandoraLayer.h"

#include "Common/Enum_Direction.h"
#include "Engine/LocalPlayer.h"
#include "Mode/PdPlayerController.h"
#include "UI/Core/UiScreen.h"
#include "UI/Core/UiSubsystem.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "UI/HUD/PdHUD.h"
#include "UI/Info/Presenter/InfoUiPresenter.h"
#include "UI/Pandora/SelectPandoraWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HudSelectPandoraLayer)

namespace
{
	EEnum_Direction ResolveDirectionFromIndex(const int32 Index)
	{
		switch (Index)
		{
		case 0:
			return EEnum_Direction::Up;
		case 1:
			return EEnum_Direction::Right;
		case 2:
			return EEnum_Direction::Down;
		case 3:
			return EEnum_Direction::Left;
		default:
			return EEnum_Direction::Center;
		}
	}
}

void UHudSelectPandoraLayer::Initialize(APdHUD* InOwnerHud)
{
	OwnerHud = InOwnerHud;
}

void UHudSelectPandoraLayer::EnsureWidget(APlayerController& Controller, const UWidgetClassDefinition& Definition)
{
	if (Widget)
	{
		return;
	}

	if (const TSubclassOf<USelectPandoraWidget> WidgetClass = Definition.GetSelectPandoraWidgetClass())
	{
		Widget = CreateWidget<USelectPandoraWidget>(&Controller, WidgetClass);
	}
}

void UHudSelectPandoraLayer::BindPresenter()
{
	APdHUD* Hud = OwnerHud.Get();
	if (!Widget || !Hud)
	{
		return;
	}

	if (UInfoUiPresenter* Presenter = Hud->GetInfoUiPresenter())
	{
		Widget->OnSelected.RemoveDynamic(Presenter, &UInfoUiPresenter::HandleSelectedPandoraDirection);
		Widget->OnSelected.AddUniqueDynamic(Presenter, &UInfoUiPresenter::HandleSelectedPandoraDirection);
	}
}

void UHudSelectPandoraLayer::ReleaseWidget()
{
	if (!Widget)
	{
		return;
	}

	const APdHUD* Hud = OwnerHud.Get();
	if (UInfoUiPresenter* Presenter = Hud ? Hud->CachedInfoUiPresenter.Get() : nullptr)
	{
		Widget->OnSelected.RemoveDynamic(Presenter, &UInfoUiPresenter::HandleSelectedPandoraDirection);
	}
	Widget->RemoveFromParent();
	Widget = nullptr;
}

bool UHudSelectPandoraLayer::Open()
{
	APdHUD* Hud = OwnerHud.Get();
	APlayerController* Controller = Hud ? Hud->GetOwningPlayerController() : nullptr;
	if (!Widget || !Controller || IsOpen())
	{
		return false;
	}

	Screen = UUiScreen::CreateBlocking(Controller, Widget, nullptr,
		FSimpleDelegate::CreateWeakLambda(Hud, [Hud]() { Hud->CloseSelectPandoraUiInternal(false); }), ECommonInputMode::All);
	Controller->GetLocalPlayer()->GetSubsystem<UUiSubsystem>()->PushScreen(Screen, EUiScreenLayer::Overlay);
	int32 Width = 0, Height = 0;
	Controller->GetViewportSize(Width, Height);
	Controller->SetMouseLocation(Width / 2, Height / 2);
	return true;
}

bool UHudSelectPandoraLayer::Close(const bool bCommitSelection)
{
	bool bSelectionWouldChangeLoadout = false;
	if (Widget)
	{
		if (bCommitSelection)
		{
			const EEnum_Direction SelectedDirection = ResolveDirectionFromIndex(DirectionIndex);
			APdHUD* Hud = OwnerHud.Get();
			if (UInfoUiPresenter* InfoUiPresenter = Hud ? Hud->GetInfoUiPresenter() : nullptr)
			{
				bSelectionWouldChangeLoadout = InfoUiPresenter->WouldSelectedPandoraDirectionChangeLoadout(SelectedDirection);
			}

			Widget->SetDirection(DirectionIndex);
		}
		Widget->RemoveFromParent();
	}

	if (Screen)
	{
		Screen->DeactivateWidget();
		Screen = nullptr;
	}
	return bSelectionWouldChangeLoadout;
}

bool UHudSelectPandoraLayer::IsOpen() const
{
	return Screen && Screen->IsActivated();
}

void UHudSelectPandoraLayer::UpdateDirectionFromMouse()
{
	const APdHUD* Hud = OwnerHud.Get();
	const APdPlayerController* Controller = Hud ? Hud->GetPdController() : nullptr;
	const UWidgetClassDefinition* Definition = Hud ? Hud->GetWidgetClassDefinition() : nullptr;
	if (!Controller || !Widget || !IsOpen() || !Definition)
	{
		return;
	}

	const FSelectPandoraWidgetSettings& SelectPandoraSettings = Definition->GetSelectPandoraWidgetSettings();

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	Controller->GetViewportSize(ViewportSizeX, ViewportSizeY);

	float MouseX = 0.f;
	float MouseY = 0.f;
	Controller->GetMousePosition(MouseX, MouseY);

	const FVector2D MousePosition(MouseX, MouseY);
	const FVector2D ViewportCenter(static_cast<double>(ViewportSizeX) / 2.0, static_cast<double>(ViewportSizeY) / 2.0);
	const FVector2D DirectionFromCenter = MousePosition - ViewportCenter;

	if (DirectionFromCenter.Size() < SelectPandoraSettings.DeadZoneRadius)
	{
		DirectionIndex = -1;
		return;
	}

	const double SegmentAngle = SelectPandoraSettings.SegmentAngle;
	if (FMath::IsNearlyZero(SegmentAngle))
	{
		return;
	}

	const double DirectionAngle = FMath::RadiansToDegrees(FMath::Atan2(DirectionFromCenter.Y, DirectionFromCenter.X));
	const double NormalizedAngle = FMath::Fmod(DirectionAngle + 450.0 + (SegmentAngle / 2.0), 360.0);
	DirectionIndex = FMath::FloorToInt(NormalizedAngle / SegmentAngle);
}
