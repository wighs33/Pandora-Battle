#include "UI/Widget/SelectPandoraWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SelectPandoraWidget)

void USelectPandoraWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CacheDefaultImageBrushes();
	RefreshSelectedLoadoutNumber();
}

void USelectPandoraWidget::SetPandoraImage(int32 Nth, UTexture2D* PandoraImage)
{
	SetImageByIndex(
		{ FirstPandoraImage.Get(), SecondPandoraImage.Get(), ThirdPandoraImage.Get() },
		DefaultPandoraImageBrushes,
		Nth,
		PandoraImage);
}

void USelectPandoraWidget::SetPandoraEnabled(int32 Nth, bool bEnabled)
{
	SetImageTintByIndex(
		{ FirstPandoraImage.Get(), SecondPandoraImage.Get(), ThirdPandoraImage.Get() },
		Nth,
		bEnabled ? EnabledPandoraTint : DisabledPandoraTint);

	const TArray<UImage*> CutImages =
		{ Img_FirstCut.Get(), Img_SecondCut.Get(), Img_ThirdCut.Get() };
	const int32 CutImageIndex = Nth - 1;
	if (CutImages.IsValidIndex(CutImageIndex) && CutImages[CutImageIndex])
	{
		CutImages[CutImageIndex]->SetVisibility(
			bEnabled ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void USelectPandoraWidget::SetWeaponImage(int32 Nth, UTexture2D* WeaponImage)
{
	SetImageByIndex(
		{ FirstWeaponImage.Get(), SecondWeaponImage.Get(), ThirdWeaponImage.Get() },
		DefaultWeaponImageBrushes,
		Nth,
		WeaponImage);
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

void USelectPandoraWidget::CacheDefaultImageBrushes()
{
	if (bDefaultImageBrushesCached)
	{
		return;
	}

	DefaultPandoraImageBrushes.Reset();
	DefaultWeaponImageBrushes.Reset();

	for (const UImage* Image : { FirstPandoraImage.Get(), SecondPandoraImage.Get(), ThirdPandoraImage.Get() })
	{
		DefaultPandoraImageBrushes.Add(Image ? Image->GetBrush() : FSlateBrush());
	}

	for (const UImage* Image : { FirstWeaponImage.Get(), SecondWeaponImage.Get(), ThirdWeaponImage.Get() })
	{
		DefaultWeaponImageBrushes.Add(Image ? Image->GetBrush() : FSlateBrush());
	}

	bDefaultImageBrushesCached = true;
}

void USelectPandoraWidget::SetImageByIndex(const TArray<UImage*>& Images, const TArray<FSlateBrush>& DefaultBrushes, int32 Nth, UTexture2D* Texture)
{
	CacheDefaultImageBrushes();

	const int32 ImageIndex = Nth - 1;
	if (!Images.IsValidIndex(ImageIndex) || !Images[ImageIndex])
	{
		return;
	}

	if (!Texture)
	{
		if (DefaultBrushes.IsValidIndex(ImageIndex))
		{
			Images[ImageIndex]->SetBrush(DefaultBrushes[ImageIndex]);
		}
		return;
	}

	Images[ImageIndex]->SetBrushFromTexture(Texture, false);
}

void USelectPandoraWidget::SetImageTintByIndex(const TArray<UImage*>& Images, int32 Nth, const FLinearColor& TintColor) const
{
	const int32 ImageIndex = Nth - 1;
	if (!Images.IsValidIndex(ImageIndex) || !Images[ImageIndex])
	{
		return;
	}

	Images[ImageIndex]->SetColorAndOpacity(TintColor);
}

void USelectPandoraWidget::RefreshSelectedLoadoutNumber()
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APdPlayerState* PlayerState = PlayerController
		? PlayerController->GetPlayerState<APdPlayerState>()
		: nullptr;
	SetSelectedLoadoutNumberVisibility(
		PlayerState
			? PlayerState->GetSelectedWeaponPandoraLoadoutNumber()
			: 0);
}

void USelectPandoraWidget::SetSelectedLoadoutNumberVisibility(
	const int32 LoadoutNumber) const
{
	const TArray<UTextBlock*> NumberTexts =
		{ Txt_First.Get(), Txt_Second.Get(), Txt_Third.Get() };
	const TArray<UImage*> HighlightImages =
		{ Img_Highlight1.Get(), Img_Highlight2.Get(), Img_Highlight3.Get() };
	for (int32 Index = 0; Index < NumberTexts.Num(); ++Index)
	{
		const bool bSelected = LoadoutNumber == Index + 1;
		if (UTextBlock* NumberText = NumberTexts[Index])
		{
			NumberText->SetVisibility(
				bSelected
					? ESlateVisibility::SelfHitTestInvisible
					: ESlateVisibility::Collapsed);
		}
		if (HighlightImages.IsValidIndex(Index))
		{
			if (UImage* HighlightImage = HighlightImages[Index])
			{
				HighlightImage->SetVisibility(
					bSelected
						? ESlateVisibility::Visible
						: ESlateVisibility::Collapsed);
			}
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
