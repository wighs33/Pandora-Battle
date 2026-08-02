#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "PlayerVitalsWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
struct FOnAttributeChangeData;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPlayerVitalsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Player Vitals")
	TObjectPtr<UProgressBar> StaminaBar;

private:
	void InitializeStaminaPresentation();
	void QueueStaminaPresentationInitializeRetry();
	void BindStaminaAttributeDelegates();
	void UnbindStaminaAttributeDelegates();
	void BeginGameSettingContentPreload();
	void ReleaseGameSettingContentPreload();
	void RefreshStaminaFillTint();
	UProgressBar* ResolveStaminaBar() const;
	UAbilitySystemComponent* ResolveOwnerAbilitySystemComponent() const;
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void HandleMaxStaminaChanged(const FOnAttributeChangeData& Data);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle StaminaChangedDelegateHandle;
	FDelegateHandle MaxStaminaChangedDelegateHandle;
	FTimerHandle StaminaPresentationInitializeTimerHandle;
	FLinearColor NormalStaminaFillTint = FLinearColor::White;
	float CurrentStamina = 0.0f;
	float CurrentMaxStamina = 0.0f;
	int32 StaminaPresentationInitializeRetryCount = 0;
	int32 GameSettingContentPreloadGeneration = 0;
	bool bHasCachedNormalStaminaFillTint = false;

	static constexpr int32 MaxStaminaPresentationInitializeRetryCount = 20;
};
