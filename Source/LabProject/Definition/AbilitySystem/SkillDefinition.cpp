#include "Definition/AbilitySystem/SkillDefinition.h"

#include "AbilitySystem/Skill/SkillAction.h"
#include "Common/LabGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillDefinition)

USkillDefinition::USkillDefinition()
{
	Activation.CooldownRemovalTags.AddTag(LabGameplayTags::Effect_Policy_RemoveOnDeath);

	Damage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	GameplayEffect.MagnitudeDataTag = LabGameplayTags::Data_Damage;
}

FPrimaryAssetId USkillDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Skill"), GetFName());
}

FText USkillDefinition::GetDisplayName() const
{
	return !Name.IsNone() ? FText::FromName(Name) : FText::FromName(GetFName());
}

UObject* USkillDefinition::GetIconResource() const
{
	return Icon.Get();
}

FSkillGameplayEffectConfig USkillDefinition::GetResolvedDamageConfig() const
{
	FSkillGameplayEffectConfig ResolvedDamage = Damage.ToGameplayEffectConfig();
	if (!ResolvedDamage.MagnitudeDataTag.IsValid())
	{
		ResolvedDamage.MagnitudeDataTag = LabGameplayTags::Data_Damage;
	}
	return ResolvedDamage;
}
