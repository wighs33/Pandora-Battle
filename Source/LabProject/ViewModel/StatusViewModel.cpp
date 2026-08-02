#include "StatusViewModel.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Character/CharacterBase.h"
#include "GameFramework/Controller.h"
#include "Mode/PdPlayerState.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/LevelingComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusViewModel)

DEFINE_LOG_CATEGORY(StatusViewModelLog);

const FName UStatusViewModel::ViewModelName = TEXT("StatusViewModel");

namespace StatusViewModel
{
	float GetAttributeValue(UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute)
	{
		bool bFound = false;
		const float Value = ASC ? ASC->GetGameplayAttributeValue(Attribute, bFound) : 0.f;

		return Value;
	}

	float GetRequiredExperienceForNextLevel(UAbilitySystemComponent* ASC)
	{
		const APdPlayerState* PlayerState = ASC ? Cast<APdPlayerState>(ASC->GetOwner()) : nullptr;
		const ULevelingComponent* LevelingComponent = PlayerState ? PlayerState->GetLevelingComponent() : nullptr;
		return LevelingComponent ? LevelingComponent->GetRequiredExperienceForNextLevel() : 0.f;
	}

	float RoundResourceValue(float Value)
	{
		return FMath::RoundToFloat(Value);
	}

	float RoundPercentValue(float Value)
	{
		return FMath::RoundToFloat(Value);
	}

	ACharacterBase* ResolveCharacter(UAbilitySystemComponent* ASC)
	{
		if (!ASC)
		{
			return nullptr;
		}

		if (ACharacterBase* AvatarCharacter = Cast<ACharacterBase>(ASC->GetAvatarActor()))
		{
			return AvatarCharacter;
		}

		if (ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(ASC->GetOwner()))
		{
			return OwnerCharacter;
		}

		const APdPlayerState* PlayerState = Cast<APdPlayerState>(ASC->GetOwner());
		const AController* Controller = PlayerState ? Cast<AController>(PlayerState->GetOwner()) : nullptr;
		return Controller ? Cast<ACharacterBase>(Controller->GetPawn()) : nullptr;
	}

	UEquipmentComponent* ResolveEquipmentComponent(UAbilitySystemComponent* ASC)
	{
		const ACharacterBase* Character = ResolveCharacter(ASC);
		return Character ? Character->GetEquipmentComponent() : nullptr;
	}

	float CalculateFinalStrengthDamage(UAbilitySystemComponent* ASC, float Strength)
	{
		const ACharacterBase* Character = ResolveCharacter(ASC);
		UCombatComponent* CombatComponent = Character ? Character->GetCombatComponent() : nullptr;
		return CombatComponent ? CombatComponent->GetStrengthAdjustedWeaponDamageMagnitude(Strength) : 0.f;
	}

	float CalculateFinalCriticalDamage(float FinalStrengthDamage, float Critical)
	{
		const float CriticalDamageMultiplier = 2.f + FMath::Max(Critical, 0.f) * 0.01f;
		return FMath::Max(FinalStrengthDamage, 0.f) * CriticalDamageMultiplier;
	}

	float CalculateFinalArmor(float Armor, float FinalStrengthDamage)
	{
		return FMath::Max(FinalStrengthDamage, 0.f) * FMath::Clamp(Armor, 0.f, 100.f) * 0.01f;
	}

	float CalculateFinalRecovery(float Recovery, float MaxHealth)
	{
		return FMath::Max(MaxHealth, 0.f) * FMath::Max(Recovery, 0.f) * 0.01f;
	}
}

UStatusViewModel::UStatusViewModel()
{
}

