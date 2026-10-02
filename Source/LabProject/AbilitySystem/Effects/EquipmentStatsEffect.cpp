#include "AbilitySystem/Effects/EquipmentStatsEffect.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentStatsEffect)

UEquipmentStatsEffect::UEquipmentStatsEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	for (const FGameplayTag& StatTag : GetStatTags())
	{
		FGameplayAttribute Attribute;
		if (!UBasicAttributeSet::ResolveAttributeFromStatTag(StatTag, Attribute))
		{
			continue;
		}

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = StatTag;
		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	}
}

TConstArrayView<FGameplayTag> UEquipmentStatsEffect::GetStatTags()
{
	static const FGameplayTag StatTags[] =
	{
		LabGameplayTags::Status_Offense_Strength,
		LabGameplayTags::Status_Offense_Intelligence,
		LabGameplayTags::Status_Offense_Critical,
		LabGameplayTags::Status_Defense_Armor,
		LabGameplayTags::Status_Defense_Recovery,
		LabGameplayTags::Status_Defense_MaxShield,
		LabGameplayTags::Status_Resistance_Frostbite,
		LabGameplayTags::Status_Resistance_Burn,
		LabGameplayTags::Status_Resistance_ElectricShock,
		LabGameplayTags::Status_Resource_MaxHealth,
		LabGameplayTags::Status_Resource_MaxMana,
		LabGameplayTags::Status_Resource_MaxStamina,
		LabGameplayTags::Status_Agility_AttackSpeed,
		LabGameplayTags::Status_Agility_MovementSpeed,
		LabGameplayTags::Status_Agility_Arcane,
	};
	return StatTags;
}

FGameplayEffectSpecHandle UEquipmentStatsEffect::MakeSpec(
	const UAbilitySystemComponent& AbilitySystem,
	const TMap<FGameplayTag, float>& StatMagnitudes,
	const UObject* SourceObject)
{
	FGameplayEffectContextHandle Context = AbilitySystem.MakeEffectContext();
	Context.AddSourceObject(SourceObject);
	FGameplayEffectSpecHandle SpecHandle = AbilitySystem.MakeOutgoingSpec(StaticClass(), 1.0f, Context);
	if (SpecHandle.IsValid())
	{
		// 모든 수정자가 SetByCaller라 값을 넣지 않은 태그는 경고가 나므로, 아이템에 없는 능력치도 0으로 채운다.
		for (const FGameplayTag& StatTag : GetStatTags())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(StatTag, StatMagnitudes.FindRef(StatTag));
		}
	}
	return SpecHandle;
}
