#pragma once

#include "CoreMinimal.h"

class UImage;
class USkillDefinition;

enum class ESkillEffectIconSet : uint8
{
	SkillTip,
	PandoraDescription
};

namespace PdSkillEffectIconResolver
{
	void ApplySkillEffectIcon(
		const UObject* WorldContextObject,
		const USkillDefinition* Skill,
		UImage* ImageWidget,
		ESkillEffectIconSet IconSet);
}
