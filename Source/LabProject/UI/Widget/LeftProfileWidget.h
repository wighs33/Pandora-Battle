#pragma once

#include "Blueprint/UserWidget.h"
#include "LeftProfileWidget.generated.h"

class UButton;
class UImage;
class UUserWidget;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ULeftProfileWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UButton> AchievementButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UButton> AchievementButton_1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UButton> AchievementButton_2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UButton> AchievementButton_3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UButton> AchievementButton_4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UButton> AchievementButton_5;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UImage> AchievementImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UImage> AchievementImage_1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UImage> AchievementImage_2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UImage> AchievementImage_3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UImage> AchievementImage_4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UImage> AchievementImage_5;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UImage> PlayerAchieveIcon;

private:
	UFUNCTION()
	void HandleAchievementButtonClicked();

	UFUNCTION()
	void HandleAchievementButtonClicked_1();

	UFUNCTION()
	void HandleAchievementButtonClicked_2();

	UFUNCTION()
	void HandleAchievementButtonClicked_3();

	UFUNCTION()
	void HandleAchievementButtonClicked_4();

	UFUNCTION()
	void HandleAchievementButtonClicked_5();

	void BindAchievementButtons();
	void UnbindAchievementButtons();
	void ApplyAchievementIcon(int32 AchievementIndex);
	UButton* GetAchievementButton(int32 AchievementIndex) const;
	UImage* GetAchievementImage(int32 AchievementIndex) const;
	UImage* FindHudPlayerAvatarImage() const;
	UImage* FindImageInUserWidget(UUserWidget* RootWidget, FName ImageName) const;
	UImage* FindImageInWidget(UWidget* RootWidget, FName ImageName) const;
};