void UStatusViewModel::InitializeViewModel(UObject* SourceObject)
{
	// =================================================================================================================

	UAbilitySystemComponent* InASC = Cast<UAbilitySystemComponent>(SourceObject);
	if (!InASC)
	{

		return;
	}


	// =================================================================================================================

	if (ASC.Get() == InASC && IsViewModelInitialized())
	{

		RefreshEquipmentComponentBinding();
		UpdateAllData();
		return;
	}

	// =================================================================================================================

	if (IsViewModelInitialized())
	{
		UninitializeViewModel();
	}

	ASC = InASC;
	RefreshEquipmentComponentBinding();

	// =================================================================================================================

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetLevelAttribute()).AddUObject(this, &ThisClass::OnLevelingChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetExperienceAttribute()).AddUObject(this, &ThisClass::OnLevelingChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxExperienceAttribute()).AddUObject(this, &ThisClass::OnLevelingChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStrengthAttribute()).AddUObject(this, &ThisClass::OnOffenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetIntelligenceAttribute()).AddUObject(this, &ThisClass::OnOffenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetCriticalAttribute()).AddUObject(this, &ThisClass::OnOffenseChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArmorAttribute()).AddUObject(this, &ThisClass::OnDefenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetRecoveryAttribute()).AddUObject(this, &ThisClass::OnDefenseChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldAttribute()).AddUObject(this, &ThisClass::OnDefenseChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFrostbiteAttribute()).AddUObject(this, &ThisClass::OnResistanceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetBurnAttribute()).AddUObject(this, &ThisClass::OnResistanceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetElectricShockAttribute()).AddUObject(this, &ThisClass::OnResistanceChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFirstPandoraAttribute()).AddUObject(this, &ThisClass::OnPandoraForceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetSecondPandoraAttribute()).AddUObject(this, &ThisClass::OnPandoraForceChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetThirdPandoraAttribute()).AddUObject(this, &ThisClass::OnPandoraForceChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAttackSpeedAttribute()).AddUObject(this, &ThisClass::OnAgilityChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMovementSpeedAttribute()).AddUObject(this, &ThisClass::OnAgilityChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArcaneAttribute()).AddUObject(this, &ThisClass::OnAgilityChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetOffensePointAttribute()).AddUObject(this, &ThisClass::OnInvestmentPointChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetDefensePointAttribute()).AddUObject(this, &ThisClass::OnInvestmentPointChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetResistancePointAttribute()).AddUObject(this, &ThisClass::OnInvestmentPointChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetPandoraForcePointAttribute()).AddUObject(this, &ThisClass::OnInvestmentPointChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetResourcePointAttribute()).AddUObject(this, &ThisClass::OnInvestmentPointChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAgilityPointAttribute()).AddUObject(this, &ThisClass::OnInvestmentPointChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStrengthLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetIntelligenceLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArcaneLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArmorLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetRecoveryLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFrostbiteLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetBurnLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetElectricShockLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFirstPandoraLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetSecondPandoraLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetThirdPandoraLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAttackSpeedLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMovementSpeedLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetCriticalLevelAttribute()).AddUObject(this, &ThisClass::OnStatLevelChanged);

	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute()).AddUObject(this, &ThisClass::OnHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &ThisClass::OnMaxHealthChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetShieldAttribute()).AddUObject(this, &ThisClass::OnShieldChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldAttribute()).AddUObject(this, &ThisClass::OnMaxShieldChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetManaAttribute()).AddUObject(this, &ThisClass::OnManaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaAttribute()).AddUObject(this, &ThisClass::OnMaxManaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute()).AddUObject(this, &ThisClass::OnStaminaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaAttribute()).AddUObject(this, &ThisClass::OnMaxStaminaChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthIncreasePercentAttribute()).AddUObject(this, &ThisClass::OnResourceIncreasePercentChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldIncreasePercentAttribute()).AddUObject(this, &ThisClass::OnResourceIncreasePercentChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaIncreasePercentAttribute()).AddUObject(this, &ThisClass::OnResourceIncreasePercentChanged);
	InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaIncreasePercentAttribute()).AddUObject(this, &ThisClass::OnResourceIncreasePercentChanged);

	UpdateAllData();


	Super::InitializeViewModel(SourceObject);
}

void UStatusViewModel::UninitializeViewModel()
{
	// =================================================================================================================

	if (UAbilitySystemComponent* ASCPtr = ASC.Get())
	{
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetExperienceAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxExperienceAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStrengthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetIntelligenceAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetCriticalAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArmorAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetRecoveryAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFrostbiteAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetBurnAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetElectricShockAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFirstPandoraAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetSecondPandoraAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetThirdPandoraAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAttackSpeedAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMovementSpeedAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArcaneAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetOffensePointAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetDefensePointAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetResistancePointAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetPandoraForcePointAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetResourcePointAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAgilityPointAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStrengthLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetIntelligenceLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArcaneLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetArmorLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetRecoveryLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFrostbiteLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetBurnLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetElectricShockLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetFirstPandoraLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetSecondPandoraLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetThirdPandoraLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAttackSpeedLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMovementSpeedLevelAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetCriticalLevelAttribute()).RemoveAll(this);

		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetShieldAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetManaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthIncreasePercentAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldIncreasePercentAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaIncreasePercentAttribute()).RemoveAll(this);
		ASCPtr->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaIncreasePercentAttribute()).RemoveAll(this);
	}

	ClearEquipmentComponentBinding();
	ASC.Reset();

	Super::UninitializeViewModel();
}

