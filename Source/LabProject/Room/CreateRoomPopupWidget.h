#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "Blueprint/UserWidget.h"
#include "CreateRoomPopupWidget.generated.h"

class UButton;
class UConnectingPopupWidget;
class UEditableTextBox;
class UUiSubsystem;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UCreateRoomPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleCreateLoadingCancel();

	void HandleCreateSessionComplete(uint64 RequestId, bool bWasSuccessful);
	void OpenLobbyAsListenServer() const;
	FString GetRoomNameInput() const;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UButton> Btn_Create;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UButton> Btn_Cancel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Room|Bind")
	TObjectPtr<UEditableTextBox> TxtBox_InputGameName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|Session")
	FString DefaultRoomName = TEXT("New Room");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|Session", meta = (ClampMin = "1"))
	int32 MaxPublicConnections = LabGameSession::MaxPlayerCount;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|Session")
	bool bCreateLAN = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|Travel")
	TSoftObjectPtr<UWorld> LobbyMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Room|Travel")
	FString LobbyTravelMapName = TEXT("/Game/Map/LV_Lobby");

private:
	void ApplyWidgetDefinitionSettings();
	UButton* GetCreateButton() const;
	UEditableTextBox* GetRoomNameTextBox() const;
	FString GetResolvedLobbyTravelMapName() const;
	UUiSubsystem* GetUiSubsystem() const;
	UConnectingPopupWidget* ShowConnectingPopup(bool bShowCancelButton);
	void HideConnectingPopup() const;

	FDelegateHandle CreateSessionCompleteHandle;
	uint64 ActiveCreateRequestId = 0;
};
