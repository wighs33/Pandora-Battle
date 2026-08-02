#include "Animation/AnimNotify_SpawnSkillSlash.h"

#include "Common/WeaponAnimNotifyNames.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_SpawnSkillSlash)

UAnimNotify_SpawnSkillSlash::UAnimNotify_SpawnSkillSlash()
{
	WeaponEventName = WeaponAnimNotifyNames::SpawnSkillSlash();
}

FString UAnimNotify_SpawnSkillSlash::GetNotifyName_Implementation() const
{
	return WeaponAnimNotifyNames::SpawnSkillSlash().ToString();
}