void UStatusViewModel::UpdateLevelingData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	const float NewLevel = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetLevelAttribute());
	const float NewCurrentExperience = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetExperienceAttribute());
	const float NewMaxExperience = StatusViewModel::GetRequiredExperienceForNextLevel(ASCPtr);

	UE_MVVM_SET_PROPERTY_VALUE(Level, NewLevel);
	UE_MVVM_SET_PROPERTY_VALUE(CurrentExperience, NewCurrentExperience);
	UE_MVVM_SET_PROPERTY_VALUE(MaxExperience, NewMaxExperience);
	UE_MVVM_SET_PROPERTY_VALUE(ExperiencePercent, NewMaxExperience > 0.f ? FMath::Clamp(NewCurrentExperience / NewMaxExperience, 0.f, 1.f) : 0.f);

}

void UStatusViewModel::UpdateOffenseData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	const float NewStrength = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetStrengthAttribute());
	const float NewCritical = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetCriticalAttribute());
	RefreshEquipmentComponentBinding();
	const float NewFinalStrength = StatusViewModel::CalculateFinalStrengthDamage(ASCPtr, NewStrength);

	UE_MVVM_SET_PROPERTY_VALUE(Strength, NewStrength);
	UE_MVVM_SET_PROPERTY_VALUE(FinalStrength, NewFinalStrength);
	UE_MVVM_SET_PROPERTY_VALUE(Intelligence, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetIntelligenceAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Critical, NewCritical);
	UE_MVVM_SET_PROPERTY_VALUE(FinalCriticalDamage, StatusViewModel::CalculateFinalCriticalDamage(NewFinalStrength, NewCritical));

}

void UStatusViewModel::UpdateDefenseData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	const float NewRecovery = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetRecoveryAttribute());
	const float CurrentMaxHealth = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxHealthAttribute());
	const float CurrentMaxShield = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxShieldAttribute());
	const float CurrentStrength = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetStrengthAttribute());
	RefreshEquipmentComponentBinding();
	const float CurrentFinalStrength = StatusViewModel::CalculateFinalStrengthDamage(ASCPtr, CurrentStrength);
	const float NewArmor = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetArmorAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Armor, NewArmor);
	UE_MVVM_SET_PROPERTY_VALUE(FinalArmor, StatusViewModel::CalculateFinalArmor(NewArmor, CurrentFinalStrength));
	UE_MVVM_SET_PROPERTY_VALUE(Recovery, NewRecovery);
	UE_MVVM_SET_PROPERTY_VALUE(FinalRecovery, StatusViewModel::RoundResourceValue(StatusViewModel::CalculateFinalRecovery(NewRecovery, CurrentMaxHealth)));
	UE_MVVM_SET_PROPERTY_VALUE(MaxShield, StatusViewModel::RoundResourceValue(CurrentMaxShield));

}

void UStatusViewModel::UpdateResistanceData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(Frostbite, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetFrostbiteAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Burn, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetBurnAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ElectricShock, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetElectricShockAttribute()));

}

void UStatusViewModel::UpdatePandoraForceData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(FirstPandora, StatusViewModel::RoundPercentValue(
		StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetFirstPandoraAttribute())));
	UE_MVVM_SET_PROPERTY_VALUE(SecondPandora, StatusViewModel::RoundPercentValue(
		StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetSecondPandoraAttribute())));
	UE_MVVM_SET_PROPERTY_VALUE(ThirdPandora, StatusViewModel::RoundPercentValue(
		StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetThirdPandoraAttribute())));

}

void UStatusViewModel::UpdateAgilityData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(AttackSpeed, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetAttackSpeedAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MovementSpeed, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMovementSpeedAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(Arcane, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetArcaneAttribute()));

}

