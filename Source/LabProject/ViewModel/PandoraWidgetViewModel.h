#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Styling/SlateBrush.h"
#include "ViewModel/CommonViewModelBase.h"
#include "PandoraWidgetViewModel.generated.h"

UCLASS(BlueprintType)
class LABPROJECT_API UPandoraWidgetViewModel : public UCommonViewModelBase
{
	GENERATED_BODY()

public:
	UPandoraWidgetViewModel();

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	FText LevelText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	FSlateBrush IconBrush;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	FLinearColor OverlayColor;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	float ContentOpacity = 1.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	ESlateVisibility StateIconVisibility = ESlateVisibility::Collapsed;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	FLinearColor StateIconColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	bool bCanSpend = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	bool bIsLocked = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	bool bNotEnoughPoints = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Widget ViewModel")
	bool bAtMaxLevel = false;

	void ResetViewData();
	void SetDisplayName(const FText& InDisplayName);
	void SetLevelText(const FText& InLevelText);
	void SetIconResource(UObject* ResourceObject, const FVector2D& DefaultImageSize = FVector2D(100.0f, 100.0f));
	void SetOverlayColor(const FLinearColor& InOverlayColor);
	void SetContentOpacity(float InContentOpacity);
	void SetStateIconVisibility(ESlateVisibility InStateIconVisibility);
	void SetStateIconColor(const FLinearColor& InStateIconColor);
	void SetCanSpend(bool bInCanSpend);
	void SetIsLocked(bool bInIsLocked);
	void SetNotEnoughPoints(bool bInNotEnoughPoints);
	void SetAtMaxLevel(bool bInAtMaxLevel);

	static const FName ViewModelName;
};
