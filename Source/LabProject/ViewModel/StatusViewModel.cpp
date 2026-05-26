#include "StatusViewModel.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusViewModel)

DEFINE_LOG_CATEGORY(StatusViewModelLog);

const FName UStatusViewModel::ViewModelName = TEXT("StatusViewModel");

namespace StatusViewModel
{
	/** ASC에서 Attribute 값을 읽습니다. */
	float GetAttributeValue(UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute)
	{
		bool bFound = false;
		const float Value = ASC ? ASC->GetGameplayAttributeValue(Attribute, bFound) : 0.f;
		UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] StatusViewModel read attribute: vmASC=%s owner=%s attribute=%s value=%.3f found=%s"),
			*GetNameSafe(ASC),
			*GetNameSafe(ASC ? ASC->GetOwner() : nullptr),
			*Attribute.GetName(),
			Value,
			bFound ? TEXT("true") : TEXT("false"));
		return Value;
	}

}

/** 상태창 ViewModel 기본 상태를 초기화합니다. */
UStatusViewModel::UStatusViewModel()
{
}

/** SourceObject로부터 ASC를 찾아 초기화합니다. */
void UStatusViewModel::InitializeViewModel(UObject* SourceObject)
{
	// =================================================================================================================
	// === ASC 확인

	UAbilitySystemComponent* InASC = Cast<UAbilitySystemComponent>(SourceObject);
	if (!InASC)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] InitializeViewModel failed: SourceObject is not an ASC. source=%s"),
			*GetNameSafe(SourceObject));
		return;
	}
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] InitializeViewModel: viewModel=%s asc=%s owner=%s initialized=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InASC),
		*GetNameSafe(InASC->GetOwner()),
		IsViewModelInitialized() ? TEXT("true") : TEXT("false"));

	// =================================================================================================================
	// === 같은 ASC 재사용

	if (ASC.Get() == InASC && IsViewModelInitialized())
	{
		UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] InitializeViewModel reused same ASC. Updating all data."));
		UpdateAllData();
		return;
	}

	// =================================================================================================================
	// === 기존 바인딩 정리

	if (IsViewModelInitialized())
	{
		UninitializeViewModel();
	}

	ASC = InASC;

	// =================================================================================================================
	// === 델리게이트 바인딩

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStrengthAttribute()).AddUObject(this, &ThisClass::OnOffenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetIntelligenceAttribute()).AddUObject(this, &ThisClass::OnOffenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArcaneAttribute()).AddUObject(this, &ThisClass::OnOffenseChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArmorAttribute()).AddUObject(this, &ThisClass::OnDefenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetRecoveryAttribute()).AddUObject(this, &ThisClass::OnDefenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMagicResistanceAttribute()).AddUObject(this, &ThisClass::OnDefenseChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetImmunityAttribute()).AddUObject(this, &ThisClass::OnResistanceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFortitudeAttribute()).AddUObject(this, &ThisClass::OnResistanceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetSanityAttribute()).AddUObject(this, &ThisClass::OnResistanceChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFirstPandoraAttribute()).AddUObject(this, &ThisClass::OnPandoraForceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetSecondPandoraAttribute()).AddUObject(this, &ThisClass::OnPandoraForceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetThirdPandoraAttribute()).AddUObject(this, &ThisClass::OnPandoraForceChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAttackSpeedAttribute()).AddUObject(this, &ThisClass::OnAgilityChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMovementSpeedAttribute()).AddUObject(this, &ThisClass::OnAgilityChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetCriticalChanceAttribute()).AddUObject(this, &ThisClass::OnAgilityChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::OnMaxHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetShieldAttribute()).AddUObject(this, &ThisClass::OnShieldChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldAttribute()).AddUObject(this, &ThisClass::OnMaxShieldChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetManaAttribute()).AddUObject(this, &ThisClass::OnManaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaAttribute()).AddUObject(this, &ThisClass::OnMaxManaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute()).AddUObject(this, &ThisClass::OnStaminaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaAttribute()).AddUObject(this, &ThisClass::OnMaxStaminaChanged);

	UpdateAllData();
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] InitializeViewModel completed: viewModel=%s asc=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InASC));

	Super::InitializeViewModel(SourceObject);
}

