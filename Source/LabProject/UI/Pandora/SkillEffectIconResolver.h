#pragma once

#include "CoreMinimal.h"

class UImage;
class USkillDefinition;

namespace PdSkillEffectIconResolver
{
	void ApplySkillEffectIcon(
		const UObject* WorldContextObject,
		const USkillDefinition* Skill,
		UImage* ImageWidget);
}
