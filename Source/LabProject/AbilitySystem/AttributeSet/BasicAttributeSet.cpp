#include "BasicAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Engine/World.h"
#include "AbilitySystem/AttributeSet/DamageRules.h"
#include "Character/CharacterBase.h"
#include "Common/Enum_Direction.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Mode/PlayerEliminationSubsystem.h"
#include "Net/Core/PushModel/PushModel.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(BasicAttributeSet)

namespace
{
	constexpr float MaxInvestedStatLevel = 100.f;

	float ClampResourceAttribute(float Value, float MaxValue)
	{
		return FMath::Clamp(Value, 0.f, FMath::Max(MaxValue, 0.f));
	}

	// 스탯 포인트를 투자한 단계. 0~100단계로 제한한다.
	bool IsInvestedStatLevelAttribute(const FGameplayAttribute& Attribute)
	{
		static const FGameplayAttribute InvestedStatLevelAttributes[] = {
			UBasicAttributeSet::GetStrengthLevelAttribute(),
			UBasicAttributeSet::GetIntelligenceLevelAttribute(),
			UBasicAttributeSet::GetArcaneLevelAttribute(),
			UBasicAttributeSet::GetArmorLevelAttribute(),
			UBasicAttributeSet::GetRecoveryLevelAttribute(),
			UBasicAttributeSet::GetFrostbiteLevelAttribute(),
			UBasicAttributeSet::GetBurnLevelAttribute(),
			UBasicAttributeSet::GetElectricShockLevelAttribute(),
			UBasicAttributeSet::GetFirstPandoraLevelAttribute(),
			UBasicAttributeSet::GetSecondPandoraLevelAttribute(),
			UBasicAttributeSet::GetThirdPandoraLevelAttribute(),
			UBasicAttributeSet::GetMaxHealthLevelAttribute(),
			UBasicAttributeSet::GetMaxShieldLevelAttribute(),
			UBasicAttributeSet::GetMaxManaLevelAttribute(),
			UBasicAttributeSet::GetMaxStaminaLevelAttribute(),
			UBasicAttributeSet::GetAttackSpeedLevelAttribute(),
			UBasicAttributeSet::GetMovementSpeedLevelAttribute(),
			UBasicAttributeSet::GetCriticalLevelAttribute(),
		};
		return MakeArrayView(InvestedStatLevelAttributes).Contains(Attribute);
	}

	// 경험치·스탯 포인트와 단계·장비에서 나온 능력치. 음수만 막는다.
	bool IsNonNegativeStatAttribute(const FGameplayAttribute& Attribute)
	{
		static const FGameplayAttribute NonNegativeStatAttributes[] = {
			UBasicAttributeSet::GetExperienceAttribute(),
			UBasicAttributeSet::GetMaxExperienceAttribute(),
			UBasicAttributeSet::GetOffensePointAttribute(),
			UBasicAttributeSet::GetDefensePointAttribute(),
			UBasicAttributeSet::GetResistancePointAttribute(),
			UBasicAttributeSet::GetPandoraForcePointAttribute(),
			UBasicAttributeSet::GetResourcePointAttribute(),
			UBasicAttributeSet::GetAgilityPointAttribute(),
			UBasicAttributeSet::GetStrengthAttribute(),
			UBasicAttributeSet::GetIntelligenceAttribute(),
			UBasicAttributeSet::GetArcaneAttribute(),
			UBasicAttributeSet::GetArmorAttribute(),
			UBasicAttributeSet::GetRecoveryAttribute(),
			UBasicAttributeSet::GetFrostbiteAttribute(),
			UBasicAttributeSet::GetBurnAttribute(),
			UBasicAttributeSet::GetElectricShockAttribute(),
			UBasicAttributeSet::GetFirstPandoraAttribute(),
			UBasicAttributeSet::GetSecondPandoraAttribute(),
			UBasicAttributeSet::GetThirdPandoraAttribute(),
			UBasicAttributeSet::GetMaxHealthIncreasePercentAttribute(),
			UBasicAttributeSet::GetMaxShieldIncreasePercentAttribute(),
			UBasicAttributeSet::GetMaxManaIncreasePercentAttribute(),
			UBasicAttributeSet::GetMaxStaminaIncreasePercentAttribute(),
			UBasicAttributeSet::GetAttackSpeedAttribute(),
			UBasicAttributeSet::GetMovementSpeedAttribute(),
			UBasicAttributeSet::GetCriticalAttribute(),
		};
		return MakeArrayView(NonNegativeStatAttributes).Contains(Attribute);
	}

