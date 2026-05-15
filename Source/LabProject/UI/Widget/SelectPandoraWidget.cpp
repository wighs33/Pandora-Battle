#include "UI/Widget/SelectPandoraWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SelectPandoraWidget)

void USelectPandoraWidget::SetPandoraImage(int32 Nth, UTexture2D* PandoraImage)
{
	SetImageByIndex({ FirstPandoraImage.Get(), SecondPandoraImage.Get(), ThirdPandoraImage.Get() }, Nth, PandoraImage);
}

void USelectPandoraWidget::SetWeaponImage(int32 Nth, UTexture2D* WeaponImage)
{
	SetImageByIndex({ FirstWeaponImage.Get(), SecondWeaponImage.Get(), ThirdWeaponImage.Get() }, Nth, WeaponImage);
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

void USelectPandoraWidget::SetImageByIndex(const TArray<UImage*>& Images, int32 Nth, UTexture2D* Texture) const
{
	const int32 ImageIndex = Nth - 1;
	if (!Images.IsValidIndex(ImageIndex) || !Images[ImageIndex])
	{
		return;
	}

	Images[ImageIndex]->SetBrushFromTexture(Texture, false);
}

void USelectPandoraWidget::SelectDirection(EEnum_Direction InDirection, bool bBroadcast)
{
	Direction = InDirection;

	if (bBroadcast)
	{
		OnSelected.Broadcast(Direction);
	}
}
