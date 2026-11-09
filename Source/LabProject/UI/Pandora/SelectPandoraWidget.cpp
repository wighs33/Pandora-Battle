#include "UI/Pandora/SelectPandoraWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Component/Player/SelectingPandoraAndWeaponComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerState.h"
#include "UI/Pandora/PandoraIconArt.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SelectPandoraWidget)

namespace
{
	/** 칸 순서는 0 위, 1 오른쪽, 2 아래, 3 왼쪽이다. 화면 좌표는 y가 아래로 커지므로 위 칸이 -90도다. */
	constexpr int32 WheelSegmentCount = 4;
	const float WheelSegmentAngles[WheelSegmentCount] = { -UE_HALF_PI, 0.f, UE_HALF_PI, UE_PI };

	const FName WheelCenterKeys[WheelSegmentCount] = {
		TEXT("Info.SecondPandora"), TEXT("Info.ThirdPandora"), TEXT("Pandora.Unequip"), TEXT("Info.FirstPandora") };
	const FName WheelSelectKey(TEXT("Pandora.Select"));

	/** 가리킨 칸의 빛 그림(Img_WheelHover)은 위 칸을 가리킨 모습으로 그려져 있다. */
	constexpr float WheelHoverDrawnDegrees = -90.f;

	// 열릴 때: 조금 덜 감쇠된 스프링으로 살짝 넘쳤다가 자리 잡는다(감쇠비 약 0.62).
	const UiMotion::FSpringSettings WheelOpenSpring{ 380.f, 24.f };
	// 강조가 칸을 옮겨 갈 때: 단단하고 거의 넘치지 않는다.
	const UiMotion::FSpringSettings WheelHoverSpring{ 520.f, 40.f };
	// 가리킨 칸의 뚜껑이 들릴 때: 통통 튀며 들린다.
	const UiMotion::FSpringSettings WheelLiftSpring{ 600.f, 22.f };

	/** 장착 묶음 번호(1~3)의 그림. 번호가 범위를 벗어나거나 위젯이 없으면 nullptr */
	UImage* GetLoadoutImage(const TArray<UImage*>& Images, const int32 Nth)
	{
		const int32 Index = Nth - 1;
		return Images.IsValidIndex(Index) ? Images[Index] : nullptr;
	}
}

USelectPandoraWidget::USelectPandoraWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 휠은 화면 가운데에서 커지며 나타나므로 떠오르지 않는다.
	IntroRise = 0.f;
	IntroDuration = 0.16f;
}

void USelectPandoraWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	CacheDefaultPandoraBrushes();
	RefreshSelectedLoadoutNumber();
	RefreshCenterLabel();
}

void USelectPandoraWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	OpenElapsed = 0.f;
	OpenScale.Snap(OpenStartScale);
	HoveredDirectionIndex = INDEX_NONE;
	HoverAmount.Snap(0.f);
	for (UiMotion::FSpring& Lift : SegmentLifts)
	{
		Lift.Snap(0.f);
	}
	SetRenderScale(FVector2D(OpenStartScale));
	RefreshSelectedLoadoutNumber();
	RefreshCenterLabel();
}