void UStatusViewModel::UpdateInvestmentPointData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(OffensePoint, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetOffensePointAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(DefensePoint, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetDefensePointAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ResistancePoint, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetResistancePointAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(PandoraForcePoint, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetPandoraForcePointAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ResourcePoint, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetResourcePointAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(AgilityPoint, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetAgilityPointAttribute()));

}

void UStatusViewModel::UpdateStatLevelData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(StrengthLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetStrengthLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(IntelligenceLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetIntelligenceLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(CriticalLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetCriticalLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ArmorLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetArmorLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(RecoveryLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetRecoveryLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MaxShieldLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxShieldLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(FrostbiteLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetFrostbiteLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(BurnLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetBurnLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ElectricShockLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetElectricShockLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(FirstPandoraLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetFirstPandoraLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(SecondPandoraLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetSecondPandoraLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ThirdPandoraLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetThirdPandoraLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealthLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxHealthLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MaxManaLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxManaLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MaxStaminaLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxStaminaLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(AttackSpeedLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetAttackSpeedLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(MovementSpeedLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMovementSpeedLevelAttribute()));
	UE_MVVM_SET_PROPERTY_VALUE(ArcaneLevel, StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetArcaneLevelAttribute()));

}

void UStatusViewModel::UpdateResourceIncreasePercentData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{
		UE_MVVM_SET_PROPERTY_VALUE(MaxHealthIncreasePercent, 0.f);
		UE_MVVM_SET_PROPERTY_VALUE(MaxShieldIncreasePercent, 0.f);
		UE_MVVM_SET_PROPERTY_VALUE(MaxManaIncreasePercent, 0.f);
		UE_MVVM_SET_PROPERTY_VALUE(MaxStaminaIncreasePercent, 0.f);

		return;
	}

	UE_MVVM_SET_PROPERTY_VALUE(MaxHealthIncreasePercent,
		StatusViewModel::RoundPercentValue(StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxHealthIncreasePercentAttribute())));
	UE_MVVM_SET_PROPERTY_VALUE(MaxShieldIncreasePercent,
		StatusViewModel::RoundPercentValue(StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxShieldIncreasePercentAttribute())));
	UE_MVVM_SET_PROPERTY_VALUE(MaxManaIncreasePercent,
		StatusViewModel::RoundPercentValue(StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxManaIncreasePercentAttribute())));
	UE_MVVM_SET_PROPERTY_VALUE(MaxStaminaIncreasePercent,
		StatusViewModel::RoundPercentValue(StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxStaminaIncreasePercentAttribute())));


}

void UStatusViewModel::UpdateHealthData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	const float CurrentHealth = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetHealthAttribute());
	const float CurrentMaxHealth = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxHealthAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Health, StatusViewModel::RoundResourceValue(CurrentHealth));
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, StatusViewModel::RoundResourceValue(CurrentMaxHealth));
	UE_MVVM_SET_PROPERTY_VALUE(HealthPercent, CurrentMaxHealth > 0.f ? CurrentHealth / CurrentMaxHealth : 0.f);
	UE_MVVM_SET_PROPERTY_VALUE(FinalRecovery, StatusViewModel::RoundResourceValue(StatusViewModel::CalculateFinalRecovery(Recovery, CurrentMaxHealth)));

}

/** Shield related values. */
void UStatusViewModel::UpdateShieldData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	const float CurrentShield = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetShieldAttribute());
	const float CurrentMaxShield = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxShieldAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Shield, StatusViewModel::RoundResourceValue(CurrentShield));
	UE_MVVM_SET_PROPERTY_VALUE(MaxShield, StatusViewModel::RoundResourceValue(CurrentMaxShield));
	UE_MVVM_SET_PROPERTY_VALUE(ShieldPercent, CurrentMaxShield > 0.f ? CurrentShield / CurrentMaxShield : 0.f);

}

void UStatusViewModel::UpdateManaData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	const float CurrentMana = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetManaAttribute());
	const float CurrentMaxMana = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxManaAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Mana, StatusViewModel::RoundResourceValue(CurrentMana));
	UE_MVVM_SET_PROPERTY_VALUE(MaxMana, StatusViewModel::RoundResourceValue(CurrentMaxMana));
	UE_MVVM_SET_PROPERTY_VALUE(ManaPercent, CurrentMaxMana > 0.f ? CurrentMana / CurrentMaxMana : 0.f);

}

