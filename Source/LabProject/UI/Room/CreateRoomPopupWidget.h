#pragma once

#include "CoreMinimal.h"
#include "Common/GameSessionConstants.h"
#include "UI/Widget/LocalizedMenuWidget.h"
#include "CreateRoomPopupWidget.generated.h"

class UButton;
class UConnectingPopupWidget;
class UEditableTextBox;
class UUiSubsystem;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UCreateRoomPopupWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleCreateLoadingCancel();

	void HandleCreateSessionComplete(uint64 RequestId, bool bWasSuccessful);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void OpenLobbyAsListenServer() const;
	FString GetRoomNameInput() const;

private:
	void ApplyWidgetDefinitionSettings();
	UButton* GetCreateButton() const;
	UEditableTextBox* GetRoomNameTextBox() const;
	FString GetResolvedLobbyTravelMapName() const;
	UUiSubsystem* GetUiSubsystem() const;
	UConnectingPopupWidget* ShowConnectingPopup(bool bShowCancelButton);
	void HideConnectingPopup() const;

protected:
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

private:
	FDelegateHandle CreateSessionCompleteHandle;
	uint64 ActiveCreateRequestId = 0;
};
