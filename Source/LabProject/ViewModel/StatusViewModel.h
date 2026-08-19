#pragma once

#include "CoreMinimal.h"
#include "ViewModel/CommonViewModelBase.h"
#include "StatusViewModel.generated.h"

class UAbilitySystemComponent;
class UEquipmentComponent;
struct FOnAttributeChangeData;

DECLARE_LOG_CATEGORY_EXTERN(StatusViewModelLog, Log, All);

UCLASS(BlueprintType)
class LABPROJECT_API UStatusViewModel : public UCommonViewModelBase
{
	GENERATED_BODY()

public:
	UStatusViewModel();

	// Timing hooks
	virtual void InitializeViewModel(UObject* SourceObject) override;

	virtual void UninitializeViewModel() override;

private:
	// Attribute delegate callbacks
	void OnLevelingChanged(const FOnAttributeChangeData& Data);

	void OnOffenseChanged(const FOnAttributeChangeData& Data);

	void OnDefenseChanged(const FOnAttributeChangeData& Data);

	void OnResistanceChanged(const FOnAttributeChangeData& Data);

	void OnPandoraForceChanged(const FOnAttributeChangeData& Data);

	void OnAgilityChanged(const FOnAttributeChangeData& Data);

	void OnInvestmentPointChanged(const FOnAttributeChangeData& Data);

	void OnStatLevelChanged(const FOnAttributeChangeData& Data);

	void OnHealthChanged(const FOnAttributeChangeData& Data);

	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);

	void OnShieldChanged(const FOnAttributeChangeData& Data);

	void OnMaxShieldChanged(const FOnAttributeChangeData& Data);

	void OnManaChanged(const FOnAttributeChangeData& Data);

	void OnMaxManaChanged(const FOnAttributeChangeData& Data);

	void OnStaminaChanged(const FOnAttributeChangeData& Data);

	void OnMaxStaminaChanged(const FOnAttributeChangeData& Data);

	void OnResourceIncreasePercentChanged(const FOnAttributeChangeData& Data);

	void OnEquipmentStatsChanged();

	void RefreshEquipmentComponentBinding();

	void ClearEquipmentComponentBinding();

public:
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Leveling")
	float Level = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Experience")
	float CurrentExperience = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Experience")
	float MaxExperience = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Experience")
	float ExperiencePercent = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float Strength = 10.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float FinalStrength = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float Intelligence = 10.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Agility")
	float Arcane = 10.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float Armor = 2.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float FinalArmor = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float Recovery = 2.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float FinalRecovery = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float MaxShield = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resistance", meta = (DisplayName = "Frostbite"))
	float Frostbite = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resistance", meta = (DisplayName = "Burn"))
	float Burn = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resistance", meta = (DisplayName = "Electric Shock"))
	float ElectricShock = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|PandoraForce")
	float FirstPandora = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|PandoraForce")
	float SecondPandora = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|PandoraForce")
	float ThirdPandora = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Agility", meta = (ForceUnits = "%"))
	float AttackSpeed = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Agility", meta = (ForceUnits = "%"))
	float MovementSpeed = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float Critical = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float FinalCriticalDamage = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusStrengthText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusIntelligenceText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusArcaneText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusArmorText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusRecoveryText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusMaxShieldText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusFrostbiteText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusBurnText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusElectricShockText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusFirstPandoraText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusSecondPandoraText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusThirdPandoraText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusMaxHealthText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusMaxManaText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusMaxStaminaText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusAttackSpeedText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusMovementSpeedText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Equipment Bonus")
	FText EquipmentBonusCriticalText;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Point")
	float OffensePoint = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Point")
	float DefensePoint = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Point")
	float ResistancePoint = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Point")
	float PandoraForcePoint = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Point")
	float ResourcePoint = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Point")
	float AgilityPoint = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float StrengthLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float IntelligenceLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float ArcaneLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float ArmorLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float RecoveryLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float MaxShieldLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level", meta = (DisplayName = "Frostbite Level"))
	float FrostbiteLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level", meta = (DisplayName = "Burn Level"))
	float BurnLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level", meta = (DisplayName = "Electric Shock Level"))
	float ElectricShockLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float FirstPandoraLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float SecondPandoraLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float ThirdPandoraLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float MaxHealthLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float MaxManaLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float MaxStaminaLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resource", meta = (ForceUnits = "%"))
	float MaxHealthIncreasePercent = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense", meta = (ForceUnits = "%"))
	float MaxShieldIncreasePercent = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resource", meta = (ForceUnits = "%"))
	float MaxManaIncreasePercent = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resource", meta = (ForceUnits = "%"))
	float MaxStaminaIncreasePercent = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float AttackSpeedLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float MovementSpeedLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stat Level")
	float CriticalLevel = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Health")
	float Health = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Health")
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Health")
	float HealthPercent = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Shield")
	float Shield = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Shield")
	float ShieldPercent = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Mana")
	float Mana = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Mana")
	float MaxMana = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Mana")
	float ManaPercent = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stamina")
	float Stamina = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stamina")
	float MaxStamina = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stamina")
	float StaminaPercent = 1.f;

	void UpdateLevelingData();

	void UpdateOffenseData();

	void UpdateDefenseData();

	void UpdateResistanceData();

	void UpdatePandoraForceData();

	void UpdateAgilityData();

	void UpdateInvestmentPointData();

	void UpdateStatLevelData();

	void UpdateEquipmentDerivedData();

	void UpdateHealthData();

	void UpdateShieldData();

	void UpdateManaData();

	void UpdateStaminaData();

	void UpdateAllData();

public:
	static const FName ViewModelName;

protected:
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY()
	TWeakObjectPtr<UEquipmentComponent> EquipmentComponent;
};