	// 레벨·성장 값·능력치를 속성별 허용 범위로 자른다. 범위가 정해진 속성이 아니면 false.
	bool TryClampToAttributeRange(const FGameplayAttribute& Attribute, float& InOutValue)
	{
		if (Attribute == UBasicAttributeSet::GetLevelAttribute())
		{
			InOutValue = FMath::Max(InOutValue, 1.f);
			return true;
		}

		if (IsInvestedStatLevelAttribute(Attribute))
		{
			InOutValue = FMath::Clamp(InOutValue, 0.f, MaxInvestedStatLevel);
			return true;
		}

		if (IsNonNegativeStatAttribute(Attribute))
		{
			InOutValue = FMath::Max(InOutValue, 0.f);
			return true;
		}

		return false;
	}

	bool EffectSpecHasAssetTag(const FGameplayEffectSpec& EffectSpec, const FGameplayTag& AssetTag)
	{
		return AssetTag.IsValid() && EffectSpec.Def && EffectSpec.Def->GetAssetTags().HasTag(AssetTag);
	}

	float ResolveStatusResistance(const UBasicAttributeSet& AttributeSet,
		const PdDamageRules::EStatusDamage StatusDamage)
	{
		switch (StatusDamage)
		{
		case PdDamageRules::EStatusDamage::Burning:
			return AttributeSet.GetBurnLevel();
		case PdDamageRules::EStatusDamage::Frozen:
			return AttributeSet.GetFrostbiteLevel();
		case PdDamageRules::EStatusDamage::ElectricShock:
			return AttributeSet.GetElectricShockLevel();
		default:
			return 0.f;
		}
	}

	bool TryActivateAbilityByTag(UAbilitySystemComponent* ASC, const FGameplayTag& AbilityTag)
	{
		if (!ASC || !AbilityTag.IsValid())
		{
			return false;
		}

		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(AbilityTag);
		return ASC->TryActivateAbilitiesByTag(AbilityTags, true);
	}

	void TryActivateHitReactionAbility(UAbilitySystemComponent* ASC)
	{
		if (TryActivateAbilityByTag(ASC, LabGameplayTags::GameplayAbility_HitReaction))
		{
			return;
		}

		TryActivateAbilityByTag(ASC, LabGameplayTags::Action_HitReact);
	}

	void TryActivateDeathAbility(UAbilitySystemComponent* ASC)
	{
		if (!ASC)
		{
			return;
		}

		const int32 DeadTagCount = ASC->GetTagCount(LabGameplayTags::State_Dead);
		if (DeadTagCount > 0)
		{
			return;
		}

		FGameplayTagContainer DeathAbilityTags;
		DeathAbilityTags.AddTag(LabGameplayTags::GameplayAbility_Death);

		TArray<FGameplayAbilitySpec*> DeathAbilitySpecs;
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(DeathAbilityTags, DeathAbilitySpecs, false);
		for (const FGameplayAbilitySpec* DeathAbilitySpec : DeathAbilitySpecs)
		{
			if (DeathAbilitySpec && ASC->TryActivateAbility(DeathAbilitySpec->Handle, true))
			{
				return;
			}
		}
	}
}