/** ASC 바인딩을 해제합니다. */
void UStatusViewModel::UninitializeViewModel()
{
	// =================================================================================================================
	// === 델리게이트 해제

	if (UAbilitySystemComponent* ASCPtr = ASC.Get())
	{
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStrengthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetIntelligenceAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArcaneAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArmorAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetRecoveryAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMagicResistanceAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetImmunityAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFortitudeAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetSanityAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFirstPandoraAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetSecondPandoraAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetThirdPandoraAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAttackSpeedAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMovementSpeedAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetCriticalChanceAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetShieldAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetManaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaAttribute()).RemoveAll(this);
	}

	ASC.Reset();

	Super::UninitializeViewModel();
}

/** 공격 관련 값을 갱신합니다. */
void UStatusViewModel::UpdateOffenseData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] UpdateOffenseData skipped: ASC is null."));
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Strength, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetStrengthAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Intelligence, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetIntelligenceAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Arcane, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetArcaneAttribute()));
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdateOffenseData applied: strength=%.3f intelligence=%.3f arcane=%.3f"),
		Strength,
		Intelligence,
		Arcane);
}

/** 방어 관련 값을 갱신합니다. */
void UStatusViewModel::UpdateDefenseData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] UpdateDefenseData skipped: ASC is null."));
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Armor, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetArmorAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Recovery, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetRecoveryAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MagicResistance, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMagicResistanceAttribute()));
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdateDefenseData applied: Armor=%.3f recovery=%.3f magicResistance=%.3f"),
		Armor,
		Recovery,
		MagicResistance);
}

/** 저항 관련 값을 갱신합니다. */
void UStatusViewModel::UpdateResistanceData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] UpdateResistanceData skipped: ASC is null."));
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Immunity, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetImmunityAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Fortitude, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetFortitudeAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Sanity, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetSanityAttribute()));
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdateResistanceData applied: immunity=%.3f fortitude=%.3f sanity=%.3f"),
		Immunity,
		Fortitude,
		Sanity);
}

/** 판도라 관련 값을 갱신합니다. */
void UStatusViewModel::UpdatePandoraForceData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] UpdatePandoraForceData skipped: ASC is null."));
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(FirstPandora, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetFirstPandoraAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(SecondPandora, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetSecondPandoraAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ThirdPandora, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetThirdPandoraAttribute()));
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdatePandoraForceData applied: first=%.3f second=%.3f third=%.3f"),
		FirstPandora,
		SecondPandora,
		ThirdPandora);
}

/** 민첩 관련 값을 갱신합니다. */
void UStatusViewModel::UpdateAgilityData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] UpdateAgilityData skipped: ASC is null."));
		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(AttackSpeed, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetAttackSpeedAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MovementSpeed, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMovementSpeedAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(CriticalChance, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetCriticalChanceAttribute()));
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdateAgilityData applied: attackSpeed=%.3f movementSpeed=%.3f criticalChance=%.3f"),
		AttackSpeed,
		MovementSpeed,
		CriticalChance);
}

/** 체력 관련 값을 갱신합니다. */
void UStatusViewModel::UpdateHealthData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] UpdateHealthData skipped: ASC is null."));
		return;
	}

	const float CurrentHealth = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetHealthAttribute());
	const float CurrentMaxHealth = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxHealthAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Health, CurrentHealth);
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, CurrentMaxHealth);
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, CurrentMaxHealth > 0.f ? CurrentHealth / CurrentMaxHealth : 0.f);
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdateHealthData applied: health=%.3f maxHealth=%.3f percent=%.3f"),
		Health,
		MaxHealth,
		HealthPercent);
}

/** Shield related values. */
void UStatusViewModel::UpdateShieldData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] UpdateShieldData skipped: ASC is null."));
		return;
	}

	const float CurrentShield = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetShieldAttribute());
	const float CurrentMaxShield = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxShieldAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Shield, CurrentShield);
	UE_MVVM_SET_PROPERTY_VALUE(MaxShield, CurrentMaxShield);
	UE_MVVM_SET_PROPERTY_VALUE(ShieldPercent, CurrentMaxShield > 0.f ? CurrentShield / CurrentMaxShield : 0.f);
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdateShieldData applied: shield=%.3f maxShield=%.3f percent=%.3f"),
		Shield,
		MaxShield,
		ShieldPercent);
}

