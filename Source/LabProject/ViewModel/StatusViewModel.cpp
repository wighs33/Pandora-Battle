// Copyright Epic Games, Inc. All Rights Reserved.

#include "StatusViewModel.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/PdAttributeSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusViewModel)

DEFINE_LOG_CATEGORY(StatusViewModelLog);

const FName UStatusViewModel::ViewModelName = TEXT("StatusViewModel");

namespace StatusViewModel
{
	float GetAttributeValue(UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute)
	{
		bool bFound = false;
		return ASC ? ASC->GetGameplayAttributeValue(Attribute, bFound) : 0.f;
	}
}

UStatusViewModel::UStatusViewModel()
{
}

void UStatusViewModel::InitializeViewModel(UObject* SourceObject)
{
	UAbilitySystemComponent* InASC = Cast<UAbilitySystemComponent>(SourceObject);
	if (!InASC)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("InitializeViewModel failed: SourceObject is not an ASC."));
		return;
	}

	if (ASC.Get() == InASC && IsViewModelInitialized())
	{
		UpdateAllData();
		return;
	}

	if (IsViewModelInitialized())
	{
		UninitializeViewModel();
	}

	ASC = InASC;

	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetStrengthAttribute()).AddUObject(this, &ThisClass::OnOffenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetIntelligenceAttribute()).AddUObject(this, &ThisClass::OnOffenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetArcaneAttribute()).AddUObject(this, &ThisClass::OnOffenseChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetToughnessAttribute()).AddUObject(this, &ThisClass::OnDefenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetRecoveryAttribute()).AddUObject(this, &ThisClass::OnDefenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMagicResistanceAttribute()).AddUObject(this, &ThisClass::OnDefenseChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetImmunityAttribute()).AddUObject(this, &ThisClass::OnResistanceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetFortitudeAttribute()).AddUObject(this, &ThisClass::OnResistanceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetSanityAttribute()).AddUObject(this, &ThisClass::OnResistanceChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetFirstPandoraAttribute()).AddUObject(this, &ThisClass::OnPandoraForceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetSecondPandoraAttribute()).AddUObject(this, &ThisClass::OnPandoraForceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetThirdPandoraAttribute()).AddUObject(this, &ThisClass::OnPandoraForceChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetAttackSpeedAttribute()).AddUObject(this, &ThisClass::OnAgilityChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMovementSpeedAttribute()).AddUObject(this, &ThisClass::OnAgilityChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetCriticalChanceAttribute()).AddUObject(this, &ThisClass::OnAgilityChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::OnMaxHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetManaAttribute()).AddUObject(this, &ThisClass::OnManaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMaxManaAttribute()).AddUObject(this, &ThisClass::OnMaxManaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetStaminaAttribute()).AddUObject(this, &ThisClass::OnStaminaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMaxStaminaAttribute()).AddUObject(this, &ThisClass::OnMaxStaminaChanged);

	UpdateAllData();

	Super::InitializeViewModel(SourceObject);
}

void UStatusViewModel::UninitializeViewModel()
{
	if (UAbilitySystemComponent* ASCPtr = ASC.Get())
	{
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetStrengthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetIntelligenceAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetArcaneAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetToughnessAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetRecoveryAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMagicResistanceAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetImmunityAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetFortitudeAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetSanityAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetFirstPandoraAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetSecondPandoraAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetThirdPandoraAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetAttackSpeedAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMovementSpeedAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetCriticalChanceAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetHealthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetManaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMaxManaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetStaminaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UPdAttributeSet::GetMaxStaminaAttribute()).RemoveAll(this);
	}

	ASC.Reset();

	Super::UninitializeViewModel();
}

void UStatusViewModel::UpdateOffenseData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Strength, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetStrengthAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Intelligence, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetIntelligenceAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Arcane, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetArcaneAttribute()));
}

void UStatusViewModel::UpdateDefenseData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Toughness, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetToughnessAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Recovery, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetRecoveryAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MagicResistance, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetMagicResistanceAttribute()));
}

void UStatusViewModel::UpdateResistanceData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Immunity, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetImmunityAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Fortitude, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetFortitudeAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Sanity, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetSanityAttribute()));
}

void UStatusViewModel::UpdatePandoraForceData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(FirstPandora, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetFirstPandoraAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(SecondPandora, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetSecondPandoraAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ThirdPandora, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetThirdPandoraAttribute()));
}

void UStatusViewModel::UpdateAgilityData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(AttackSpeed, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetAttackSpeedAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MovementSpeed, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetMovementSpeedAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(CriticalChance, StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetCriticalChanceAttribute()));
}

void UStatusViewModel::UpdateHealthData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		return;
	}

	const float CurrentHealth = StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetHealthAttribute());
	const float CurrentMaxHealth = StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetMaxHealthAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Health, CurrentHealth);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, CurrentMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, CurrentMaxHealth > 0.f ? CurrentHealth / CurrentMaxHealth : 0.f);
}

void UStatusViewModel::UpdateManaData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		return;
	}

	const float CurrentMana = StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetManaAttribute());
	const float CurrentMaxMana = StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetMaxManaAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Mana, CurrentMana);
	UE_MVVM_SET_PROPERTY_VALUE(MaxMana, CurrentMaxMana);
	UE_MVVM_SET_PROPERTY_VALUE(ManaPercent, CurrentMaxMana > 0.f ? CurrentMana / CurrentMaxMana : 0.f);
}

void UStatusViewModel::UpdateStaminaData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		return;
	}

	const float CurrentStamina = StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetStaminaAttribute());
	const float CurrentMaxStamina = StatusViewModel::GetAttributeValue(ASCPtr, UPdAttributeSet::GetMaxStaminaAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Stamina, CurrentStamina);
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, CurrentMaxStamina);
	UE_MVVM_SET_PROPERTY_VALUE(StaminaPercent, CurrentMaxStamina > 0.f ? CurrentStamina / CurrentMaxStamina : 0.f);
}

void UStatusViewModel::UpdateAllData()
{
	UpdateOffenseData();
	UpdateDefenseData();
	UpdateResistanceData();
	UpdatePandoraForceData();
	UpdateAgilityData();
	UpdateHealthData();
	UpdateManaData();
	UpdateStaminaData();
}

void UStatusViewModel::OnOffenseChanged(const FOnAttributeChangeData& Data)
{
	UpdateOffenseData();
}

void UStatusViewModel::OnDefenseChanged(const FOnAttributeChangeData& Data)
{
	UpdateDefenseData();
}

void UStatusViewModel::OnResistanceChanged(const FOnAttributeChangeData& Data)
{
	UpdateResistanceData();
}

void UStatusViewModel::OnPandoraForceChanged(const FOnAttributeChangeData& Data)
{
	UpdatePandoraForceData();
}

void UStatusViewModel::OnAgilityChanged(const FOnAttributeChangeData& Data)
{
	UpdateAgilityData();
}

void UStatusViewModel::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	UpdateHealthData();
}

void UStatusViewModel::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	UpdateHealthData();
}

void UStatusViewModel::OnManaChanged(const FOnAttributeChangeData& Data)
{
	UpdateManaData();
}

void UStatusViewModel::OnMaxManaChanged(const FOnAttributeChangeData& Data)
{
	UpdateManaData();
}

void UStatusViewModel::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
	UpdateStaminaData();
}

void UStatusViewModel::OnMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
	UpdateStaminaData();
}
