#include "UI/Pandora/SkillEffectIconResolver.h"

#include "Components/Image.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
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

	ESkillEffectIconType ResolveSkillEffectIconType(const USkillDefinition* SkillDefinition)
	{
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
		const ESkillEffectIconType EffectIconType)
	{
		const UWidgetClassDefinition* WidgetDefinition =
			UWidgetClassDefinition::ResolveWidgetClassDefinition(WorldContextObject);
		if (!WidgetDefinition)
		{
			return nullptr;
		}

		const FSkillTipWidgetSettings& Settings =
			WidgetDefinition->GetPandoraDescriptionEffectIconSettings();
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
	const USkillDefinition* Skill,
	UImage* ImageWidget)
{
	if (!ImageWidget)
	{
		return;
	}

	const ESkillEffectIconType EffectIconType = ResolveSkillEffectIconType(Skill);
	if (EffectIconType == ESkillEffectIconType::None)
	{
		ImageWidget->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	if (UTexture2D* Image = ResolveConfiguredImage(WorldContextObject, EffectIconType))
	{
		ImageWidget->SetBrushFromTexture(Image, false);
	}

	ImageWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
