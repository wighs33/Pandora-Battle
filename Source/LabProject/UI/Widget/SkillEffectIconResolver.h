#pragma once

#include "CoreMinimal.h"

class UImage;
struct FSkill;

enum class ESkillEffectIconSet : uint8
{
	SkillTip,
	PandoraDescription
};

namespace PdSkillEffectIconResolver
{
	void ApplySkillEffectIcon(
		const UObject* WorldContextObject,
		const FSkill* Skill,
		UImage* ImageWidget,
		ESkillEffectIconSet IconSet);
}
