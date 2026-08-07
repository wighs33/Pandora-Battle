#include "Definition/Pandora/PandoraDefinition.h"

#include "AbilitySystem/Ability/AOEAttackAbility.h"
#include "AbilitySystem/Ability/AuraAbility.h"
#include "AbilitySystem/Ability/DashAbility.h"
#include "AbilitySystem/Ability/FillShieldAbility.h"
#include "AbilitySystem/Ability/MissileAbility.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/Ability/ProjectileAbility.h"
#include "AbilitySystem/Ability/ShieldAbility.h"
#include "AbilitySystem/Ability/StaticAbility.h"
#include "AbilitySystem/Ability/SummonAbility.h"
#include "AbilitySystem/Ability/TrailAbility.h"
#include "AbilitySystem/EffectActors/EffectAreaBase.h"
#include "Abilities/GameplayAbility.h"
#include "Definition/Item/ItemDefinition.h"

#if WITH_EDITOR
#include "Common/LabGameplayTags.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraDefinition)

namespace
{
	constexpr int32 FixedPandoraMaxLevel = 3;

	TSubclassOf<UGameplayAbility> GetDefaultAbilityClassForSkillDataType(const ESkillDataType SkillDataType)
	{
		switch (SkillDataType)
		{
		case ESkillDataType::Projectile:
			return UProjectileAbility::StaticClass();
		case ESkillDataType::Area:
			return UAOEAttackAbility::StaticClass();
		case ESkillDataType::Dash:
			return UDashAbility::StaticClass();
		case ESkillDataType::Aura:
			return UAuraAbility::StaticClass();
		case ESkillDataType::Trail:
			return UTrailAbility::StaticClass();
		case ESkillDataType::Missile:
			return UMissileAbility::StaticClass();
		case ESkillDataType::Summon:
			return USummonAbility::StaticClass();
		case ESkillDataType::Static:
			return UStaticAbility::StaticClass();
		case ESkillDataType::ShieldBubble:
			return UShieldAbility::StaticClass();
		case ESkillDataType::FillShield:
			return UFillShieldAbility::StaticClass();
		case ESkillDataType::Default:
		default:
			return nullptr;
		}
	}

	TSubclassOf<UGameplayAbility> GetDefaultAbilityClassForSkillDefinition(const USkillDefinition* SkillDefinition)
	{
		if (!SkillDefinition)
		{
			return nullptr;
		}

		return GetDefaultAbilityClassForSkillDataType(SkillDefinition->GetResolvedSkillDataType());
	}

	bool DoesAbilityClassMatchSkillDataType(
		const TSubclassOf<UGameplayAbility> AbilityClass,
		const USkillDefinition* SkillDefinition)
	{
		const TSubclassOf<UGameplayAbility> ExpectedAbilityClass = GetDefaultAbilityClassForSkillDefinition(SkillDefinition);
		return AbilityClass
			&& AbilityClass->IsChildOf(UPdGameplayAbility::StaticClass())
			&& (!ExpectedAbilityClass || AbilityClass->IsChildOf(ExpectedAbilityClass));
	}

	bool IsInputDrivenSkillType(const ESkillType SkillType)
	{
		return SkillType == ESkillType::Instant
			|| SkillType == ESkillType::Press
			|| SkillType == ESkillType::Duration;
	}

	void FilterActiveAbilityClassesBySkillDataType(
		TArray<TSubclassOf<UGameplayAbility>>& InOutAbilityClasses,
		const USkillDefinition* SkillDefinition)
	{
		if (!SkillDefinition || !IsInputDrivenSkillType(SkillDefinition->SkillType))
		{
			return;
		}

		InOutAbilityClasses.RemoveAll([SkillDefinition](const TSubclassOf<UGameplayAbility>& AbilityClass)
		{
			return !DoesAbilityClassMatchSkillDataType(AbilityClass, SkillDefinition);
		});
	}

#if WITH_EDITOR
	void MarkPandoraInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	bool MatchesAnyKnownPandoraType(const UPandoraDefinition& PandoraDefinition)
	{
		const UProjectTagConfig* ProjectTagConfig = UProjectTagConfig::GetDefaultConfig();
		if (!ProjectTagConfig)
		{
			return false;
		}

		TArray<FGameplayTag> PandoraTypeTags;
		ProjectTagConfig->GetPandoraFilterTypeTags(PandoraTypeTags);
		for (const FGameplayTag& PandoraTypeTag : PandoraTypeTags)
		{
			if (PandoraDefinition.MatchesPandoraType(PandoraTypeTag))
			{
				return true;
			}
		}

		return false;
	}

