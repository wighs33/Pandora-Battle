#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/ItemViewData.h"
#include "ItemDetailWidget.generated.h"

class UImage;
class UItemInstance;
class UPanelWidget;
class USkinDefinition;
class USkinInstance;
class UTextBlock;
class UTexture2D;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UItemDetailWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Detail")
	void SetItem(UItemInstance* InItemInstance, UItemInstance* InCompareItemInstance = nullptr);

	UFUNCTION(BlueprintCallable, Category = "!UI|Detail")
	void SetItemViewData(const FPdItemViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "!UI|Detail")
	void SetSkin(USkinInstance* InSkinInstance);

	void SetSkinDefinition(const USkinDefinition* InSkinDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI|Detail")
	void ClearDetails();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Detail|Stats")
	FLinearColor NeutralStatColor = FLinearColor::White;

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Detail|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Detail|Bind")
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Detail|Bind")
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Detail|Bind")
	TObjectPtr<UPanelWidget> StatsList;

private:
	void CacheOptionalWidgets();
	void SetIconResource(UObject* IconResource) const;
	void SetHeader(const FPdItemViewData& ViewData) const;
	void PopulateStats(const TMap<FGameplayTag, float>& NewStats);
	void AddStatRow(FGameplayTag StatTag, float NewValue);
	static FString GetDisplayNameForStatTag(FGameplayTag StatTag);
};
