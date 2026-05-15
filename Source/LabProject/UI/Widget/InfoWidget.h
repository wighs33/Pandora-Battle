#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/Widget/LeftEquipmentWidget.h"
#include "UI/Widget/LeftPandoraWidget.h"
#include "UI/Widget/LeftProfileWidget.h"
#include "UI/Widget/LeftSkinWidget.h"
#include "UI/Widget/RightPandoraWidget.h"
#include "UI/Widget/RightInventoryWidget.h"
#include "UI/Widget/RightSkinWidget.h"
#include "UI/Widget/RightStatusWidget.h"
#include "InfoWidget.generated.h"

class UButton;
class UWidget;
class UWidgetSwitcher;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPdOnClickedInfoCenterButton, FGameplayTag, LeftUiTag, FGameplayTag, RightUiTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnClickedMapButton);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInfoWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Tab Selection
	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectProfileTab();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectItemTab();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectSkinTab();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectPandoraTab();

	UFUNCTION(BlueprintCallable, Category = "!UI|Info")
	void SelectTabByLeftTag(FGameplayTag LeftUiTag);

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	URightInventoryWidget* GetRightInventoryWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	URightStatusWidget* GetRightStatusWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	ULeftEquipmentWidget* GetLeftEquipmentWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	ULeftSkinWidget* GetLeftSkinWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	ULeftPandoraWidget* GetLeftPandoraWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	URightSkinWidget* GetRightSkinWidget() const;

	UFUNCTION(BlueprintPure, Category = "!UI|Info")
	URightPandoraWidget* GetRightPandoraWidget() const;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnClickedInfoCenterButton OnClickedInfoCenterButton;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Info")
	FPdOnClickedMapButton OnClickedMapButton;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Root Widgets
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> LeftWidgetSwitcher;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> RightWidgetSwitcher;

	//------------------------------------------------------------------------------------------------------------------
	//--- Tab Buttons
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> ProfileTabButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> ItemTabButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> SkinButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> PandoraButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<UButton> MapButton;

	//------------------------------------------------------------------------------------------------------------------
	//--- Left Pages
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftProfileWidget> WB_LeftProfile;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftEquipmentWidget> WB_LeftEquipment;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftSkinWidget> WB_LeftSkin;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<ULeftPandoraWidget> WB_LeftPandora;

	//------------------------------------------------------------------------------------------------------------------
	//--- Right Pages
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightStatusWidget> WB_RightStatus;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightInventoryWidget> WB_RightInventory;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightSkinWidget> WB_RightSkin;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Info", meta = (BindWidget))
	TObjectPtr<URightPandoraWidget> WB_RightPandora;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Button Callbacks
	UFUNCTION()
	void OnProfileTabButtonClicked();

	UFUNCTION()
	void OnItemTabButtonClicked();

	UFUNCTION()
	void OnSkinButtonClicked();

	UFUNCTION()
	void OnPandoraButtonClicked();

	UFUNCTION()
	void OnMapButtonClicked();

	void SelectInfoCenterPage(UWidget* LeftWidget, UWidget* RightWidget, const FGameplayTag& LeftUiTag, const FGameplayTag& RightUiTag);
	FGameplayTag GetProfileLeftUiTag() const;
	FGameplayTag GetProfileRightUiTag() const;
	FGameplayTag GetItemLeftUiTag() const;
	FGameplayTag GetItemRightUiTag() const;
	FGameplayTag GetSkinLeftUiTag() const;
	FGameplayTag GetSkinRightUiTag() const;
	FGameplayTag GetPandoraLeftUiTag() const;
	FGameplayTag GetPandoraRightUiTag() const;
};