UBasicAttributeSet::UBasicAttributeSet()
{
	Level = 1.0f;
	Experience = 0.0f;
	MaxExperience = 0.0f;
	OffensePoint = 0.0f;
	DefensePoint = 0.0f;
	ResistancePoint = 0.0f;
	PandoraForcePoint = 0.0f;
	ResourcePoint = 0.0f;
	AgilityPoint = 0.0f;
	StrengthLevel = 0.0f;
	IntelligenceLevel = 0.0f;
	ArcaneLevel = 0.0f;
	ArmorLevel = 0.0f;
	RecoveryLevel = 0.0f;
	FrostbiteLevel = 0.0f;
	BurnLevel = 0.0f;
	ElectricShockLevel = 0.0f;
	FirstPandoraLevel = 0.0f;
	SecondPandoraLevel = 0.0f;
	ThirdPandoraLevel = 0.0f;
	MaxHealthLevel = 0.0f;
	MaxShieldLevel = 0.0f;
	MaxManaLevel = 0.0f;
	MaxStaminaLevel = 0.0f;
	AttackSpeedLevel = 0.0f;
	MovementSpeedLevel = 0.0f;
	CriticalLevel = 0.0f;
	Health = 0.0f;
	MaxHealth = 0.0f;
	Shield = 0.0f;
	MaxShield = 0.0f;
	Stamina = 0.0f;
	MaxStamina = 0.0f;
	MaxHealthIncreasePercent = 0.0f;
	MaxShieldIncreasePercent = 0.0f;
	MaxManaIncreasePercent = 0.0f;
	MaxStaminaIncreasePercent = 0.0f;
}

bool UBasicAttributeSet::ResolveAttributeFromStatTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute)
{
	static const TMap<FGameplayTag, FGameplayAttribute> AttributeMappings = {
		{ LabGameplayTags::Status_Level, GetLevelAttribute() },
		{ LabGameplayTags::Status_Experience, GetExperienceAttribute() },
		{ LabGameplayTags::Status_MaxExperience, GetMaxExperienceAttribute() },
		{ LabGameplayTags::Status_Point_Offense, GetOffensePointAttribute() },
		{ LabGameplayTags::Status_Point_Defense, GetDefensePointAttribute() },
		{ LabGameplayTags::Status_Point_Resistance, GetResistancePointAttribute() },
		{ LabGameplayTags::Status_Point_PandoraForce, GetPandoraForcePointAttribute() },
		{ LabGameplayTags::Status_Point_Resource, GetResourcePointAttribute() },
		{ LabGameplayTags::Status_Point_Agility, GetAgilityPointAttribute() },
		{ LabGameplayTags::Status_Offense_Strength, GetStrengthAttribute() },
		{ LabGameplayTags::Status_Offense_Intelligence, GetIntelligenceAttribute() },
		{ LabGameplayTags::Status_Offense_Critical, GetCriticalAttribute() },
		{ LabGameplayTags::Status_Offense_StrengthLevel, GetStrengthLevelAttribute() },
		{ LabGameplayTags::Status_Offense_IntelligenceLevel, GetIntelligenceLevelAttribute() },
		{ LabGameplayTags::Status_Offense_CriticalLevel, GetCriticalLevelAttribute() },
		{ LabGameplayTags::Status_Defense_Armor, GetArmorAttribute() },
		{ LabGameplayTags::Status_Defense_Recovery, GetRecoveryAttribute() },
		{ LabGameplayTags::Status_Defense_Shield, GetShieldAttribute() },
		{ LabGameplayTags::Status_Defense_MaxShield, GetMaxShieldAttribute() },
		{ LabGameplayTags::Status_Defense_MaxShieldIncreasePercent, GetMaxShieldIncreasePercentAttribute() },
		{ LabGameplayTags::Status_Defense_ArmorLevel, GetArmorLevelAttribute() },
		{ LabGameplayTags::Status_Defense_RecoveryLevel, GetRecoveryLevelAttribute() },
		{ LabGameplayTags::Status_Defense_MaxShieldLevel, GetMaxShieldLevelAttribute() },
		{ LabGameplayTags::Status_Resistance_Frostbite, GetFrostbiteAttribute() },
		{ LabGameplayTags::Status_Resistance_Burn, GetBurnAttribute() },
		{ LabGameplayTags::Status_Resistance_ElectricShock, GetElectricShockAttribute() },
		{ LabGameplayTags::Status_Resistance_FrostbiteLevel, GetFrostbiteLevelAttribute() },
		{ LabGameplayTags::Status_Resistance_BurnLevel, GetBurnLevelAttribute() },
		{ LabGameplayTags::Status_Resistance_ElectricShockLevel, GetElectricShockLevelAttribute() },
		{ LabGameplayTags::Status_PandoraForce_FirstPandora, GetFirstPandoraAttribute() },
		{ LabGameplayTags::Status_PandoraForce_SecondPandora, GetSecondPandoraAttribute() },
		{ LabGameplayTags::Status_PandoraForce_ThirdPandora, GetThirdPandoraAttribute() },
		{ LabGameplayTags::Status_PandoraForce_FirstPandoraLevel, GetFirstPandoraLevelAttribute() },
		{ LabGameplayTags::Status_PandoraForce_SecondPandoraLevel, GetSecondPandoraLevelAttribute() },
		{ LabGameplayTags::Status_PandoraForce_ThirdPandoraLevel, GetThirdPandoraLevelAttribute() },
		{ LabGameplayTags::Status_Resource_Health, GetHealthAttribute() },
		{ LabGameplayTags::Status_Resource_Mana, GetManaAttribute() },
		{ LabGameplayTags::Status_Resource_Stamina, GetStaminaAttribute() },
		{ LabGameplayTags::Status_Resource_MaxHealth, GetMaxHealthAttribute() },
		{ LabGameplayTags::Status_Resource_MaxMana, GetMaxManaAttribute() },
		{ LabGameplayTags::Status_Resource_MaxStamina, GetMaxStaminaAttribute() },
		{ LabGameplayTags::Status_Resource_MaxHealthIncreasePercent, GetMaxHealthIncreasePercentAttribute() },
		{ LabGameplayTags::Status_Resource_MaxManaIncreasePercent, GetMaxManaIncreasePercentAttribute() },
		{ LabGameplayTags::Status_Resource_MaxStaminaIncreasePercent, GetMaxStaminaIncreasePercentAttribute() },
		{ LabGameplayTags::Status_Resource_MaxHealthLevel, GetMaxHealthLevelAttribute() },
		{ LabGameplayTags::Status_Resource_MaxManaLevel, GetMaxManaLevelAttribute() },
		{ LabGameplayTags::Status_Resource_MaxStaminaLevel, GetMaxStaminaLevelAttribute() },
		{ LabGameplayTags::Status_Agility_AttackSpeed, GetAttackSpeedAttribute() },
		{ LabGameplayTags::Status_Agility_MovementSpeed, GetMovementSpeedAttribute() },
		{ LabGameplayTags::Status_Agility_Arcane, GetArcaneAttribute() },
		{ LabGameplayTags::Status_Agility_CriticalDamageMultiplier, GetCriticalDamageMultiplierAttribute() },
		{ LabGameplayTags::Status_Agility_AttackSpeedLevel, GetAttackSpeedLevelAttribute() },
		{ LabGameplayTags::Status_Agility_MovementSpeedLevel, GetMovementSpeedLevelAttribute() },
		{ LabGameplayTags::Status_Agility_ArcaneLevel, GetArcaneLevelAttribute() }
	};

	const FGameplayAttribute* Attribute = AttributeMappings.Find(StatTag);
	OutAttribute = Attribute ? *Attribute : FGameplayAttribute();
	return OutAttribute.IsValid();
}

void UBasicAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// =================================================================================================================

	const FGameplayAttribute& Attribute = Data.EvaluatedData.Attribute;

	// 공격 피해는 CalculateOutgoingDamage로 타격 시점에 계산하므로 이 메타 속성으로 들어온 값은 쓰지 않는다.
	if (Attribute == GetOutgoingDamageAttribute())
	{
		SetOutgoingDamage(0.f);
		return;
	}

	// GE가 바꾼 레벨·성장 값·능력치의 기본값을 허용 범위로 되돌린다.
	// 장비·투자처럼 계속 걸린 효과는 현재값에만 더해지므로, 현재값을 기본값에 쓰면 그 효과가 두 번 들어간다.
	const FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(this);
	float ClampedBaseValue = AttributeData ? AttributeData->GetBaseValue() : 0.f;
	if (AttributeData && TryClampToAttributeRange(Attribute, ClampedBaseValue))
	{
		if (ClampedBaseValue != AttributeData->GetBaseValue())
		{
			SetAttributeBaseValue(Attribute, ClampedBaseValue);
		}
		return;
	}

	if (Attribute == GetIncomingDamageAttribute())
	{
		HandleIncomingDamageExecuted(Data.EffectSpec);
		return;
	}

	if (Attribute == GetHealthAttribute())
	{
		HandleHealthExecuted(Data);
		return;
	}

	if (Attribute == GetStaminaAttribute())
	{
		SetStamina(GetStamina());
		return;
	}

	if (Attribute == GetManaAttribute())
	{
		SetMana(GetMana());
	}
}

