#include "Pandora/PandoraSkillRuntimeContext.h"

#include "Pandora/PandoraDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSkillRuntimeContext)

void UPandoraSkillRuntimeContext::Initialize(
	const UPandoraDefinition* InPandoraDefinition,
	const USkillDataAsset* InSkillDataAsset,
	const int32 InSkillIndex,
	const int32 InPandoraLevel)
{
	PandoraDefinition = InPandoraDefinition;
	SkillDataAsset = InSkillDataAsset;
	SkillIndex = InSkillIndex;
	PandoraLevel = FMath::Max(InPandoraLevel, 1);
}

const FSkill* UPandoraSkillRuntimeContext::GetPandoraSkill() const
{
	const UPandoraDefinition* Definition = PandoraDefinition.Get();
	return Definition && Definition->Skill.IsValidIndex(SkillIndex) ? &Definition->Skill[SkillIndex] : nullptr;
}

TArray<FProjectileImpactEffectAreaSpawnConfig> UPandoraSkillRuntimeContext::GetProjectileImpactEffectAreasForLevel(const int32 Level) const
{
	if (const FSkill* Skill = GetPandoraSkill())
	{
		return Skill->GetProjectileImpactEffectAreasForLevel(Level);
	}

	return SkillDataAsset ? SkillDataAsset->GetLegacyProjectileImpactEffectAreasForLevel(Level) : TArray<FProjectileImpactEffectAreaSpawnConfig>();
}
