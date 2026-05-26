#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AbilitySlotWidget.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UImage;
class UInputAction;
class UOverlay;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;
struct FSkill;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UAbilitySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void SetAbilitySpecHandle(FGameplayAbilitySpecHandle InAbilitySpecHandle);

	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void SetAbilitySlotData(FGameplayAbilitySpecHandle InAbilitySpecHandle, FText InDisplayNameOverride, UObject* InIconOverride);

	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void SetAbilitySlotDataEnabled(FGameplayAbilitySpecHandle InAbilitySpecHandle, FText InDisplayNameOverride, UObject* InIconOverride, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void SetAbilityDisplayOverride(FText InDisplayNameOverride, UObject* InIconOverride);

	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void SetAbilitySlotEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "!UI|Ability")
	FGameplayAbilitySpecHandle GetAbilitySpecHandle() const { return AbilitySpecHandle; }

	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void SetAbilityImage();

	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void SetInputKeyIcon();

	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void CheckForCooldown();

	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void CheckForActivation();

	UFUNCTION(BlueprintCallable, Category = "!UI|Ability")
	void UpdateCooldownProgress();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void InitializeAbilityObject();
	void BindGameplayTagEvents();
	void UnbindGameplayTagEvents();
	void ClearCooldownTimer();
	void HandleCooldownTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void HandleGameplayAbilityTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void SetInputKeyRenderOpacity(float InOpacity) const;
	void ApplyAbilitySlotEnabledState();

	UObject* ResolveAbilityImage() const;
	FText ResolveAbilityDisplayName() const;
	const FSkill* ResolvePandoraSkill() const;
	UInputAction* ResolveInputAction() const;
	UObject* ResolveInputIconObject() const;

	static FSlateBrush MakeImageBrush(UObject* ResourceObject);
	static FSlateBrush MakeImageBrushFromExisting(const FSlateBrush& ExistingBrush, UObject* ResourceObject, FVector2D ImageSize);
	static float CalculateCooldownPercent(float TimeRemaining, double CooldownDuration);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability", meta = (ExposeOnSpawn = "true", AllowPrivateAccess = "true"))
	FGameplayAbilitySpecHandle AbilitySpecHandle;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Ability", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGameplayAbility> AbilityObjectRef;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Ability", meta = (AllowPrivateAccess = "true"))
	bool bHasAbilityDisplayOverride = false;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Ability", meta = (AllowPrivateAccess = "true"))
	FText AbilityDisplayNameOverride;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Ability", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UObject> AbilityIconOverride;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Ability", meta = (AllowPrivateAccess = "true"))
	bool bAbilitySlotEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Disabled", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float DisabledSlotOpacity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTexture2D> DefaultAbilityImage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Cooldown", meta = (AllowPrivateAccess = "true"))
	bool bShowCooldownTimeRemaining = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input", meta = (AllowPrivateAccess = "true"))
	bool bHideInputKeyIcon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input", meta = (AllowPrivateAccess = "true"))
	float ReadyInputKeyOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input", meta = (AllowPrivateAccess = "true"))
	float CooldownInputKeyOpacity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input", meta = (AllowPrivateAccess = "true"))
	FLinearColor ActiveInputKeyColor = FLinearColor(1.0f, 0.78f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input", meta = (AllowPrivateAccess = "true"))
	FLinearColor InactiveInputKeyColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input", meta = (AllowPrivateAccess = "true"))
	FVector2D InputKeyIconSize = FVector2D(32.0f, 32.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input", meta = (AllowPrivateAccess = "true"))
	TMap<FString, TObjectPtr<UObject>> StringToIconMapping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input Actions", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Skill1InputAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input Actions", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Skill2InputAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input Actions", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Skill3InputAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Ability|Input Actions", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Skill4InputAction;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Ability|Cooldown", meta = (AllowPrivateAccess = "true"))
	double TotalCooldownTime = 0.0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> AbilityImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AbilityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CooldownProgress;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> CooldownTimerContainer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TimerText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> AbilityActiveFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UOverlay> InputKeyOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> InputKeyBackground;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> KeyIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> InputKeyIcon;

	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;
	FDelegateHandle CooldownTagChangedHandle;
	FDelegateHandle GameplayAbilityTagChangedHandle;
	FTimerHandle UpdateCooldownTimerHandle;
};
