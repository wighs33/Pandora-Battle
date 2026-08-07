#pragma once

#include "AttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "EnemyHealthBarWidget.generated.h"

class AActor;
class UAbilitySystemComponent;
class UProgressBar;
struct FOnAttributeChangeData;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Health")
	void SetOwnerActor(AActor* InOwnerActor);

	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Health")
	void UpdateHealthPercent();

	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Health")
	void AnimateHealth(double From, double To);

	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Health")
	void DecreaseHealthIncrement();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Enemy|Health")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Enemy|Health")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Enemy|Health")
	float MaxHealth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.001"), Category = "!UI|Enemy|Health")
	float AnimateHealthDecreaseIncrement = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"), Category = "!UI|Enemy|Health")
	float AnimateHealthDelay = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"), Category = "!UI|Enemy|Health")
	float AnimatedHealthDecreaseStep = 0.025f;

private:
	void InitializeFromOwner();
	void QueueInitializeRetry();
	void StopInitializeRetry();
	void BindAttributeDelegates();
	void UnbindAttributeDelegates();
	void StartDecreaseHealthAnimation();
	void ClearAnimationTimers();
	void HideAnimatedProgressBar();
	UProgressBar* GetProgressBar() const;
	UProgressBar* GetAnimatedProgressBar() const;
	UProgressBar* FindProgressBarByName(FName WidgetName) const;
	float GetAttributeValue(const FGameplayAttribute& Attribute, bool* bOutSuccessfullyFoundAttribute = nullptr) const;
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	float GetHealthPercent(float CurrentValue, float MaxValue) const;

	void OnHealthChanged(const FOnAttributeChangeData& ChangeData);
	void OnMaxHealthChanged(const FOnAttributeChangeData& ChangeData);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FTimerHandle InitializeTimerHandle;
	FTimerHandle AnimateHealthDelayTimer;
	FTimerHandle DecreaseHealthTimer;
	int32 InitializeRetryCount = 0;
	float AnimatedHealthPercent = 0.0f;
	float AnimatedHealthTargetPercent = 0.0f;

	static constexpr int32 MaxInitializeRetryCount = 20;
};