	bool IsKnownWeaponTag(const FGameplayTag WeaponTag)
	{
		if (!WeaponTag.IsValid())
		{
			return false;
		}

		const UProjectTagConfig* ProjectTagConfig = UProjectTagConfig::GetDefaultConfig();
		const FGameplayTag WeaponTypeTag = ProjectTagConfig
			? ProjectTagConfig->GetItemWeaponTypeTag()
			: LabGameplayTags::Item_Weapon;
		return WeaponTypeTag.IsValid()
			&& (WeaponTag.MatchesTag(WeaponTypeTag) || WeaponTypeTag.MatchesTag(WeaponTag));
	}

	void ValidatePandoraShopData(FDataValidationContext& Context, EDataValidationResult& Result, const FShopProductDefinitionData& ShopData)
	{
		if (ShopData.GoldPrice < 0)
		{
			MarkPandoraInvalid(Context, Result, NSLOCTEXT("PandoraDefinition", "InvalidShopGoldPrice", "ShopData.GoldPrice cannot be negative."));
		}
	}

	void ValidatePandoraIdentity(FDataValidationContext& Context, EDataValidationResult& Result, const UPandoraDefinition& PandoraDefinition)
	{
		if (PandoraDefinition.DisplayName.IsEmpty())
		{
			Context.AddWarning(NSLOCTEXT("PandoraDefinition", "MissingDisplayName", "DisplayName is empty. UI will fall back to the asset name in some places."));
		}

		if (!PandoraDefinition.IconTexture)
		{
			Context.AddWarning(NSLOCTEXT("PandoraDefinition", "MissingIconTexture", "IconTexture is not set."));
		}

		if (!PandoraDefinition.IdTag.IsValid())
		{
			Context.AddWarning(NSLOCTEXT(
				"PandoraDefinition",
				"MissingIdTag",
				"IdTag is not set. The asset remains valid as an intentional catalog placeholder and cannot be discovered by tag."));
		}
		else if (!MatchesAnyKnownPandoraType(PandoraDefinition))
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "UnknownPandoraTypeTag", "IdTag does not match a configured Pandora type tag: {0}"),
				FText::FromString(PandoraDefinition.IdTag.ToString())));
		}

		if (PandoraDefinition.Tier < 0)
		{
			MarkPandoraInvalid(Context, Result, NSLOCTEXT("PandoraDefinition", "InvalidTier", "Tier cannot be negative."));
		}
	}

	void ValidatePandoraLevelRules(FDataValidationContext& Context, EDataValidationResult& Result, const UPandoraDefinition& PandoraDefinition)
	{
		if (PandoraDefinition.MaxLevel != FixedPandoraMaxLevel)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "IgnoredMaxLevel", "MaxLevel is currently ignored by runtime code. Pandora max level is fixed to {0}."),
				FText::AsNumber(FixedPandoraMaxLevel)));
		}

		if (PandoraDefinition.PointsRequiredPerLevel.Num() < FixedPandoraMaxLevel)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "MissingPointsRequiredPerLevel", "PointsRequiredPerLevel has {0} entries. Missing levels will cost 1 point at runtime."),
				FText::AsNumber(PandoraDefinition.PointsRequiredPerLevel.Num())));
		}
		else if (PandoraDefinition.PointsRequiredPerLevel.Num() > FixedPandoraMaxLevel)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "ExtraPointsRequiredPerLevel", "PointsRequiredPerLevel has more than {0} entries. Extra entries are ignored by runtime code."),
				FText::AsNumber(FixedPandoraMaxLevel)));
		}

		for (int32 Index = 0; Index < PandoraDefinition.PointsRequiredPerLevel.Num(); ++Index)
		{
			if (PandoraDefinition.PointsRequiredPerLevel[Index] < 1)
			{
				MarkPandoraInvalid(Context, Result, FText::Format(
					NSLOCTEXT("PandoraDefinition", "InvalidPointsRequiredPerLevel", "PointsRequiredPerLevel[{0}] must be at least 1."),
					FText::AsNumber(Index)));
			}
		}
	}

	void ValidatePandoraWeaponCompatibility(FDataValidationContext& Context, EDataValidationResult& Result, const UPandoraDefinition& PandoraDefinition)
	{
		for (const FGameplayTag& WeaponTag : PandoraDefinition.ActivatableWeaponTags)
		{
			if (!WeaponTag.IsValid())
			{
				MarkPandoraInvalid(Context, Result, NSLOCTEXT("PandoraDefinition", "InvalidWeaponCompatibilityTag", "ActivatableWeaponTags contains an invalid GameplayTag."));
				continue;
			}

			if (!IsKnownWeaponTag(WeaponTag))
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("PandoraDefinition", "UnknownWeaponCompatibilityTag", "ActivatableWeaponTags contains a tag that does not match the configured weapon item type: {0}"),
					FText::FromString(WeaponTag.ToString())));
			}
		}
	}

	void ValidatePandoraSkillEntry(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FSkill& SkillEntry,
		const int32 SkillSlotIndex)
	{
		const USkillDefinition* SkillDefinition = SkillEntry.SkillDefinition.Get();
		if (!SkillDefinition)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "MissingSkillDefinition", "Skill[{0}] has no SkillDefinition."),
				FText::AsNumber(SkillSlotIndex)));
			return;
		}

		if (SkillDefinition->GetDisplayName().IsEmpty())
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "SkillMissingDisplayName", "Skill[{0}] uses a SkillDefinition with no display name."),
				FText::AsNumber(SkillSlotIndex)));
		}

		if (SkillDefinition->ShouldShowInAbilitiesBar() && !SkillDefinition->GetIconResource())
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "SkillMissingIcon", "Skill[{0}] is shown in the abilities bar, but its SkillDefinition has no icon."),
				FText::AsNumber(SkillSlotIndex)));
		}

		const bool bInputDrivenSkill = IsInputDrivenSkillType(SkillDefinition->SkillType);
		const TArray<TSubclassOf<UGameplayAbility>>& ExplicitAbilitiesToGrant = SkillDefinition->GetExplicitAbilitiesToGrant();
		for (int32 AbilityIndex = 0; AbilityIndex < ExplicitAbilitiesToGrant.Num(); ++AbilityIndex)
		{
			const TSubclassOf<UGameplayAbility>& AbilityClass = ExplicitAbilitiesToGrant[AbilityIndex];
			if (!AbilityClass)
			{
				MarkPandoraInvalid(Context, Result, FText::Format(
					NSLOCTEXT("PandoraDefinition", "NullExplicitAbility", "Skill[{0}] has a null AbilitiesToGrant entry at index {1}."),
					FText::AsNumber(SkillSlotIndex),
					FText::AsNumber(AbilityIndex)));
				continue;
			}

			if (bInputDrivenSkill && !DoesAbilityClassMatchSkillDataType(AbilityClass, SkillDefinition))
			{
				MarkPandoraInvalid(Context, Result, FText::Format(
					NSLOCTEXT("PandoraDefinition", "MismatchedExplicitAbility", "Skill[{0}] must grant a UPdGameplayAbility class that matches the Skill Data Type: {1}"),
					FText::AsNumber(SkillSlotIndex),
					FText::FromString(GetNameSafe(AbilityClass.Get()))));
			}
		}

		if (bInputDrivenSkill && SkillEntry.GetAbilitiesToGrant().IsEmpty())
		{
			MarkPandoraInvalid(Context, Result, FText::Format(
				NSLOCTEXT("PandoraDefinition", "SkillGrantsNoAbilities", "Skill[{0}] is input-driven but grants no gameplay ability."),
				FText::AsNumber(SkillSlotIndex)));
		}
		else if (!bInputDrivenSkill && SkillDefinition->bShowInAbilitiesBar)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "NonInputSkillShownInBar", "Skill[{0}] is not input-driven but is marked to show in the abilities bar."),
				FText::AsNumber(SkillSlotIndex)));
		}
	}

	void ValidatePandoraSkills(FDataValidationContext& Context, EDataValidationResult& Result, const UPandoraDefinition& PandoraDefinition)
	{
		if (PandoraDefinition.Skill.Num() < FixedPandoraMaxLevel)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "MissingSkillSlots", "Skill has {0} entries. The UI and runtime skill slots expect {1} entries."),
				FText::AsNumber(PandoraDefinition.Skill.Num()),
				FText::AsNumber(FixedPandoraMaxLevel)));
		}
		else if (PandoraDefinition.Skill.Num() > FixedPandoraMaxLevel)
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("PandoraDefinition", "ExtraSkillSlots", "Skill has more than {0} entries. Extra entries are ignored by runtime code."),
				FText::AsNumber(FixedPandoraMaxLevel)));
		}

		const int32 EntriesToValidate = FMath::Min(PandoraDefinition.Skill.Num(), FixedPandoraMaxLevel);
		for (int32 SkillSlotIndex = 0; SkillSlotIndex < EntriesToValidate; ++SkillSlotIndex)
		{
			ValidatePandoraSkillEntry(Context, Result, PandoraDefinition.Skill[SkillSlotIndex], SkillSlotIndex);
		}
	}

	void ValidatePandoraUnlockRules(FDataValidationContext& Context, EDataValidationResult& Result, const UPandoraDefinition& PandoraDefinition)
	{
		TSet<const UPandoraDefinition*> RequiredPandoras;
		for (int32 RuleIndex = 0; RuleIndex < PandoraDefinition.UnlockRules.Num(); ++RuleIndex)
		{
			const FPandoraUnlockRule& UnlockRule = PandoraDefinition.UnlockRules[RuleIndex];
			const UPandoraDefinition* RequiredPandora = UnlockRule.RequiredPandora.Get();
			if (!RequiredPandora)
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("PandoraDefinition", "MissingRequiredPandora", "UnlockRules[{0}] has no RequiredPandora. Runtime code will ignore this rule."),
					FText::AsNumber(RuleIndex)));
				continue;
			}

			if (RequiredPandora == &PandoraDefinition)
			{
				MarkPandoraInvalid(Context, Result, FText::Format(
					NSLOCTEXT("PandoraDefinition", "SelfUnlockRule", "UnlockRules[{0}] references this PandoraDefinition."),
					FText::AsNumber(RuleIndex)));
			}

			if (RequiredPandoras.Contains(RequiredPandora))
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("PandoraDefinition", "DuplicateUnlockRule", "UnlockRules[{0}] duplicates RequiredPandora {1}."),
					FText::AsNumber(RuleIndex),
					RequiredPandora->GetDisplayName()));
			}
			RequiredPandoras.Add(RequiredPandora);

			if (UnlockRule.RequiredLevel < 1)
			{
				MarkPandoraInvalid(Context, Result, FText::Format(
					NSLOCTEXT("PandoraDefinition", "InvalidRequiredLevel", "UnlockRules[{0}].RequiredLevel must be at least 1."),
					FText::AsNumber(RuleIndex)));
			}
			else if (UnlockRule.RequiredLevel > RequiredPandora->GetMaxLevel())
			{
				MarkPandoraInvalid(Context, Result, FText::Format(
					NSLOCTEXT("PandoraDefinition", "ImpossibleRequiredLevel", "UnlockRules[{0}].RequiredLevel is higher than the required Pandora's max level."),
					FText::AsNumber(RuleIndex)));
			}

			for (const FPandoraUnlockRule& RequiredPandoraRule : RequiredPandora->UnlockRules)
			{
				if (RequiredPandoraRule.RequiredPandora.Get() == &PandoraDefinition)
				{
					Context.AddWarning(FText::Format(
						NSLOCTEXT("PandoraDefinition", "DirectUnlockCycle", "UnlockRules[{0}] creates a direct unlock dependency cycle with {1}."),
						FText::AsNumber(RuleIndex),
						RequiredPandora->GetDisplayName()));
					break;
				}
			}
		}
	}