void UBasicAttributeSet::SetAttributeBaseValue(const FGameplayAttribute& Attribute, const float NewValue)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();
	if (ensure(AbilityComp))
	{
		AbilityComp->SetNumericAttributeBase(Attribute, NewValue);
	}
}

// 들어온 피해에 상태 저항과 방어력을 반영해 보호막·체력에 나눠 적용하고, 조건이 맞으면 피격 반응을 켠다.
void UBasicAttributeSet::HandleIncomingDamageExecuted(const FGameplayEffectSpec& EffectSpec)
{
	const float AppliedIncomingDamage = GetIncomingDamage();
	// 공격자가 이 피해 Spec에 붙인 표시. 태그가 없는 피해(스킬·상태 이상)는 일반 피해로 처리한다.
	const FGameplayTagContainer& DamageSpecTags = EffectSpec.GetDynamicAssetTags();
	const bool bCriticalHit = DamageSpecTags.HasTagExact(LabGameplayTags::Effect_Damage_Critical);
	const bool bAllowHitReact = !DamageSpecTags.HasTagExact(LabGameplayTags::Effect_Damage_NoHitReaction);
	const FGameplayEffectContextHandle& EffectContext = EffectSpec.GetContext();
	AActor* DamageInstigator = EffectContext.GetOriginalInstigator();
	if (!DamageInstigator)
	{
		DamageInstigator = EffectContext.GetInstigator();
	}
	AActor* DamageCauser = EffectContext.GetEffectCauser();
	const PdDamageRules::EStatusDamage StatusDamage = PdDamageRules::ClassifyStatusDamage(EffectSpec);
	const bool bStatusDamage = StatusDamage != PdDamageRules::EStatusDamage::None;
	const float StatusMitigatedIncomingDamage = bStatusDamage
		? PdDamageRules::MitigateByStatusResistance(AppliedIncomingDamage, ResolveStatusResistance(*this, StatusDamage))
		: AppliedIncomingDamage;
	const float TargetArmor = GetArmor();
	// 방어력은 맞는 쪽 자신의 근력 반영 무기 피해에 비례한다. 그 값은 전투 컴포넌트가 ASC에 등록해 둔 계산으로 얻는다.
	const UPdAbilitySystemComponent* OwningAbilitySystem = Cast<UPdAbilitySystemComponent>(GetOwningAbilitySystemComponent());
	const float TargetFinalStrength = OwningAbilitySystem ? OwningAbilitySystem->GetFinalStrengthDamage(GetStrength()) : 0.f;
	const float MitigatedIncomingDamage = PdDamageRules::MitigateByArmor(StatusMitigatedIncomingDamage, TargetArmor, TargetFinalStrength);

	SetIncomingDamage(0.f);
	const bool bAllowDamageHitReact = bAllowHitReact && !bStatusDamage;
	const float HealthDamage = ApplyIncomingDamage(MitigatedIncomingDamage, bCriticalHit, DamageInstigator,
		DamageCauser, bAllowDamageHitReact, !bStatusDamage);
	const bool bShouldHitReact = bAllowHitReact && !FMath::IsNearlyZero(HealthDamage) && !bStatusDamage
		&& EffectSpecHasAssetTag(EffectSpec, LabGameplayTags::Effect_HitReaction);

	if (bShouldHitReact && GetHealth() > 0.f)
	{
		TryActivateHitReactionAbility(GetOwningAbilitySystemComponent());
	}
}