// 휠 전체가 스프링으로 커지고, 뚜껑들이 위부터 시계 방향으로 차례로 나타난다.
// 가리킨 칸은 뚜껑이 바깥으로 들리며 테두리와 소켓에 빛이 켜지고, 양옆 틈빛이 그 칸 쪽으로 돌아간다.
void USelectPandoraWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	OpenElapsed += InDeltaTime;
	OpenScale.Step(1.f, InDeltaTime, WheelOpenSpring);
	SetRenderScale(FVector2D(OpenScale.Value));

	const bool bHovering = HoveredDirectionIndex != INDEX_NONE;
	if (bHovering)
	{
		HoverAngle.Step(UiMotion::NearestAngle(HoverAngle.Value, WheelSegmentAngles[HoveredDirectionIndex]), InDeltaTime, WheelHoverSpring);
	}
	HoverAmount.Step(bHovering ? 1.f : 0.f, InDeltaTime, WheelHoverSpring);

	for (int32 Segment = 0; Segment < WheelSegmentCount; ++Segment)
	{
		UiMotion::FSpring& Lift = SegmentLifts[Segment];
		Lift.Step(Segment == HoveredDirectionIndex ? 1.f : 0.f, InDeltaTime, WheelLiftSpring);
		if (UWidget* Panel = GetSegmentPanel(Segment))
		{
			const FVector2D Outward(FMath::Cos(WheelSegmentAngles[Segment]), FMath::Sin(WheelSegmentAngles[Segment]));
			Panel->SetRenderTranslation(Outward * (LidLiftDistance * Lift.Value));
			Panel->SetRenderOpacity(UiMotion::EaseOutCubic((OpenElapsed - Segment * SegmentStagger) / SegmentFadeDuration));
		}
		if (UWidget* Lit = GetSegmentLit(Segment))
		{
			Lit->SetRenderOpacity(FMath::Clamp(Lift.Value, 0.f, 1.f));
		}
	}

	if (Img_WheelHover)
	{
		Img_WheelHover->SetRenderTransformAngle(FMath::RadiansToDegrees(HoverAngle.Value) - WheelHoverDrawnDegrees);
		Img_WheelHover->SetRenderOpacity(FMath::Clamp(HoverAmount.Value, 0.f, 1.f));
	}
}

void USelectPandoraWidget::SetHoveredDirection(const int32 Index)
{
	const int32 Segment = Index >= 0 && Index < WheelSegmentCount ? Index : INDEX_NONE;
	if (Segment == HoveredDirectionIndex)
	{
		return;
	}
	// 아무 칸도 가리키지 않다가 처음 가리키면 빛이 그 칸에서 바로 켜진다(다른 칸에서 돌아 들어오지 않게).
	if (HoveredDirectionIndex == INDEX_NONE && Segment != INDEX_NONE && HoverAmount.Value <= KINDA_SMALL_NUMBER)
	{
		HoverAngle.Snap(WheelSegmentAngles[Segment]);
	}
	HoveredDirectionIndex = Segment;
	RefreshCenterLabel();
}

void USelectPandoraWidget::OnMenuLanguageChanged()
{
	Super::OnMenuLanguageChanged();
	RefreshCenterLabel();
}

void USelectPandoraWidget::RefreshCenterLabel()
{
	if (Txt_WheelCenter)
	{
		Txt_WheelCenter->SetText(MenuText(HoveredDirectionIndex != INDEX_NONE ? WheelCenterKeys[HoveredDirectionIndex] : WheelSelectKey));
	}
}

UWidget* USelectPandoraWidget::GetSegmentPanel(const int32 SegmentIndex) const
{
	UWidget* const Panels[WheelSegmentCount] = { Pnl_SecondSegment, Pnl_ThirdSegment, Pnl_UnequipSegment, Pnl_FirstSegment };
	return Panels[SegmentIndex];
}

UWidget* USelectPandoraWidget::GetSegmentLit(const int32 SegmentIndex) const
{
	UWidget* const Lits[WheelSegmentCount] = { Img_SecondLit, Img_ThirdLit, Img_UnequipLit, Img_FirstLit };
	return Lits[SegmentIndex];
}

void USelectPandoraWidget::SetPandoraImage(int32 Nth, UTexture2D* PandoraImage)
{
	CacheDefaultPandoraBrushes();
	UImage* Image = GetLoadoutImage({ FirstPandoraImage.Get(), SecondPandoraImage.Get(), ThirdPandoraImage.Get() }, Nth);
	if (!Image || !DefaultPandoraBrushes.IsValidIndex(Nth - 1))
	{
		return;
	}

	// 빈 소켓은 위젯에 정해 둔 빈 소켓 그림으로 되돌리고, 판도라가 있으면 같은 원 모양 브러시에 그림만 바꿔 끼운다.
	// 판도라 아이콘은 금테 원판 그림이라 금테 안쪽 그림 원만 소켓에 꽉 차게 넣는다.
	FSlateBrush Brush = DefaultPandoraBrushes[Nth - 1];
	if (PandoraImage)
	{
		Brush.SetResourceObject(PandoraImage);
		Brush.SetUVRegion(PandoraIconArt::MakeArtUVRegion(FVector2f(PandoraImage->GetSizeX(), PandoraImage->GetSizeY())));
	}
	Image->SetBrush(Brush);
}