#endif
}

FText FSkill::GetDisplayName() const
{
	return SkillDefinition ? SkillDefinition->GetDisplayName() : FText::GetEmpty();
}

FText FSkill::GetDescription() const
{
	return SkillDefinition ? SkillDefinition->Description : FText::GetEmpty();
}

UObject* FSkill::GetIconResource() const
{
	return SkillDefinition ? SkillDefinition->GetIconResource() : nullptr;
}

bool FSkill::ShouldShowInAbilitiesBar() const
{
	return SkillDefinition && SkillDefinition->bShowInAbilitiesBar;
}

TArray<TSubclassOf<UGameplayAbility>> FSkill::GetAbilitiesToGrant() const
{
	TArray<TSubclassOf<UGameplayAbility>> Result;
	if (SkillDefinition && IsInputDrivenSkillType(SkillDefinition->SkillType))
	{
		const USkillDefinition* SkillDefinitionData = SkillDefinition.Get();
		const auto AddDefaultAbilityClass = [&Result, SkillDefinitionData]()
		{
			if (const TSubclassOf<UGameplayAbility> DefaultAbilityClass = GetDefaultAbilityClassForSkillDefinition(SkillDefinitionData))
			{
				Result.AddUnique(DefaultAbilityClass);
			}
		};

		Result.Append(SkillDefinitionData->GetExplicitAbilitiesToGrant());

		if (Result.IsEmpty())
		{
			AddDefaultAbilityClass();
		}

		FilterActiveAbilityClassesBySkillDataType(Result, SkillDefinitionData);
		if (Result.IsEmpty())
		{
			AddDefaultAbilityClass();
		}

		return Result;
	}

	return Result;
}