void UStatusViewModel::UpdateManaData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] UpdateManaData skipped: ASC is null."));
		return;
	}

	const float CurrentMana = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetManaAttribute());
	const float CurrentMaxMana = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxManaAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Mana, CurrentMana);
	UE_MVVM_SET_PROPERTY_VALUE(MaxMana, CurrentMaxMana);
	UE_MVVM_SET_PROPERTY_VALUE(ManaPercent, CurrentMaxMana > 0.f ? CurrentMana / CurrentMaxMana : 0.f);
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdateManaData applied: mana=%.3f maxMana=%.3f percent=%.3f"),
		Mana,
		MaxMana,
		ManaPercent);
}

/** 스태미나 관련 값을 갱신합니다. */
void UStatusViewModel::UpdateStaminaData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_LOG(StatusViewModelLog, Warning, TEXT("[StatUpgrade] UpdateStaminaData skipped: ASC is null."));
		return;
	}

	const float CurrentStamina = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetStaminaAttribute());
	const float CurrentMaxStamina = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxStaminaAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Stamina, CurrentStamina);
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, CurrentMaxStamina);
	UE_MVVM_SET_PROPERTY_VALUE(StaminaPercent, CurrentMaxStamina > 0.f ? CurrentStamina / CurrentMaxStamina : 0.f);
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdateStaminaData applied: stamina=%.3f maxStamina=%.3f percent=%.3f"),
		Stamina,
		MaxStamina,
		StaminaPercent);
}

/** 모든 값을 갱신합니다. */
void UStatusViewModel::UpdateAllData()
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] UpdateAllData started: viewModel=%s asc=%s"),
		*GetNameSafe(this),
		*GetNameSafe(ASC.Get()));
	UpdateOffenseData();
	UpdateDefenseData();
	UpdateResistanceData();
	UpdatePandoraForceData();
	UpdateAgilityData();
	UpdateHealthData();
	UpdateShieldData();
	UpdateManaData();
	UpdateStaminaData();
}

/** 공격 수치 변경을 처리합니다. */
void UStatusViewModel::OnOffenseChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnOffenseChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateOffenseData();
}

/** 방어 수치 변경을 처리합니다. */
void UStatusViewModel::OnDefenseChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnDefenseChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateDefenseData();
}

/** 저항 수치 변경을 처리합니다. */
void UStatusViewModel::OnResistanceChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnResistanceChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateResistanceData();
}

/** 판도라 수치 변경을 처리합니다. */
void UStatusViewModel::OnPandoraForceChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnPandoraForceChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdatePandoraForceData();
}

/** 민첩 수치 변경을 처리합니다. */
void UStatusViewModel::OnAgilityChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnAgilityChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateAgilityData();
}

/** 체력 변경을 처리합니다. */
void UStatusViewModel::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnHealthChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateHealthData();
}

/** 최대 체력 변경을 처리합니다. */
void UStatusViewModel::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnMaxHealthChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateHealthData();
}

/** Shield changed. */
void UStatusViewModel::OnShieldChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnShieldChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateShieldData();
}

void UStatusViewModel::OnMaxShieldChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnMaxShieldChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateShieldData();
}

void UStatusViewModel::OnManaChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnManaChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateManaData();
}

/** 최대 마나 변경을 처리합니다. */
void UStatusViewModel::OnMaxManaChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnMaxManaChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateManaData();
}

/** 스태미나 변경을 처리합니다. */
void UStatusViewModel::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnStaminaChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateStaminaData();
}

/** 최대 스태미나 변경을 처리합니다. */
void UStatusViewModel::OnMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
	UE_LOG(StatusViewModelLog, Log, TEXT("[StatUpgrade] OnMaxStaminaChanged: attribute=%s old=%.3f new=%.3f"),
		*Data.Attribute.GetName(),
		Data.OldValue,
		Data.NewValue);
	UpdateStaminaData();
}
