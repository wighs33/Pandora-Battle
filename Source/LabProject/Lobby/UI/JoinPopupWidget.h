#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "JoinPopupWidget.generated.h"

class UButton;
class UEditableTextBox;
class UUserWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UJoinPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleCancelClicked();

	void RemoveConnectingPopup();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Join;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Cancel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UEditableTextBox> Editable_InputIP;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI")
	TSubclassOf<UUserWidget> ConnectingPopupWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|UI", meta = (ClampMin = "0.0"))
	float ConnectingPopupLifetime = 5.0f;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ConnectingPopupWidget;

	FTimerHandle ConnectingPopupTimerHandle;
};