UPandoraDefinition::UPandoraDefinition()
{
	NormalizeLevelRules();
}

void UPandoraDefinition::PostLoad()
{
	Super::PostLoad();

	NormalizeLevelRules();
}

FPrimaryAssetId UPandoraDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("PandoraDefinition"), GetFName());
}

#if WITH_EDITOR
void UPandoraDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	NormalizeLevelRules();
}

EDataValidationResult UPandoraDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidatePandoraIdentity(Context, Result, *this);
	ValidatePandoraLevelRules(Context, Result, *this);
	ValidatePandoraWeaponCompatibility(Context, Result, *this);
	ValidatePandoraSkills(Context, Result, *this);
	ValidatePandoraUnlockRules(Context, Result, *this);
	ValidatePandoraShopData(Context, Result, ShopData);

	return Result;
}
#endif

FText UPandoraDefinition::GetDisplayName() const
{
	return !DisplayName.IsEmpty() ? DisplayName : FText::FromName(GetFName());
}

FText UPandoraDefinition::GetDescription() const
{
	return Description;
}

UObject* UPandoraDefinition::GetIconResource() const
{
	return IconTexture.Get();
}

int32 UPandoraDefinition::GetMaxLevel() const
{
	return FixedPandoraMaxLevel;
}

int32 UPandoraDefinition::GetRequiredPointsForLevel(const int32 Level) const
{
	const int32 Index = FMath::Max(Level, 1) - 1;
	return PointsRequiredPerLevel.IsValidIndex(Index) ? FMath::Max(PointsRequiredPerLevel[Index], 1) : 1;
}

