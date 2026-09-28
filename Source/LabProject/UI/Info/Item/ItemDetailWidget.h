#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Info/Item/ItemViewData.h"
#include "ItemDetailWidget.generated.h"

class UImage;
class UItemInstance;
class UMenuLocalizationSubsystem;
class UPanelWidget;
class USkinDefinition;
class UTextBlock;
class UTexture2D;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UItemDetailWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Detail")
	void SetItem(UItemInstance* InItemInstance, UItemInstance* InCompareItemInstance = nullptr);

	UFUNCTION(BlueprintCallable, Category = "!UI|Detail")
	void SetItemViewData(const FItemViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "!UI|Detail")
	void SetSkinDefinition(const USkinDefinition* InSkinDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI|Detail")
	void ClearDetails();

private:
	UFUNCTION()
	void RefreshLocalizedDetails();
	UMenuLocalizationSubsystem* GetLocalization() const;
	void ApplyLocalizedFont() const;
	TWeakObjectPtr<UItemInstance> DisplayedItem;
	TWeakObjectPtr<const USkinDefinition> DisplayedSkin;

	void SetIconResource(UObject* IconResource) const;
	void SetHeader(const FItemViewData& ViewData) const;
	void PopulateStats(
		const TMap<FGameplayTag, float>& NewStats,
		const TMap<FGameplayTag, float>& UpgradeBonusStats);
	void AddStatRow(FGameplayTag StatTag, float NewValue, float UpgradeBonusValue);
	static FString GetDisplayNameForStatTag(FGameplayTag StatTag);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Detail|Stats")
	FLinearColor NeutralStatColor = FLinearColor::White;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Detail|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Detail|Bind")
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Detail|Bind")
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Detail|Bind")
	TObjectPtr<UPanelWidget> StatsList;
};
