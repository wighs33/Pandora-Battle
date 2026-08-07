#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Definition/Player/CharacterActionDefinition.h"
#include "ActionSlotEntryWidget.generated.h"

class APdPlayer;
class UAbilitySystemComponent;
class UGameplayEffect;
class UImage;
class UInputAction;
class UOverlay;
class UProgressBar;
class UTextBlock;
class UWidget;
struct FActiveGameplayEffect;
struct FActiveGameplayEffectHandle;
struct FGameplayEffectSpec;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UActionSlotEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|ActionSlot")
	void SetActionSlotData(int32 InSlotIndex, ECharacterActionType InActionType, UCharacterActionDefinition* InActionDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI|ActionSlot")
	void RefreshVisual();

	UFUNCTION(BlueprintCallable, Category = "!UI|ActionSlot|Cooldown")
	void CheckForCooldown();

	UFUNCTION(BlueprintCallable, Category = "!UI|ActionSlot|Cooldown")
	void UpdateCooldownProgress();

	UFUNCTION(BlueprintPure, Category = "!UI|ActionSlot")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure, Category = "!UI|ActionSlot")
	ECharacterActionType GetActionType() const { return ActionType; }

	UFUNCTION(BlueprintPure, Category = "!UI|ActionSlot")
	const UCharacterActionDefinition* GetActionDefinition() const { return ActionDefinition; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void CacheOptionalWidgets();
	void ApplyWidgetDefinitionSettings();
	void ApplyActionVisual();
	void ApplyInputKeyIcon();
	void BindAbilityCooldownChanged();
	void UnbindAbilityCooldownChanged();
	void RetryBindAbilityCooldown();
	void ScheduleAbilityCooldownBindingRetry();
	void ClearAbilityCooldownBindingRetry();
	void ClearCooldownTimer();
	void HandleAbilityCooldownTagChanged(FGameplayTag ChangedTag, int32 NewCount);
	void HandleAbilityCooldownEffectAdded(
		UAbilitySystemComponent* TargetAbilitySystemComponent,
		const FGameplayEffectSpec& AppliedSpec,
		FActiveGameplayEffectHandle ActiveHandle);
	void HandleAbilityCooldownEffectRemoved(const FActiveGameplayEffect& RemovedEffect);
	void SetInputKeyRenderOpacity(float InOpacity) const;

	APdPlayer* ResolveOwningPlayerCharacter() const;
	UObject* ResolveActionIcon() const;
	UInputAction* ResolveInputAction() const;
	UObject* ResolveInputIconObject() const;
	FGameplayTag ResolveAbilityCooldownTag() const;
	bool ResolveAbilityCooldownTiming(float& OutTimeRemaining, float& OutDuration) const;
	bool IsAbilityCooldownSpec(const FGameplayEffectSpec& GameplayEffectSpec) const;
	float ResolveCooldownTimeRemaining() const;
	double ResolveConfiguredCooldownDuration() const;

	static float CalculateCooldownPercent(float TimeRemaining, double CooldownDuration);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot", meta = (AllowPrivateAccess = "true"))
	ECharacterActionType ActionType = ECharacterActionType::PandoraWeaponSwap;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|ActionSlot", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterActionDefinition> ActionDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot|Cooldown", meta = (AllowPrivateAccess = "true"))
	bool bShowCooldownTimeRemaining = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot|Input", meta = (AllowPrivateAccess = "true"))
	float ReadyInputKeyOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot|Input", meta = (AllowPrivateAccess = "true"))
	float CooldownInputKeyOpacity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot|Input", meta = (AllowPrivateAccess = "true"))
	bool bHideInputKeyIcon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot|Input", meta = (AllowPrivateAccess = "true"))
	FVector2D InputKeyIconSize = FVector2D(32.0f, 32.0f);

	UPROPERTY(BlueprintReadOnly, Category = "!UI|ActionSlot|Cooldown", meta = (AllowPrivateAccess = "true"))
	double TotalCooldownTime = 0.0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CooldownProgress;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> CooldownTimerContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimerText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> InputKeyOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> KeyIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActionActiveFrame;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	FGameplayTag BoundAbilityCooldownTag;
	FDelegateHandle AbilityCooldownChangedHandle;
	FDelegateHandle AbilityCooldownEffectAddedHandle;
	FDelegateHandle AbilityCooldownEffectRemovedHandle;
	FTimerHandle AbilityCooldownBindingRetryTimerHandle;
	FTimerHandle UpdateCooldownTimerHandle;
};
