#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "LeftProfileWidget.generated.h"

class APdPlayerState;
class UAchievementDefinition;
class UButton;
struct FStreamableHandle;
class UImage;
class URecordDefinition;
class UTextBlock;
class UUserWidget;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ULeftProfileWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	ULeftProfileWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|Profile|Tier")
	void RefreshTierImage();

	UFUNCTION(BlueprintCallable, Category = "!UI|Profile|Achievement")
	void RefreshAchievementButtons();

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

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

	UFUNCTION()
	void HandleAchievementButtonClicked_6();
	void HandleSteamAchievementStateChanged();
	void HandlePlayerNameRefreshRetry();
	void HandleMatchDisplayNameChanged(const FText& NewDisplayName);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BindAchievementButtons();
	void UnbindAchievementButtons();
	void BindSteamAchievementStateChanged();
	void UnbindSteamAchievementStateChanged();
	void BeginContentPreload();
	void BeginPresentationPreload(int32 PreloadGeneration);
	void ReleaseContentPreloads();
	void BindMatchDisplayNameChanged();
	void UnbindMatchDisplayNameChanged();
	bool RefreshPlayerName();
	void SchedulePlayerNameRefreshRetry();
	void ApplyAchievementIcon(int32 AchievementIndex);
	void RefreshSelectedAchievementIcon();
	void ApplyAchievementBrush(int32 AchievementIndex);
	void RefreshAchievementMessage();
	void UpdateAchievementSelection(int32 AchievementIndex);
	int32 FindAchievementIndexById(FName AchievementId) const;
	bool IsAchievementUnlocked(int32 AchievementIndex) const;
	UButton* GetAchievementButton(int32 AchievementIndex) const;
	UImage* GetAchievementImage(int32 AchievementIndex) const;
	UImage* FindHudPlayerAvatarImage() const;
	UImage* FindImageInUserWidget(UUserWidget* RootWidget, FName ImageName) const;
	UImage* FindImageInWidget(UWidget* RootWidget, FName ImageName) const;
	FString ResolveProfileSavePlayerId() const;
	const URecordDefinition* ResolveRecordDefinition();
	const UAchievementDefinition* ResolveAchievementDefinition() const;

protected:
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
	TObjectPtr<UButton> AchievementButton_6;

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
	TObjectPtr<UImage> AchievementImage_6;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Achievement")
	TObjectPtr<UImage> PlayerAchieveIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile")
	TObjectPtr<UTextBlock> Txt_PlayerName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Tier")
	TObjectPtr<UTextBlock> Txt_WinCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Profile|Tier")
	TObjectPtr<UImage> Img_Tier;

private:
	TWeakObjectPtr<APdPlayerState> BoundPlayerState;
	FDelegateHandle MatchDisplayNameChangedHandle;
	FDelegateHandle SteamAchievementStateChangedHandle;
	FTimerHandle PlayerNameRefreshRetryTimerHandle;
	int32 PlayerNameRefreshRetryCount = 0;
	int32 ContentPreloadGeneration = 0;
	bool bAchievementQueryPending = false;
	TSharedPtr<FStreamableHandle> DefinitionPreloadHandle;
	TSharedPtr<FStreamableHandle> PresentationPreloadHandle;
};
