#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FindSessionsCallbackProxy.h"
#include "RoomListWidget.generated.h"

class UAudioVolumeSlider;
class UAudioVolumeControl;
class UButton;
class UConnectingPopupWidget;
class UCreateRoomPopupWidget;
class URoomItemWidget;
class UUiSubsystem;
class UWrapBox;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URoomListWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Room")
	void SetInfo();

	UFUNCTION(BlueprintCallable, Category = "!Room")
	void RefreshUI();

protected:
	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleCreateGameClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleRefreshCancel();

	void HandleFindSessionsComplete(
		uint64 RequestId,
		const TArray<FBlueprintSessionResult>& Results,
		bool bWasSuccessful);
	void HandleDestroySessionForClose(uint64 RequestId, bool bWasSuccessful);
	void OpenTitleMap() const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UWrapBox> RoomList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UButton> Btn_Refresh;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UButton> Btn_CreateGame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UButton> Btn_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UAudioVolumeSlider> AudioVolumeSlider_;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UButton> Btn_Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|UI")
	TSubclassOf<URoomItemWidget> RoomItemWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|UI")
	TSubclassOf<UCreateRoomPopupWidget> CreateRoomPopupWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|UI", meta = (ClampMin = "1"))
	int32 MaxRoomSlots = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|Session", meta = (ClampMin = "1"))
	int32 MaxSearchResults = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|Session")
	bool bSearchLAN = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|Session")
	bool bUseLobbies = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Room|UI")
	TArray<TObjectPtr<URoomItemWidget>> Rooms;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Room|Session")
	TArray<FBlueprintSessionResult> SessionInfos;

private:
	void ApplyWidgetDefinitionSettings();
	FString GetResolvedTitleTravelMapName() const;
	UUiSubsystem* GetUiSubsystem() const;
	UConnectingPopupWidget* ShowConnectingPopup(bool bShowCancelButton);
	void HideConnectingPopup() const;

	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
	uint64 ActiveFindRequestId = 0;
	uint64 ActiveDestroyRequestId = 0;
	bool bPendingCloseAfterDestroy = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioVolumeControl> AudioVolumeControl;
};