void UPandoraDefinition::NormalizeLevelRules()
{
	MaxLevel = FixedPandoraMaxLevel;
	PointsRequiredPerLevel.SetNum(FixedPandoraMaxLevel);
	for (int32& RequiredPoints : PointsRequiredPerLevel)
	{
		RequiredPoints = FMath::Max(RequiredPoints, 1);
	}
}

bool UPandoraDefinition::MatchesPandoraType(const FGameplayTag PandoraTypeTag) const
{
	return IdTag.IsValid() && PandoraTypeTag.IsValid() && IdTag.MatchesTag(PandoraTypeTag);
}

int32 UPandoraDefinition::GetFixedMaxLevel()
{
	return FixedPandoraMaxLevel;
}

int32 UPandoraDefinition::GetRequiredLevelForSkillSlot(const int32 SkillSlotIndex)
{
	switch (SkillSlotIndex)
	{
	case 0:
		return 1;
	case 1:
		return 2;
	case 2:
		return 3;
	default:
		return FixedPandoraMaxLevel + 1;
	}
}

bool UPandoraDefinition::IsSkillSlotUnlocked(const int32 SkillSlotIndex, const int32 PandoraLevel) const
{
	const int32 RequiredLevel = GetRequiredLevelForSkillSlot(SkillSlotIndex);
	return Skill.IsValidIndex(SkillSlotIndex)
		&& RequiredLevel >= 1
		&& RequiredLevel <= GetMaxLevel()
		&& FMath::Clamp(PandoraLevel, 0, GetMaxLevel()) >= RequiredLevel;
}

bool UPandoraDefinition::IsCompatibleWithWeaponTag(const FGameplayTag WeaponTag) const
{
	if (ActivatableWeaponTags.IsEmpty())
	{
		return true;
	}

	if (!WeaponTag.IsValid())
	{
		return false;
	}

	for (const FGameplayTag& ActivatableWeaponTag : ActivatableWeaponTags)
	{
		if (!ActivatableWeaponTag.IsValid())
		{
			continue;
		}

		if (WeaponTag.MatchesTag(ActivatableWeaponTag) || ActivatableWeaponTag.MatchesTag(WeaponTag))
		{
			return true;
		}
	}

	return false;
}

bool UPandoraDefinition::IsCompatibleWithWeaponDefinition(const UItemDefinition* WeaponDefinition) const
{
	return IsCompatibleWithWeaponTag(WeaponDefinition ? WeaponDefinition->IdTag : FGameplayTag());
}
