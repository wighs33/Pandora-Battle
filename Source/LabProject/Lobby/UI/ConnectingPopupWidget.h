#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ConnectingPopupWidget.generated.h"

class UButton;
class UWidgetAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FConnectingPopupCanceledSignature);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UConnectingPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UConnectingPopupWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Lobby|Connecting")
	void SetCancelButtonEnabled(bool bEnabled);

	UPROPERTY(BlueprintAssignable, Category = "!Lobby|Connecting")
	FConnectingPopupCanceledSignature OnCanceled;

protected:
	UFUNCTION()
	void HandleCancelClicked();

	void ApplyCancelButtonState() const;
	void PlayWaitAnimation();
	void StopWaitAnimation();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Lobby|Bind")
	TObjectPtr<UButton> Btn_Cancel;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional), Category = "!Lobby|Animation")
	TObjectPtr<UWidgetAnimation> WaitAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Connecting")
	bool bCancelButtonEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Lobby|Animation", meta = (ClampMin = "0.01"))
	float WaitAnimationPlaybackSpeed = 1.0f;
};