void USelectPandoraWidget::SetPandoraEnabled(int32 Nth, bool bEnabled)
{
	if (UImage* Image = GetLoadoutImage({ FirstPandoraImage.Get(), SecondPandoraImage.Get(), ThirdPandoraImage.Get() }, Nth))
	{
		Image->SetColorAndOpacity(bEnabled ? EnabledPandoraTint : DisabledPandoraTint);
	}
	if (UImage* Cut = GetLoadoutImage({ Img_FirstCut.Get(), Img_SecondCut.Get(), Img_ThirdCut.Get() }, Nth))
	{
		Cut->SetVisibility(bEnabled ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void USelectPandoraWidget::SetWeaponImage(int32 Nth, UTexture2D* WeaponImage)
{
	// 무기가 없으면 무기판을 비워 둔다.
	if (UImage* Image = GetLoadoutImage({ FirstWeaponImage.Get(), SecondWeaponImage.Get(), ThirdWeaponImage.Get() }, Nth))
	{
		Image->SetBrushFromTexture(WeaponImage);
		Image->SetVisibility(WeaponImage ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void USelectPandoraWidget::SetDirection(int32 Index)
{
	switch (Index)
	{
	case -1:
		SelectDirection(EEnum_Direction::Center, false);
		break;
	case 0:
		SelectDirection(EEnum_Direction::Up, true);
		break;
	case 1:
		SelectDirection(EEnum_Direction::Right, true);
		break;
	case 2:
		SelectDirection(EEnum_Direction::Down, true);
		break;
	case 3:
		SelectDirection(EEnum_Direction::Left, true);
		break;
	default:
		break;
	}
}

void USelectPandoraWidget::CacheDefaultPandoraBrushes()
{
	if (bDefaultPandoraBrushesCached)
	{
		return;
	}

	DefaultPandoraBrushes.Reset();
	for (const UImage* Image : { FirstPandoraImage.Get(), SecondPandoraImage.Get(), ThirdPandoraImage.Get() })
	{
		DefaultPandoraBrushes.Add(Image ? Image->GetBrush() : FSlateBrush());
	}
	bDefaultPandoraBrushesCached = true;
}

void USelectPandoraWidget::RefreshSelectedLoadoutNumber()
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APdPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APdPlayerState>() : nullptr;
	const USelectingPandoraAndWeaponComponent* PandoraAndWeaponComponent = PlayerState ? PlayerState->GetSelectingPandoraAndWeaponComponent() : nullptr;
	const int32 LoadoutNumber = PandoraAndWeaponComponent ? PandoraAndWeaponComponent->GetSelectedPandoraAndWeaponNumber() : 0;
	SetSelectedLoadoutNumberVisibility(LoadoutNumber);
}

// 장착 중인 묶음의 소켓은 이에 불이 켜진다.
void USelectPandoraWidget::SetSelectedLoadoutNumberVisibility(const int32 LoadoutNumber) const
{
	const TArray<UImage*> HighlightImages = { Img_Highlight1.Get(), Img_Highlight2.Get(), Img_Highlight3.Get() };
	for (int32 Index = 0; Index < HighlightImages.Num(); ++Index)
	{
		if (UImage* HighlightImage = HighlightImages[Index])
		{
			HighlightImage->SetVisibility(LoadoutNumber == Index + 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void USelectPandoraWidget::SelectDirection(EEnum_Direction InDirection, bool bBroadcast)
{
	Direction = InDirection;

	if (bBroadcast)
	{
		OnSelected.Broadcast(Direction);
		RefreshSelectedLoadoutNumber();
	}
}
