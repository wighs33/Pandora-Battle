#include "PandoraDefinition.h"

#include "AbilitySystem/Data/PdStatusEffectDataAsset.h"
#include "AbilitySystem/EffectActors/EffectAreaBase.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "Item/ItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraDefinition)

DEFINE_LOG_CATEGORY(PandoraDefinitionLog);

FName FSkill::GetSkillName() const
{
	return SkillDefinition && !SkillDefinition->Name.IsNone() ? SkillDefinition->Name : NAME_None;
}

FText FSkill::GetDisplayName() const
{
	return SkillDefinition ? SkillDefinition->GetDisplayName() : FText::GetEmpty();
}

FText FSkill::GetDescription() const
{
	return SkillDefinition ? SkillDefinition->Description : FText::GetEmpty();
}

FText FSkill::GetDescriptionForLevel(const int32 Level) const
{
	return SkillDefinition ? SkillDefinition->GetDescriptionForLevel(Level) : FText::GetEmpty();
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
	return GetAbilitiesToGrantForLevel(1);
}

TArray<TSubclassOf<UGameplayAbility>> FSkill::GetAbilitiesToGrantForLevel(const int32 Level) const
{
	TArray<TSubclassOf<UGameplayAbility>> Result;
	const int32 EffectiveLevel = FMath::Max(Level, 1);

	for (const FPandoraSkillLevelUnlock& LevelUnlock : LevelUnlocks)
	{
		if (EffectiveLevel < FMath::Max(LevelUnlock.RequiredLevel, 1))
		{
			continue;
		}

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : LevelUnlock.AbilitiesToGrant)
		{
			if (AbilityClass)
			{
				Result.AddUnique(AbilityClass);
			}
		}
	}

	if (Result.IsEmpty() && LevelUnlocks.IsEmpty() && SkillDefinition && SkillDefinition->HasLegacyLevelUnlocks())
	{
		return SkillDefinition->GetLegacyAbilitiesToGrantForLevel(Level);
	}

	return Result;
}

TArray<TSubclassOf<UGameplayEffect>> FSkill::GetEffectsToApply() const
{
	return GetEffectsToApplyForLevel(1);
}

TArray<TSubclassOf<UGameplayEffect>> FSkill::GetEffectsToApplyForLevel(const int32 Level) const
{
	TArray<TSubclassOf<UGameplayEffect>> Result;
	const int32 EffectiveLevel = FMath::Max(Level, 1);

	for (const FPandoraSkillLevelUnlock& LevelUnlock : LevelUnlocks)
	{
		if (EffectiveLevel < FMath::Max(LevelUnlock.RequiredLevel, 1))
		{
			continue;
		}

		for (const TSubclassOf<UGameplayEffect>& EffectClass : LevelUnlock.EffectsToApply)
		{
			if (EffectClass)
			{
				Result.AddUnique(EffectClass);
			}
		}
	}

	if (Result.IsEmpty() && LevelUnlocks.IsEmpty() && SkillDefinition && SkillDefinition->HasLegacyLevelUnlocks())
	{
		return SkillDefinition->GetLegacyEffectsToApplyForLevel(Level);
	}

	return Result;
}

TArray<TObjectPtr<UPdStatusEffectDataAsset>> FSkill::GetStatusEffectsToUnlock() const
{
	return GetStatusEffectsToUnlockForLevel(1);
}

TArray<TObjectPtr<UPdStatusEffectDataAsset>> FSkill::GetStatusEffectsToUnlockForLevel(const int32 Level) const
{
	TArray<TObjectPtr<UPdStatusEffectDataAsset>> Result;
	const int32 EffectiveLevel = FMath::Max(Level, 1);

	for (const FPandoraSkillLevelUnlock& LevelUnlock : LevelUnlocks)
	{
		if (EffectiveLevel < FMath::Max(LevelUnlock.RequiredLevel, 1))
		{
			continue;
		}

		for (const TObjectPtr<UPdStatusEffectDataAsset>& StatusEffectDataAsset : LevelUnlock.StatusEffectsToUnlock)
		{
			if (StatusEffectDataAsset)
			{
				Result.AddUnique(StatusEffectDataAsset);
			}
		}
	}

	if (Result.IsEmpty() && LevelUnlocks.IsEmpty() && SkillDefinition && SkillDefinition->HasLegacyLevelUnlocks())
	{
		return SkillDefinition->GetLegacyStatusEffectsToUnlockForLevel(Level);
	}

	return Result;
}

TArray<FProjectileImpactEffectAreaSpawnConfig> FSkill::GetProjectileImpactEffectAreasForLevel(const int32 Level) const
{
	TArray<FProjectileImpactEffectAreaSpawnConfig> Result;
	const int32 EffectiveLevel = FMath::Max(Level, 1);

	for (const FPandoraSkillLevelUnlock& LevelUnlock : LevelUnlocks)
	{
		if (EffectiveLevel < FMath::Max(LevelUnlock.RequiredLevel, 1))
		{
			continue;
		}

		for (const FProjectileImpactEffectAreaSpawnConfig& ImpactEffectArea : LevelUnlock.ProjectileImpactEffectAreas)
		{
			if (ImpactEffectArea.EffectAreaClass)
			{
				Result.Add(ImpactEffectArea);
			}
		}
	}

	if (Result.IsEmpty() && LevelUnlocks.IsEmpty() && SkillDefinition && SkillDefinition->HasLegacyLevelUnlocks())
	{
		return SkillDefinition->GetLegacyProjectileImpactEffectAreasForLevel(Level);
	}

	return Result;
}

int32 FSkill::GetMaxLevel() const
{
	int32 MaxUnlockLevel = 1;
	for (const FPandoraSkillLevelUnlock& LevelUnlock : LevelUnlocks)
	{
		MaxUnlockLevel = FMath::Max(MaxUnlockLevel, FMath::Max(LevelUnlock.RequiredLevel, 1));
	}

	return MaxUnlockLevel;
}

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

FText UPandoraDefinition::GetDescriptionForLevel(const int32 Level) const
{
	if (Level <= 0)
	{
		return FText::GetEmpty();
	}

	const int32 DescriptionIndex = Level - 1;
	return DescriptionPerLevel.IsValidIndex(DescriptionIndex)
		? DescriptionPerLevel[DescriptionIndex]
		: FText::GetEmpty();
}

UObject* UPandoraDefinition::GetIconResource() const
{
	if (IconOverride)
	{
		return IconOverride.Get();
	}

	return IconTexture.Get();
}

UObject* UPandoraDefinition::GetActiveIconResource() const
{
	return ActiveIconOverride ? ActiveIconOverride.Get() : GetIconResource();
}

int32 UPandoraDefinition::GetMaxLevel() const
{
	return FMath::Max(MaxLevel, 1);
}

int32 UPandoraDefinition::GetRequiredPointsForLevel(const int32 Level) const
{
	const int32 Index = FMath::Max(Level, 1) - 1;
	return PointsRequiredPerLevel.IsValidIndex(Index) ? FMath::Max(PointsRequiredPerLevel[Index], 1) : 1;
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