// 체력을 직접 바꾸는 GE. 회복 GE에 실린 마나도 함께 채우고, 상태 이상이 아닌 피해면 피격 반응을 켠다.
void UBasicAttributeSet::HandleHealthExecuted(const FGameplayEffectModCallbackData& Data)
{
	const bool bStatusDamage = PdDamageRules::ClassifyStatusDamage(Data.EffectSpec) != PdDamageRules::EStatusDamage::None;
	const bool bShouldHitReact = Data.EvaluatedData.Magnitude < 0.f && !bStatusDamage
		&& EffectSpecHasAssetTag(Data.EffectSpec, LabGameplayTags::Effect_HitReaction);
	const float RecoveryManaMagnitude = Data.EvaluatedData.Magnitude > 0.f
		? Data.EffectSpec.GetSetByCallerMagnitude(LabGameplayTags::Data_Mana, false, 0.f)
		: 0.f;
	if (RecoveryManaMagnitude > 0.f)
	{
		const float OldMana = GetMana();
		const float NewMana = ClampResourceAttribute(OldMana + RecoveryManaMagnitude, GetMaxMana());
		SetMana(NewMana);
		MARK_PROPERTY_DIRTY_FROM_NAME(UBasicAttributeSet, Mana, this);
	}

	SetHealth(GetHealth());
	if (bShouldHitReact && GetHealth() > 0.f)
	{
		TryActivateHitReactionAbility(GetOwningAbilitySystemComponent());
	}
}

void UBasicAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// =================================================================================================================

	if (Attribute == GetHealthAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxShield());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxStamina());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxMana());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxShieldAttribute()
		|| Attribute == GetMaxStaminaAttribute() || Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else
	{
		TryClampToAttributeRange(Attribute, NewValue);
	}
}

void UBasicAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// =================================================================================================================

	if (OldValue != NewValue)
	{
		if (FProperty* Property = Attribute.GetUProperty())
		{
			MARK_PROPERTY_DIRTY(this, Property);
		}
	}

	// =================================================================================================================

	if (Attribute == GetMaxHealthAttribute())
	{
		SetHealth(GetHealth());
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		SetShield(GetShield());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		SetStamina(GetStamina());
	}
	else if (Attribute == GetMaxManaAttribute())
	{
		SetMana(GetMana());
	}

	if (Attribute == GetHealthAttribute() && OldValue > 0.f && NewValue <= 0.f)
	{
		TryActivateDeathAbility(GetOwningAbilitySystemComponent());
	}

	if (Attribute == GetShieldAttribute())
	{
		if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
		{
			if (AActor* OwnerActor = ASC->GetOwnerActor(); OwnerActor && OwnerActor->HasAuthority())
			{
				if (OldValue <= 0.f && NewValue > 0.f)
				{
					ASC->AddGameplayCue(LabGameplayTags::GameplayCue_ShieldUp);
				}
				else if (OldValue > 0.f && NewValue <= 0.f)
				{
					ASC->RemoveGameplayCue(LabGameplayTags::GameplayCue_ShieldUp);
					ASC->ExecuteGameplayCue(LabGameplayTags::GameplayCue_ShieldDown);
				}
			}
		}
	}
}

float UBasicAttributeSet::CalculateCooldownDuration(const float BaseCooldownDuration) const
{
	const float ReductionPercent = FMath::Clamp(GetArcane(), 0.0f, 100.0f);
	return FMath::Max(BaseCooldownDuration, 0.0f) * (1.0f - ReductionPercent / 100.0f);
}

float UBasicAttributeSet::GetAttackSpeedPlayRate() const
{
	return 1.0f + FMath::Max(GetAttackSpeed(), 0.0f) * 0.01f;
}

float UBasicAttributeSet::GetPandoraLoadoutDamageBonusPercent(const EEnum_Direction LoadoutDirection) const
{
	switch (LoadoutDirection)
	{
	case EEnum_Direction::Left:
		return FMath::Max(GetFirstPandora(), 0.0f);
	case EEnum_Direction::Up:
		return FMath::Max(GetSecondPandora(), 0.0f);
	case EEnum_Direction::Right:
		return FMath::Max(GetThirdPandora(), 0.0f);
	default:
		return 0.0f;
	}
}

float UBasicAttributeSet::GetStatusEffectDamageBonusPercent(const FGameplayTag& StatusTag) const
{
	if (!StatusTag.IsValid())
	{
		return 0.0f;
	}

	if (StatusTag.MatchesTag(LabGameplayTags::Status_Burning))
	{
		return FMath::Max(GetBurn(), 0.0f);
	}

	if (StatusTag.MatchesTag(LabGameplayTags::Status_Frostbite))
	{
		return FMath::Max(GetFrostbite(), 0.0f);
	}

	if (StatusTag.MatchesTag(LabGameplayTags::Status_ElectricShock))
	{
		return FMath::Max(GetElectricShock(), 0.0f);
	}

	return 0.0f;
}

