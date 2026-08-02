#pragma once

#include "AttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "EnemyShieldBarWidget.generated.h"

class AActor;
class UAbilitySystemComponent;
class UProgressBar;
struct FOnAttributeChangeData;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UEnemyShieldBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Shield")
	void SetOwnerActor(AActor* InOwnerActor);

	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Shield")
	void UpdateShieldPercent();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Enemy|Shield")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Enemy|Shield")
	float CurrentShield = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Enemy|Shield")
	float MaxShield = 100.0f;

private:
	void InitializeFromOwner();
	void QueueInitializeRetry();
	void BindAttributeDelegates();
	void UnbindAttributeDelegates();
	UProgressBar* GetProgressBar() const;
	float GetAttributeValue(const FGameplayAttribute& Attribute, bool* bOutSuccessfullyFoundAttribute = nullptr) const;
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	float GetShieldPercent() const;

	void OnShieldChanged(const FOnAttributeChangeData& ChangeData);
	void OnMaxShieldChanged(const FOnAttributeChangeData& ChangeData);

	UPROPERTY(meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> ShieldProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle ShieldChangedHandle;
	FDelegateHandle MaxShieldChangedHandle;
	FTimerHandle InitializeTimerHandle;
	int32 InitializeRetryCount = 0;

	static constexpr int32 MaxInitializeRetryCount = 20;
};
