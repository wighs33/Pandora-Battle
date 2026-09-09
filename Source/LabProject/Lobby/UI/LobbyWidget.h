#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "Blueprint/UserWidget.h"
#include "Definition/Level/LevelDefinition.h"
#include "LobbyWidget.generated.h"

class APdPlayerState;
class ALobbyGameState;
class UAudioVolumeSlider;
class UAudioVolumeControl;
class UButton;
class UConnectingPopupWidget;
class UGameConfigWidget;
class UImage;
class ULobbyUserWidget;
class UTextBlock;
class UUiSubsystem;
class UVerticalBox;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ULobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	void SetInfo();

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	void RefreshUI();

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	void StartGameCountdown(float DelaySeconds);

	UFUNCTION(BlueprintCallable, Category = "!Lobby|UI")
	void HideGameCountdown();

	UFUNCTION(BlueprintPure, Category = "!Lobby|UI")
	TArray<APdPlayerState*> GetLobbyPlayerStates() const;

	bool CloseTopmostUiForEscape();

protected:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleGameConfigClicked();

	UFUNCTION()
	void HandleGameStartClicked();

	UFUNCTION()
	void HandleEnterClicked();

	UFUNCTION()
	void HandleInviteClicked();

	UFUNCTION()
	void HandleMapPreviousClicked();

	UFUNCTION()
	void HandleMapNextClicked();

	void HandleDestroySessionForClose(bool bWasSuccessful);
	void DestroySessionForClose();
	void SendRemoteClientsToTitleMap(const FString& TitleMapName) const;
	void TravelToTitleMap() const;
	void ApplyLobbyInputPassthroughVisibility();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UVerticalBox> UserList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_GameConfig;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_GameStart;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Enter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Invite;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_MapPrevious;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_MapNext;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UAudioVolumeSlider> AudioVolumeSlider_;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Sound;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UTextBlock> Txt_SelectedMapName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UTextBlock> Txt_SelectedMapPlayerCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UImage> Img_SelectedMapThumbnail;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UWidget> GameStartCountdownRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UTextBlock> Txt_GameStartCountdown;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UTextBlock> Txt_Warning;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UWidget> TeamBalanceWarningRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<ULobbyUserWidget> LobbyUserWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<UGameConfigWidget> GameConfigWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI", meta = (ClampMin = "1"))
	int32 MaxLobbySlots = LabGameSession::MaxPlayerCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Countdown")
	FText GameStartCountdownFormatText = NSLOCTEXT("Lobby", "GameStartCountdownFormatText", "{Seconds}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Countdown")
	FText GameStartCountdownFinishedText = NSLOCTEXT("Lobby", "GameStartCountdownFinishedText", "Start");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Countdown", meta = (ClampMin = "0.01"))
	float GameStartCountdownTickInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Team", meta = (MultiLine = "true",
		ToolTip = "Optional override for Txt_Warning. If empty, the text written on Txt_Warning in the widget designer is used."))
	FText TeamBalanceWarningText;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Lobby|UI")
	TArray<TObjectPtr<ULobbyUserWidget>> LobbyUsers;

	UPROPERTY(Transient)
	TObjectPtr<UGameConfigWidget> ActiveGameConfigWidget;

private:
	bool RebuildPlayerSlots();
	void ApplyWidgetDefinitionSettings();
	FString GetResolvedTitleTravelMapName() const;
	UUiSubsystem* GetUiSubsystem() const;
	UWidget* FindGameStartCountdownRoot() const;
	UTextBlock* FindGameStartCountdownText() const;
	UWidget* FindTeamBalanceWarningRoot() const;
	UTextBlock* FindTeamBalanceWarningText() const;
	UButton* FindEnterButton() const;
	UButton* FindMapPreviousButton() const;
	UButton* FindMapNextButton() const;
	UTextBlock* FindSelectedMapNameText() const;
	UTextBlock* FindSelectedMapPlayerCountText() const;
	UImage* FindSelectedMapThumbnailImage() const;
	bool GetSelectedMapOptionForUI(FLobbyMatchMapOption& OutMapOption) const;
	int32 GetMaxLobbySlotsForUI() const;
	void RefreshSelectedMapUI();
	bool AreLobbyTeamsBalancedForUI(const TArray<APdPlayerState*>& LobbyPlayerStates) const;
	void SetGameStartCountdownVisibility(ESlateVisibility InVisibility);
	void SetTeamBalanceWarningVisibility(ESlateVisibility InVisibility);
	void RefreshGameStartCountdownUI();
	void HandleGameStartCountdownTick();
	void ApplyReplicatedGameStartState();
	void SetLobbyInteractionsLocked(bool bLocked);
	FText FormatGameStartCountdownText() const;
	const ALobbyGameState* GetLobbyGameState() const;
	bool IsGameStartPending() const;
	float GetGameStartRemainingSeconds() const;
	void ShowConnectingPopup(bool bShowCancelButton) const;
	void HideConnectingPopup() const;

	FDelegateHandle DestroySessionCompleteHandle;
	FTimerHandle CloseDestroyTimerHandle;
	FTimerHandle GameStartCountdownTickHandle;
	bool bPendingCloseAfterDestroy = false;
	FText DefaultTeamBalanceWarningText;
	FSlateColor DefaultSelectedMapPlayerCountColor;
	bool bHasDefaultSelectedMapPlayerCountColor = false;
	TMap<TWeakObjectPtr<UWidget>, bool> LobbyInteractionEnabledStates;

	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeControl> AudioVolumeControl;
};
