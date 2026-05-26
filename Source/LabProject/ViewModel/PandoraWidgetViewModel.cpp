#include "ViewModel/PandoraWidgetViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraWidgetViewModel)

const FName UPandoraWidgetViewModel::ViewModelName = TEXT("PandoraWidgetViewModel");

UPandoraWidgetViewModel::UPandoraWidgetViewModel()
{
	ResetViewData();
}

void UPandoraWidgetViewModel::ResetViewData()
{
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(LevelText, FText::GetEmpty());
	UE_MVVM_SET_PROPERTY_VALUE(IconBrush, FSlateBrush());
	UE_MVVM_SET_PROPERTY_VALUE(OverlayColor, FLinearColor::Transparent);
	UE_MVVM_SET_PROPERTY_VALUE(ContentOpacity, 1.0f);
	UE_MVVM_SET_PROPERTY_VALUE(StateIconVisibility, ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(StateIconColor, FLinearColor::White);
	UE_MVVM_SET_PROPERTY_VALUE(bCanSpend, false);
	UE_MVVM_SET_PROPERTY_VALUE(bIsLocked, false);
	UE_MVVM_SET_PROPERTY_VALUE(bNotEnoughPoints, false);
	UE_MVVM_SET_PROPERTY_VALUE(bAtMaxLevel, false);
}

void UPandoraWidgetViewModel::SetDisplayName(const FText& InDisplayName)
{
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InDisplayName);
}

void UPandoraWidgetViewModel::SetLevelText(const FText& InLevelText)
{
	UE_MVVM_SET_PROPERTY_VALUE(LevelText, InLevelText);
}

void UPandoraWidgetViewModel::SetIconResource(UObject* ResourceObject, const FVector2D& DefaultImageSize)
{
	FSlateBrush NewBrush;
	if (ResourceObject)
	{
		NewBrush.DrawAs = ESlateBrushDrawType::Image;
		NewBrush.ImageSize = DefaultImageSize;
		NewBrush.SetResourceObject(ResourceObject);
	}

	UE_MVVM_SET_PROPERTY_VALUE(IconBrush, NewBrush);
}

void UPandoraWidgetViewModel::SetOverlayColor(const FLinearColor& InOverlayColor)
{
	UE_MVVM_SET_PROPERTY_VALUE(OverlayColor, InOverlayColor);
}

void UPandoraWidgetViewModel::SetContentOpacity(float InContentOpacity)
{
	UE_MVVM_SET_PROPERTY_VALUE(ContentOpacity, FMath::Clamp(InContentOpacity, 0.0f, 1.0f));
}

void UPandoraWidgetViewModel::SetStateIconVisibility(ESlateVisibility InStateIconVisibility)
{
	UE_MVVM_SET_PROPERTY_VALUE(StateIconVisibility, InStateIconVisibility);
}

void UPandoraWidgetViewModel::SetStateIconColor(const FLinearColor& InStateIconColor)
{
	UE_MVVM_SET_PROPERTY_VALUE(StateIconColor, InStateIconColor);
}

void UPandoraWidgetViewModel::SetCanSpend(bool bInCanSpend)
{
	UE_MVVM_SET_PROPERTY_VALUE(bCanSpend, bInCanSpend);
}

void UPandoraWidgetViewModel::SetIsLocked(bool bInIsLocked)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsLocked, bInIsLocked);
}

void UPandoraWidgetViewModel::SetNotEnoughPoints(bool bInNotEnoughPoints)
{
	UE_MVVM_SET_PROPERTY_VALUE(bNotEnoughPoints, bInNotEnoughPoints);
}

void UPandoraWidgetViewModel::SetAtMaxLevel(bool bInAtMaxLevel)
{
	UE_MVVM_SET_PROPERTY_VALUE(bAtMaxLevel, bInAtMaxLevel);
}
