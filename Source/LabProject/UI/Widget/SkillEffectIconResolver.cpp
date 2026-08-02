#include "UI/Widget/SkillEffectIconResolver.h"

#include "Components/Image.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/Texture2D.h"

namespace
{
	enum class ESkillEffectIconType : uint8
	{
		None,
		Burn,
		Frostbite,
		ElectricShock,
		Shield
	};

	ESkillEffectIconType ResolveSkillEffectIconType(const FSkill& Skill)
	{
		const USkillDefinition* SkillDefinition = Skill.SkillDefinition.Get();
		if (!SkillDefinition)
		{
			return ESkillEffectIconType::None;
		}

		if (SkillDefinition->bShowBurnEffectIcon)
		{
			return ESkillEffectIconType::Burn;
		}

		if (SkillDefinition->bShowFrostbiteEffectIcon)
		{
			return ESkillEffectIconType::Frostbite;
		}

		if (SkillDefinition->bShowElectricShockEffectIcon)
		{
			return ESkillEffectIconType::ElectricShock;
		}

		return SkillDefinition->bShowShieldEffectIcon
			? ESkillEffectIconType::Shield
			: ESkillEffectIconType::None;
	}

	UTexture2D* ResolveConfiguredImage(
		const UObject* WorldContextObject,
		const ESkillEffectIconType EffectIconType,
		const ESkillEffectIconSet IconSet)
	{
		const UWidgetClassDefinition* WidgetDefinition =
			UWidgetClassDefinition::ResolveWidgetClassDefinition(WorldContextObject);
		if (!WidgetDefinition)
		{
			return nullptr;
		}

		const FSkillTipWidgetSettings& Settings =
			IconSet == ESkillEffectIconSet::PandoraDescription
				? WidgetDefinition->GetPandoraDescriptionEffectIconSettings()
				: WidgetDefinition->GetSkillTipEffectIconSettings();
		switch (EffectIconType)
		{
		case ESkillEffectIconType::Burn:
			return Settings.BurnImage.Get();
		case ESkillEffectIconType::Frostbite:
			return Settings.FrostbiteImage.Get();
		case ESkillEffectIconType::ElectricShock:
			return Settings.ElectricShockImage.Get();
		case ESkillEffectIconType::Shield:
			return Settings.ShieldImage.Get();
		case ESkillEffectIconType::None:
		default:
			return nullptr;
		}
	}
}

void PdSkillEffectIconResolver::ApplySkillEffectIcon(
	const UObject* WorldContextObject,
	const FSkill* Skill,
	UImage* ImageWidget,
	const ESkillEffectIconSet IconSet)
{
	if (!ImageWidget)
	{
		return;
	}

	const ESkillEffectIconType EffectIconType = Skill
		? ResolveSkillEffectIconType(*Skill)
		: ESkillEffectIconType::None;
	if (EffectIconType == ESkillEffectIconType::None)
	{
		ImageWidget->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	if (UTexture2D* Image = ResolveConfiguredImage(WorldContextObject, EffectIconType, IconSet))
	{
		ImageWidget->SetBrushFromTexture(Image, false);
	}

	ImageWidget->SetVisibility(
		IconSet == ESkillEffectIconSet::SkillTip
			? ESlateVisibility::Visible
			: ESlateVisibility::SelfHitTestInvisible);
}
