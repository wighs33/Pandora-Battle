#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "Components/ComboBoxString.h"
#include "GameConfigWidget.generated.h"

class UButton;
class UEditableTextBox;
class UImage;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UGameConfigWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|Config")
	void RefreshUI();

	UFUNCTION(BlueprintCallable, Category = "!Lobby|Config")
	void SaveConfig();

	UFUNCTION(BlueprintPure, Category = "!Lobby|Config")
	FName GetSelectedMapKey() const;

protected:
	virtual void OnMenuLanguageChanged() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "!Lobby|Config")
	void BP_OnSelectedMapChanged(FName SelectedMapKey);

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleMapSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Back;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UComboBoxString> ComboBox_Map;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UEditableTextBox> Editable_MaxPlayerCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UEditableTextBox> Editable_MaxBotCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UImage> Img_MapThumbnail_WindNest;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UImage> Img_MapThumbnail_Test;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Config")
	FName WindNestMapKey = TEXT("Wind Nest");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Config")
	FName TestMapKey = TEXT("Test");

private:
	FName GetFirstComboBoxMapKey() const;
	UFUNCTION() UWidget* GenerateMapOption(FString Option);
	void ApplySelectedMapThumbnail(FName SelectedMapKey);
	static int32 ParseClampedInt(const UEditableTextBox* TextBox, int32 DefaultValue, int32 MinValue, int32 MaxValue);
};
