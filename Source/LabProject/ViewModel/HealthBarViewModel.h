#pragma once

#include "CoreMinimal.h"
#include "ViewModel/CommonViewModelBase.h"
#include "HealthBarViewModel.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_LOG_CATEGORY_EXTERN(HealthBarViewModelLog, Log, All);

UCLASS(BlueprintType)
class LABPROJECT_API UHealthBarViewModel : public UCommonViewModelBase
{
	GENERATED_BODY()

public:
	UHealthBarViewModel();

	// Timing hooks
	virtual void InitializeViewModel(UObject* SourceObject) override;

	virtual void UninitializeViewModel() override;

private:
	// Attribute delegate callbacks
	void OnLevelChanged(const FOnAttributeChangeData& Data);

	void OnExperienceChanged(const FOnAttributeChangeData& Data);

	void OnMaxExperienceChanged(const FOnAttributeChangeData& Data);

	void OnHealthChanged(const FOnAttributeChangeData& Data);

	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	float HealthPercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	FText HealthText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	bool bIsAlive = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel|Experience")
	float CurrentExperience = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel|Experience")
	float MaxExperience = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel|Experience")
	float ExperiencePercent = 0.f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel|Experience")
	FText ExperienceText;

	void UpdateHealthData();

	void UpdateExperienceData();

	void UpdateAllData();

	static const FName ViewModelName;

private:
	UAbilitySystemComponent* ResolveAbilitySystemComponent(UObject* SourceObject) const;

	void ResetViewData();

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
};
