#pragma once

#include "AttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "Component/Character/AbilitySystemReadySubscription.h"

#include "EnemyShieldBarWidget.generated.h"

class AActor;
class ACharacterBase;
class UAbilitySystemComponent;
class UProgressBar;
struct FOnAttributeChangeData;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UEnemyShieldBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Shield")
	void SetOwnerActor(AActor* InOwnerActor);

	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Shield")
	void UpdateShieldPercent();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void InitializeFromOwner();
	void HandleOwnerAbilitySystemReady(ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent);

	void OnShieldChanged(const FOnAttributeChangeData& ChangeData);
	void OnMaxShieldChanged(const FOnAttributeChangeData& ChangeData);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ObserveOwnerAbilitySystem();
	void BindAttributeDelegates();
	void UnbindAttributeDelegates();
	UProgressBar* GetProgressBar() const;
	float GetAttributeValue(const FGameplayAttribute& Attribute, bool* bOutSuccessfullyFoundAttribute = nullptr) const;
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	float GetShieldPercent() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Enemy|Shield")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Enemy|Shield")
	float CurrentShield = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Enemy|Shield")
	float MaxShield = 100.0f;

private:
	UPROPERTY(meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> ShieldProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle ShieldChangedHandle;
	FDelegateHandle MaxShieldChangedHandle;
	FAbilitySystemReadySubscription OwnerReadySubscription;
};
