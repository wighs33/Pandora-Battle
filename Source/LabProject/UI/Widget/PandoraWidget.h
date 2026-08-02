#pragma once

#include "Blueprint/UserWidget.h"
#include "Common/Enum_Direction.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "TimerManager.h"

#include "PandoraWidget.generated.h"

class UButton;
class UImage;
class UProgressBar;
class UTextBlock;
class UPandoraComponent;
class UPandoraTreeComponent;
class UPandoraDefinition;
class UPandoraWidget;
class UPandoraWidgetViewModel;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FPandoraWidgetInteractionSignature,
	UPandoraWidget*,
	PandoraWidget);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPandoraWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition);

	UFUNCTION(BlueprintPure, Category = "!UI|Pandora")
	UPandoraDefinition* GetPandoraDefinition() const { return PandoraDefinition.Get(); }

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraTreeComponent(UPandoraTreeComponent* InPandoraTreeComponent);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraInfo();

	const UWidget* GetPandoraDescriptionAnchorWidget() const;
	bool IsPandoraDescriptionRequested() const { return bIsPandoraDescriptionRequested; }

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora|Button Hold")
	void ConfirmSpendPointOnPandora();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora|Button Hold")
	void IncrementButtonTimer();

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora|Button Hold")
	void ResetButtonPress();

	UPROPERTY(BlueprintAssignable, Category = "!UI|Pandora|Interaction")
	FPandoraWidgetInteractionSignature OnPandoraDescriptionRequested;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Pandora|Interaction")
	FPandoraWidgetInteractionSignature OnPandoraDescriptionDismissed;

	UPROPERTY(BlueprintAssignable, Category = "!UI|Pandora|Interaction")
	FPandoraWidgetInteractionSignature OnPandoraTreeFocusRequested;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Widgets")
	TObjectPtr<UButton> Button;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Widgets")
	TObjectPtr<UProgressBar> ButtonProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Equip Hint")
	TObjectPtr<UTextBlock> Txt_Equip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Equip Hint")
	TObjectPtr<UWidget> InputKeyOverlay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Equip Hint")
	TObjectPtr<UImage> InputKeyBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Equip Hint")
	TObjectPtr<UImage> KeyIcon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Pandora")
	TObjectPtr<UPandoraDefinition> PandoraDefinition;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|Internal")
	TObjectPtr<UPandoraTreeComponent> PandoraTreeComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FLinearColor AvailableOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FLinearColor UnavailableOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FLinearColor LockedOverlayColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FLinearColor NotEnoughPointsOverlayColor = FLinearColor(0.75f, 0.0f, 0.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AvailableContentOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UnavailableContentOpacity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	bool bShowMaxText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Button Hold", meta = (ClampMin = "0.0"))
	double ButtonHoldDuration = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Button Hold", meta = (ClampMin = "0.001"))
	float ButtonHoldUpdateInterval = 0.033333f;

private:
	UFUNCTION()
	void HandleButtonPressed();

	UFUNCTION()
	void HandleButtonReleased();

	UFUNCTION()
	void HandleButtonHovered();

	UFUNCTION()
	void HandleButtonUnhovered();

	UFUNCTION()
	void HandlePandoraStateChanged();

	UFUNCTION()
	void HandlePandoraPointsChanged(int32 NewPointsAvailable);

	UFUNCTION()
	void HandlePandoraLoadoutChanged();

	void ResolvePandoraTreeComponent();
	void ResolvePandoraComponent();
	void ApplyWidgetDefinitionSettings();
	void BindPandoraTreeEvents();
	void UnbindPandoraTreeEvents();
	void BindPandoraComponentEvents();
	void UnbindPandoraComponentEvents();
	void BindButtonEvents();
	void UnbindButtonEvents();
	void ResolveControlWidgets();
	UPandoraWidgetViewModel* GetOrCreatePandoraWidgetViewModel();
	void ApplyPandoraWidgetViewModelToMvvmView();
	void ApplyDesignerDefaults();
	void ClearButtonPressTimer();
	bool IsPandoraUnlockedInSave() const;
	void RefreshPandoraDescriptionRequest(bool bForceRefresh = false);
	void ResolveEquipHintWidgets();
	void ApplyEquipHintDefaults();
	void SetEquipHintWidgetsVisible(bool bShowText, bool bShowInputKey);
	void RefreshEquipHintState(bool bHovered);
	bool CanShowEquipHint() const;
	bool CanAutoEquipPandora() const;
	bool IsPandoraEquipped() const;
	bool TryGetEquippedPandoraDirection(EEnum_Direction& OutDirection) const;
	bool HandlePandoraWidgetMouseButtonDown(const FPointerEvent& InMouseEvent);
	bool RequestAutoEquipPandora();
	bool RequestUnequipPandora();

	double ButtonHoldElapsedTime = 0.0;
	double ButtonHoldStartRealTime = 0.0;
	bool bIsButtonHoldActive = false;
	FTimerHandle ButtonHoldTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraComponent> PandoraComponent;

	FText DefaultEquipText;
	bool bHasCachedDefaultEquipText = false;
	bool bIsButtonHoverActive = false;
	bool bIsFocusWithinWidget = false;
	bool bIsPandoraDescriptionRequested = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPandoraWidgetViewModel> PandoraWidgetViewModel;
};
