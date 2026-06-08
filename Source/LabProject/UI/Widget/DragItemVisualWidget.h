#pragma once

#include "Blueprint/UserWidget.h"
#include "DragItemVisualWidget.generated.h"

class UImage;
class USizeBox;
class UTexture2D;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UDragItemVisualWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Drag")
	void SetIconTexture(UTexture2D* InIconTexture);

	UFUNCTION(BlueprintCallable, Category = "!UI|Drag")
	void SetIconSize(FVector2D InIconSize);

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Drag|Bind")
	TObjectPtr<USizeBox> RootSizeBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Drag|Bind")
	TObjectPtr<UImage> IconImage;

private:
	void CacheOptionalWidgets();
	void ApplyVisual();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> IconTexture;

	UPROPERTY(Transient)
	FVector2D IconSize = FVector2D(56.0f, 56.0f);
};
