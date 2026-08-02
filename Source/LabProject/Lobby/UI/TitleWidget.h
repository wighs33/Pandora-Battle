#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "Blueprint/UserWidget.h"
#include "FindSessionsCallbackProxy.h"
#include "TitleWidget.generated.h"

class UButton;
class UAudioVolumeSlider;
class UAudioVolumeControl;
class UConnectingPopupWidget;
class UGuideWidget;
class UShopWidget;
class UUiSubsystem;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UTitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTitleWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	UFUNCTION()
	void HandleRoomListClicked();

	UFUNCTION()
	void HandleQuickMatchClicked();

	UFUNCTION()
	void HandleTrainingModeClicked();

	UFUNCTION()
	void HandlePandoraShopClicked();

	UFUNCTION()
	void HandleGuideClicked();

	UFUNCTION()
	void HandleRecordClicked();

	UFUNCTION()
	void HandleExitClicked();

	UFUNCTION()
	void HandleQuickMatchCancel();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_RoomList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_QuickMatch;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_TrainingMode;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_PandoraShop;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Guide;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Record;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Exit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Tutorial;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UAudioVolumeSlider> AudioVolumeSlider_;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Travel")
	TSoftObjectPtr<UWorld> LobbyMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Travel")
	FString LobbyTravelMapName = TEXT("/Game/Map/LV_Lobby");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Travel")
	TSoftObjectPtr<UWorld> RoomMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Travel")
	FString RoomTravelMapName = TEXT("/Game/Map/LV_Room");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Travel")
	TSoftObjectPtr<UWorld> TrainingRoomMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Travel")
	FString TrainingRoomTravelMapName = TEXT("/Game/Map/LV_TrainingRoom");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<UShopWidget> ShopWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<UUserWidget> GuideWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<UUserWidget> RecordWidgetClass;

private:
	void ApplyWidgetDefinitionSettings();
	FString GetResolvedLobbyTravelMapName() const;
	FString GetResolvedRoomTravelMapName() const;
	FString GetResolvedTrainingRoomTravelMapName() const;
	void OpenRoomList();
	void OpenTrainingRoom();
	void OpenShop();
	void OpenGuide();
	void OpenRecord();
	void BindRecordCloseButton();
	void UnbindRecordCloseButton();
	UButton* FindRecordCloseButton() const;
	FString ResolveTitleSavePlayerId() const;
	void ResolveWidgets();
	TSubclassOf<UShopWidget> ResolveShopWidgetClass() const;
	TSubclassOf<UUserWidget> ResolveGuideWidgetClass() const;
	TSubclassOf<UUserWidget> ResolveRecordWidgetClass() const;
	void EnsurePreferredSaveGameLoaded() const;
	void StartQuickMatch();
	void TryStartQuickMatchAfterLoadingScreen();
	void BeginQuickMatchRequest();
	void CancelQuickMatchStartTimer();
	void OpenLobbyAsListenServer() const;
	void SetQuickMatchEnabled(bool bEnabled) const;
	void ClearQuickMatchDelegates();
	void HandleQuickMatchRequestComplete(uint64 RequestId, bool bWasSuccessful, bool bCreatedRoom);

	UFUNCTION()
	void HandleRecordCloseClicked();

	UUiSubsystem* GetUiSubsystem() const;
	UConnectingPopupWidget* ShowQuickMatchLoadingScreen();
	void HideConnectingPopup() const;
	void HideQuickMatchLoadingScreen() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 QuickMatchMaxSearchResults = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 QuickMatchMaxPublicConnections = LabGameSession::MaxPlayerCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true"))
	FString QuickMatchRoomName = TEXT("Quick Match");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true"))
	bool bQuickMatchLAN = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|QuickMatch", meta = (AllowPrivateAccess = "true"))
	bool bQuickMatchUseLobbies = true;

	FDelegateHandle QuickMatchRequestCompleteHandle;
	FTimerHandle QuickMatchStartTimerHandle;
	uint64 ActiveQuickMatchRequestId = 0;
	bool bQuickMatchStartPending = false;

	UPROPERTY(Transient)
	TObjectPtr<UShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> GuideWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> RecordWidget;

	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeControl> AudioVolumeControl;
};