float UBasicAttributeSet::CalculateStatusEffectDamage(const FGameplayTag& StatusTag,
	const float SkillScaledDamageMagnitude, const float DamageScale) const
{
	return PdDamageRules::CalculateStatusEffectDamage(SkillScaledDamageMagnitude,
		GetStatusEffectDamageBonusPercent(StatusTag), DamageScale);
}

bool UBasicAttributeSet::SetStatusEffectDamageOnSpec(FGameplayEffectSpecHandle& SpecHandle,
	const UBasicAttributeSet* SourceAttributes, const FGameplayTag& StatusTag, const float SkillScaledDamageMagnitude,
	const float DamageScale)
{
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	const float CalculatedDamageMagnitude = PdDamageRules::CalculateStatusEffectDamage(SkillScaledDamageMagnitude,
		SourceAttributes ? SourceAttributes->GetStatusEffectDamageBonusPercent(StatusTag) : 0.0f, DamageScale);
	if (CalculatedDamageMagnitude <= 0.0f)
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_Damage, CalculatedDamageMagnitude);
	return true;
}

float UBasicAttributeSet::CalculateOutgoingDamage(const float BaseDamage, bool& bOutCriticalHit) const
{
	return PdDamageRules::CalculateCriticalDamage(BaseDamage, GetCritical(), FMath::FRand() * 100.f, bOutCriticalHit);
}

float UBasicAttributeSet::ApplyIncomingDamage(const float IncomingDamageAmount, const bool bCriticalHit,
	AActor* DamageInstigator, AActor* DamageCauser, const bool bAllowHitReact, const bool bShowMiss)
{
	const float FinalDamage = FMath::Max(IncomingDamageAmount, 0.f);
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	ACharacterBase* DamageTargetCharacter = ASC ? Cast<ACharacterBase>(ASC->GetAvatarActor()) : nullptr;

	if (FinalDamage <= 0.f)
	{
		if (bShowMiss && DamageTargetCharacter)
		{
			DamageTargetCharacter->HandleDamageTaken(0.0f, false, false, DamageInstigator, DamageCauser);
		}
		return 0.f;
	}

	if (ASC && ASC->HasMatchingGameplayTag(LabGameplayTags::State_DefenseField_Invulnerable))
	{
		if (DamageTargetCharacter)
		{
			DamageTargetCharacter->HandleDamageTaken(0.0f, false, false, DamageInstigator, DamageCauser);
		}
		return 0.f;
	}

	const PdDamageRules::FShieldAbsorption Absorption = PdDamageRules::AbsorbByShield(FinalDamage, GetShield());
	if (Absorption.ShieldDamage > 0.f)
	{
		SetShield(Absorption.RemainingShield);
	}
	const float RemainingHealthDamage = Absorption.HealthDamage;

	if (DamageTargetCharacter)
	{
		const float DisplayDamage = Absorption.ShieldDamage + RemainingHealthDamage;
		DamageTargetCharacter->HandleDamageTaken(DisplayDamage, bCriticalHit,
			bAllowHitReact && RemainingHealthDamage > 0.0f, DamageInstigator, DamageCauser);
	}

	if (RemainingHealthDamage <= 0.f)
	{
		return 0.f;
	}

	const float OldHealth = GetHealth();
	const float NewHealth = FMath::Max(OldHealth - RemainingHealthDamage, 0.f);
	SetHealth(NewHealth);
	if (OldHealth > 0.f && NewHealth <= 0.f)
	{
		if (UPlayerEliminationSubsystem* Eliminations = UWorld::GetSubsystem<UPlayerEliminationSubsystem>(GetWorld()))
		{
			Eliminations->HandleEliminated(GetOwningActor(), DamageInstigator, DamageCauser);
		}
		TryActivateDeathAbility(GetOwningAbilitySystemComponent());
	}

	return RemainingHealthDamage;
}

