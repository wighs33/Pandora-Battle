#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "GameConfigWidget.generated.h"

class UButton;
class UEditableTextBox;
class UImage;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UGameConfigWidget : public UUserWidget
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
	void ApplySelectedMapThumbnail(FName SelectedMapKey);
	static int32 ParseClampedInt(const UEditableTextBox* TextBox, int32 DefaultValue, int32 MinValue, int32 MaxValue);
};
