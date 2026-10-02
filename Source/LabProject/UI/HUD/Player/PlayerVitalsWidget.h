#pragma once

#include "CoreMinimal.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "AttributeSet.h"
#include "Component/Character/AbilitySystemReadySubscription.h"
#include "PlayerVitalsWidget.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
class UProgressBar;
struct FOnAttributeChangeData;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPlayerVitalsWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

private:
	void HandlePossessedCharacterReady(ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent);
	void HandleResourceChanged(const FOnAttributeChangeData& Data);
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void HandleMaxStaminaChanged(const FOnAttributeChangeData& Data);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BindStaminaAttributeDelegates();
	void UnbindStaminaAttributeDelegates();
	void BeginGameSettingContentPreload();
	void ReleaseGameSettingContentPreload();
	void RefreshStaminaFillTint();
	UProgressBar* ResolveStaminaBar() const;
	void RefreshResourceReadouts();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Player Vitals")
	TObjectPtr<UProgressBar> StaminaBar;

private:
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> ResourceDelegateHandles;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle StaminaChangedDelegateHandle;
	FDelegateHandle MaxStaminaChangedDelegateHandle;
	FAbilitySystemReadySubscription PossessedCharacterReadySubscription;
	FLinearColor NormalStaminaFillTint = FLinearColor::White;
	float CurrentStamina = 0.0f;
	float CurrentMaxStamina = 0.0f;
	int32 GameSettingContentPreloadGeneration = 0;
	bool bHasCachedNormalStaminaFillTint = false;
};
