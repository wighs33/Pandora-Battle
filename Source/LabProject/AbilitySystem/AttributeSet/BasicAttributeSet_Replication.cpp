#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"

#include "Net/UnrealNetwork.h"

// 모든 속성은 푸시 모델로 복제하고, 받은 쪽은 GAS 복제 알림으로 예측값을 맞춘다.
void UBasicAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// =================================================================================================================

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Strength, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Level, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Experience, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxExperience, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, OffensePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, DefensePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ResistancePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, PandoraForcePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ResourcePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, AgilityPoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, StrengthLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, IntelligenceLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ArcaneLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ArmorLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, RecoveryLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, FrostbiteLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, BurnLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ElectricShockLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, FirstPandoraLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, SecondPandoraLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ThirdPandoraLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxHealthLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxShieldLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxManaLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxStaminaLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, AttackSpeedLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MovementSpeedLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, CriticalLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Intelligence, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Arcane, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Armor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Recovery, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Frostbite, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Burn, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ElectricShock, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, FirstPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, SecondPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ThirdPandora, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, AttackSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MovementSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Critical, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, CriticalDamageMultiplier, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Health, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxHealth, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Shield, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxShield, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Mana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxMana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Stamina, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxStamina, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxHealthIncreasePercent, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxShieldIncreasePercent, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxManaIncreasePercent, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxStaminaIncreasePercent, Params);
}

void UBasicAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Strength, OldValue);
}

void UBasicAttributeSet::OnRep_Level(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Level, OldValue);
}

void UBasicAttributeSet::OnRep_Experience(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Experience, OldValue);
}

void UBasicAttributeSet::OnRep_MaxExperience(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxExperience, OldValue);
}

void UBasicAttributeSet::OnRep_OffensePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, OffensePoint, OldValue);
}

void UBasicAttributeSet::OnRep_DefensePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, DefensePoint, OldValue);
}

void UBasicAttributeSet::OnRep_ResistancePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ResistancePoint, OldValue);
}

void UBasicAttributeSet::OnRep_PandoraForcePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, PandoraForcePoint, OldValue);
}

void UBasicAttributeSet::OnRep_ResourcePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ResourcePoint, OldValue);
}

void UBasicAttributeSet::OnRep_AgilityPoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, AgilityPoint, OldValue);
}

void UBasicAttributeSet::OnRep_StrengthLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, StrengthLevel, OldValue);
}

void UBasicAttributeSet::OnRep_IntelligenceLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, IntelligenceLevel, OldValue);
}

void UBasicAttributeSet::OnRep_ArcaneLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ArcaneLevel, OldValue);
}

void UBasicAttributeSet::OnRep_ArmorLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ArmorLevel, OldValue);
}

void UBasicAttributeSet::OnRep_RecoveryLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, RecoveryLevel, OldValue);
}

void UBasicAttributeSet::OnRep_FrostbiteLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, FrostbiteLevel, OldValue);
}

void UBasicAttributeSet::OnRep_BurnLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, BurnLevel, OldValue);
}

void UBasicAttributeSet::OnRep_ElectricShockLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ElectricShockLevel, OldValue);
}

void UBasicAttributeSet::OnRep_FirstPandoraLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, FirstPandoraLevel, OldValue);
}

void UBasicAttributeSet::OnRep_SecondPandoraLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, SecondPandoraLevel, OldValue);
}

void UBasicAttributeSet::OnRep_ThirdPandoraLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ThirdPandoraLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MaxHealthLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxHealthLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MaxShieldLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxShieldLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MaxManaLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxManaLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MaxStaminaLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxStaminaLevel, OldValue);
}

void UBasicAttributeSet::OnRep_AttackSpeedLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, AttackSpeedLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MovementSpeedLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MovementSpeedLevel, OldValue);
}

void UBasicAttributeSet::OnRep_CriticalLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, CriticalLevel, OldValue);
}

void UBasicAttributeSet::OnRep_Intelligence(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Intelligence, OldValue);
}

void UBasicAttributeSet::OnRep_Arcane(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Arcane, OldValue);
}

void UBasicAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Armor, OldValue);
}

void UBasicAttributeSet::OnRep_Recovery(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Recovery, OldValue);
}

void UBasicAttributeSet::OnRep_Frostbite(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Frostbite, OldValue);
}

void UBasicAttributeSet::OnRep_Burn(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Burn, OldValue);
}

void UBasicAttributeSet::OnRep_ElectricShock(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ElectricShock, OldValue);
}

void UBasicAttributeSet::OnRep_FirstPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, FirstPandora, OldValue);
}

void UBasicAttributeSet::OnRep_SecondPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, SecondPandora, OldValue);
}

void UBasicAttributeSet::OnRep_ThirdPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ThirdPandora, OldValue);
}

void UBasicAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, AttackSpeed, OldValue);
}

void UBasicAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MovementSpeed, OldValue);
}

void UBasicAttributeSet::OnRep_Critical(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Critical, OldValue);
}

void UBasicAttributeSet::OnRep_CriticalDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, CriticalDamageMultiplier, OldValue);
}

void UBasicAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Health, OldValue);
}

void UBasicAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxHealth, OldValue);
}

void UBasicAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Shield, OldValue);
}

void UBasicAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxShield, OldValue);
}

void UBasicAttributeSet::OnRep_MaxShieldIncreasePercent(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxShieldIncreasePercent, OldValue);
}

void UBasicAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Mana, OldValue);
}

void UBasicAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxMana, OldValue);
}

void UBasicAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Stamina, OldValue);
}

void UBasicAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxStamina, OldValue);
}

void UBasicAttributeSet::OnRep_MaxHealthIncreasePercent(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxHealthIncreasePercent, OldValue);
}

void UBasicAttributeSet::OnRep_MaxManaIncreasePercent(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxManaIncreasePercent, OldValue);
}

void UBasicAttributeSet::OnRep_MaxStaminaIncreasePercent(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxStaminaIncreasePercent, OldValue);
}
