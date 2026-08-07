#include "Pandora/PandoraSkillRuntimeContext.h"

#include "Definition/Pandora/PandoraDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraSkillRuntimeContext)

void UPandoraSkillRuntimeContext::Initialize(
	const UPandoraDefinition* InPandoraDefinition,
	const USkillDefinition* InSkillDataAsset,
	const int32 InSkillIndex,
	const int32 InPandoraLevel,
	const EEnum_Direction InLoadoutDirection)
{
	PandoraDefinition = InPandoraDefinition;
	SkillDataAsset = InSkillDataAsset;
	SkillIndex = InSkillIndex;
	PandoraLevel = FMath::Max(InPandoraLevel, 1);
	LoadoutDirection = InLoadoutDirection;
}

const FSkill* UPandoraSkillRuntimeContext::GetPandoraSkill() const
{
	const UPandoraDefinition* Definition = PandoraDefinition.Get();
	return Definition && Definition->Skill.IsValidIndex(SkillIndex) ? &Definition->Skill[SkillIndex] : nullptr;
}

TArray<FProjectileImpactEffectAreaSpawnConfig> UPandoraSkillRuntimeContext::GetProjectileImpactEffectAreas() const
{
	const USkillDefinition* SkillData = SkillDataAsset.Get();
	if (!SkillData || SkillData->SkillDataType != ESkillDataType::Projectile)
	{
		return TArray<FProjectileImpactEffectAreaSpawnConfig>();
	}

	return TArray<FProjectileImpactEffectAreaSpawnConfig>();
}
