#include "Definition/AbilitySystem/SkillDefinition.h"

#include "Common/PdGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillDefinition)

USkillDefinition::USkillDefinition()
{
	Activation.CooldownRemovalTags.AddTag(PdGameplayTags::Effect_Policy_RemoveOnDeath);

	Damage.MagnitudeDataTag = PdGameplayTags::Data_Damage;
	GameplayEffect.MagnitudeDataTag = PdGameplayTags::Data_Damage;
}

FPrimaryAssetId USkillDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Skill"), GetFName());
}

FText USkillDefinition::GetDisplayName() const
{
	return !DisplayName.IsEmpty() ? DisplayName : FText::FromName(GetFName());
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
		ResolvedDamage.MagnitudeDataTag = PdGameplayTags::Data_Damage;
	}
	return ResolvedDamage;
}
