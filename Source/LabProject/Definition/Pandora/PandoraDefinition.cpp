#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Item/ItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraDefinition)

FPrimaryAssetId UPandoraDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("PandoraDefinition"), GetFName());
}

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
	return MaxLevel;
}

int32 UPandoraDefinition::GetRequiredPointsForLevel(const int32 Level) const
{
	const int32 Index = FMath::Max(Level, 1) - 1;
	return Index < MaxLevel && PointsRequiredPerLevel.IsValidIndex(Index)
		? FMath::Max(PointsRequiredPerLevel[Index], 1)
		: 1;
}

bool UPandoraDefinition::MatchesPandoraType(const FGameplayTag PandoraTypeTag) const
{
	return IdTag.IsValid() && PandoraTypeTag.IsValid() && IdTag.MatchesTag(PandoraTypeTag);
}

const USkillDefinition* UPandoraDefinition::GetSkillDefinition(const int32 SkillSlotIndex) const
{
	return Skills.IsValidIndex(SkillSlotIndex) ? Skills[SkillSlotIndex].Get() : nullptr;
}

int32 UPandoraDefinition::GetFixedMaxLevel()
{
	return MaxLevel;
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
		return MaxLevel + 1;
	}
}

bool UPandoraDefinition::IsSkillSlotUnlocked(const int32 SkillSlotIndex, const int32 PandoraLevel) const
{
	const int32 RequiredLevel = GetRequiredLevelForSkillSlot(SkillSlotIndex);
	return Skills.IsValidIndex(SkillSlotIndex)
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