void UStatusViewModel::UpdateStaminaData()
{
	UAbilitySystemComponent* ASCPtr = ASC.Get();
	if (!ASCPtr)
	{

		return;
	}

	const float CurrentStamina = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetStaminaAttribute());
	const float CurrentMaxStamina = StatusViewModel::GetAttributeValue(ASCPtr, UBasicAttributeSet::GetMaxStaminaAttribute());

	UE_MVVM_SET_PROPERTY_VALUE(Stamina, StatusViewModel::RoundResourceValue(CurrentStamina));
	UE_MVVM_SET_PROPERTY_VALUE(MaxStamina, StatusViewModel::RoundResourceValue(CurrentMaxStamina));
	UE_MVVM_SET_PROPERTY_VALUE(StaminaPercent, CurrentMaxStamina > 0.f ? CurrentStamina / CurrentMaxStamina : 0.f);

}

void UStatusViewModel::UpdateAllData()
{

	UpdateLevelingData();
	UpdateOffenseData();
	UpdateDefenseData();
	UpdateResistanceData();
	UpdatePandoraForceData();
	UpdateAgilityData();
	UpdateInvestmentPointData();
	UpdateStatLevelData();
	UpdateHealthData();
	UpdateShieldData();
	UpdateManaData();
	UpdateStaminaData();
	UpdateResourceIncreasePercentData();
}

void UStatusViewModel::OnLevelingChanged(const FOnAttributeChangeData& Data)
{

	UpdateLevelingData();
}

void UStatusViewModel::OnOffenseChanged(const FOnAttributeChangeData& Data)
{

	UpdateOffenseData();
	UpdateDefenseData();
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

void UStatusViewModel::OnInvestmentPointChanged(const FOnAttributeChangeData& Data)
{

	UpdateInvestmentPointData();
}

void UStatusViewModel::OnStatLevelChanged(const FOnAttributeChangeData& Data)
{

	UpdateStatLevelData();
}

void UStatusViewModel::OnHealthChanged(const FOnAttributeChangeData& Data)
{

	UpdateHealthData();
}

void UStatusViewModel::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{

	UpdateHealthData();
	UpdateResourceIncreasePercentData();
}

/** Shield changed. */
void UStatusViewModel::OnShieldChanged(const FOnAttributeChangeData& Data)
{

	UpdateShieldData();
}

void UStatusViewModel::OnMaxShieldChanged(const FOnAttributeChangeData& Data)
{

	UpdateShieldData();
	UpdateResourceIncreasePercentData();
}

void UStatusViewModel::OnManaChanged(const FOnAttributeChangeData& Data)
{

	UpdateManaData();
}

void UStatusViewModel::OnMaxManaChanged(const FOnAttributeChangeData& Data)
{

	UpdateManaData();
	UpdateResourceIncreasePercentData();
}

void UStatusViewModel::OnStaminaChanged(const FOnAttributeChangeData& Data)
{

	UpdateStaminaData();
}

void UStatusViewModel::OnMaxStaminaChanged(const FOnAttributeChangeData& Data)
{

	UpdateStaminaData();
	UpdateResourceIncreasePercentData();
}

void UStatusViewModel::OnResourceIncreasePercentChanged(const FOnAttributeChangeData& Data)
{

	UpdateResourceIncreasePercentData();
	UpdateHealthData();
	UpdateShieldData();
	UpdateManaData();
	UpdateStaminaData();
}

void UStatusViewModel::OnCurrentWeaponDefinitionChanged()
{

	UpdateOffenseData();
	UpdateDefenseData();
}

void UStatusViewModel::RefreshEquipmentComponentBinding()
{
	UEquipmentComponent* ResolvedEquipmentComponent = StatusViewModel::ResolveEquipmentComponent(ASC.Get());
	if (EquipmentComponent.Get() == ResolvedEquipmentComponent)
	{
		return;
	}

	ClearEquipmentComponentBinding();

	if (!ResolvedEquipmentComponent)
	{
		return;
	}

	EquipmentComponent = ResolvedEquipmentComponent;
	ResolvedEquipmentComponent->OnCurrentWeaponDefinitionChanged.AddUObject(this, &ThisClass::OnCurrentWeaponDefinitionChanged);

}

void UStatusViewModel::ClearEquipmentComponentBinding()
{
	if (UEquipmentComponent* BoundEquipmentComponent = EquipmentComponent.Get())
	{
		BoundEquipmentComponent->OnCurrentWeaponDefinitionChanged.RemoveAll(this);
	}

	EquipmentComponent.Reset();
}
