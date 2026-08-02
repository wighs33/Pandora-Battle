#include "Definition/Settings/GameSettingDefinition.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Definition/Online/AchievementDefinition.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingDefinition)

UGameSettingDefinition::UGameSettingDefinition()
{
	AchievementData = TSoftObjectPtr<UAchievementDefinition>(
		FSoftObjectPath(TEXT("/Game/Data/DA_Achievement.DA_Achievement")));

	StatusEffectDataAssets =
	{
		TSoftObjectPtr<UStatusEffectDefinition>(
			FSoftObjectPath(TEXT("/Game/StatusEffects/DA_StatusEffect_Burn.DA_StatusEffect_Burn"))),
		TSoftObjectPtr<UStatusEffectDefinition>(
			FSoftObjectPath(TEXT("/Game/StatusEffects/DA_StatusEffect_Frostbite.DA_StatusEffect_Frostbite"))),
		TSoftObjectPtr<UStatusEffectDefinition>(
			FSoftObjectPath(TEXT("/Game/StatusEffects/DA_StatusEffect_ElectricShock.DA_StatusEffect_ElectricShock")))
	};

	FGameplayTagContainer RemoveOnDeathEffectTags;
	RemoveOnDeathEffectTags.AddTag(LabGameplayTags::Effect_Policy_RemoveOnDeath);
	RemoveOnDeathEffectTags.AddTag(LabGameplayTags::Cooldown);
	RemoveOnDeathPolicy.EffectTagQuery =
		FGameplayTagQuery::MakeQuery_MatchAnyTags(RemoveOnDeathEffectTags);
	RemoveOnDeathPolicy.OwningTagQuery =
		FGameplayTagQuery::MakeQuery_MatchTag(LabGameplayTags::Cooldown);

	// RemoveOnRespawn is the primary policy for new effects. The explicit status tags
	// and Debuff root preserve legacy content without matching unrelated Status.* stat effects.
	FGameplayTagContainer RemoveOnRespawnEffectTags;
	RemoveOnRespawnEffectTags.AddTag(LabGameplayTags::Effect_Policy_RemoveOnRespawn);
	RemoveOnRespawnEffectTags.AddTag(LabGameplayTags::Debuff);
	RemoveOnRespawnEffectTags.AddTag(LabGameplayTags::Status_Burning);
	RemoveOnRespawnEffectTags.AddTag(LabGameplayTags::Status_Frostbite);
	RemoveOnRespawnEffectTags.AddTag(LabGameplayTags::Status_ElectricShock);
	RemoveOnRespawnPolicy.EffectTagQuery =
		FGameplayTagQuery::MakeQuery_MatchAnyTags(RemoveOnRespawnEffectTags);

	FGameplayTagContainer RemoveOnRespawnOwningTags;
	RemoveOnRespawnOwningTags.AddTag(LabGameplayTags::Debuff);
	RemoveOnRespawnOwningTags.AddTag(LabGameplayTags::Status_Burning);
	RemoveOnRespawnOwningTags.AddTag(LabGameplayTags::Status_Frostbite);
	RemoveOnRespawnOwningTags.AddTag(LabGameplayTags::Status_ElectricShock);
	RemoveOnRespawnPolicy.OwningTagQuery =
		FGameplayTagQuery::MakeQuery_MatchAnyTags(RemoveOnRespawnOwningTags);
	RemoveOnRespawnPolicy.LooseTagQuery =
		FGameplayTagQuery::MakeQuery_MatchAnyTags(RemoveOnRespawnOwningTags);
	RemoveOnRespawnPolicy.GameplayCuesToRemove.AddTag(LabGameplayTags::GameplayCue_Burning);
	RemoveOnRespawnPolicy.GameplayCuesToRemove.AddTag(LabGameplayTags::GameplayCue_Frozen);
	RemoveOnRespawnPolicy.GameplayCuesToRemove.AddTag(LabGameplayTags::GameplayCue_Shocked);

	RemoveOnPandoraResetPolicy.EffectTagQuery =
		FGameplayTagQuery::MakeQuery_MatchTag(LabGameplayTags::Effect_Policy_RemoveOnPandoraReset);

	const auto AddCoreAttributeMapping =
		[this](const FGameplayTag& StatTag, const FGameplayAttribute& Attribute)
	{
		FPdAttributeTagMapping& Mapping =
			CoreAttributeConfig.AttributeMappings.AddDefaulted_GetRef();
		Mapping.StatTag = StatTag;
		Mapping.Attribute = Attribute;
	};

	AddCoreAttributeMapping(LabGameplayTags::Status_Offense_StrengthLevel, UBasicAttributeSet::GetStrengthLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Offense_IntelligenceLevel, UBasicAttributeSet::GetIntelligenceLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Offense_CriticalLevel, UBasicAttributeSet::GetCriticalLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Defense_ArmorLevel, UBasicAttributeSet::GetArmorLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Defense_RecoveryLevel, UBasicAttributeSet::GetRecoveryLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Defense_MaxShieldLevel, UBasicAttributeSet::GetMaxShieldLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resistance_FrostbiteLevel, UBasicAttributeSet::GetFrostbiteLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resistance_BurnLevel, UBasicAttributeSet::GetBurnLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resistance_ElectricShockLevel, UBasicAttributeSet::GetElectricShockLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_PandoraForce_FirstPandoraLevel, UBasicAttributeSet::GetFirstPandoraLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_PandoraForce_SecondPandoraLevel, UBasicAttributeSet::GetSecondPandoraLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_PandoraForce_ThirdPandoraLevel, UBasicAttributeSet::GetThirdPandoraLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resource_MaxHealthLevel, UBasicAttributeSet::GetMaxHealthLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resource_MaxManaLevel, UBasicAttributeSet::GetMaxManaLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resource_MaxStaminaLevel, UBasicAttributeSet::GetMaxStaminaLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Agility_AttackSpeedLevel, UBasicAttributeSet::GetAttackSpeedLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Agility_MovementSpeedLevel, UBasicAttributeSet::GetMovementSpeedLevelAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Agility_ArcaneLevel, UBasicAttributeSet::GetArcaneLevelAttribute());

	AddCoreAttributeMapping(LabGameplayTags::Status_Defense_MaxShield, UBasicAttributeSet::GetMaxShieldAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Defense_MaxShieldIncreasePercent, UBasicAttributeSet::GetMaxShieldIncreasePercentAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Offense_Critical, UBasicAttributeSet::GetCriticalAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Agility_Arcane, UBasicAttributeSet::GetArcaneAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resource_MaxHealth, UBasicAttributeSet::GetMaxHealthAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resource_MaxMana, UBasicAttributeSet::GetMaxManaAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resource_MaxStamina, UBasicAttributeSet::GetMaxStaminaAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resource_MaxHealthIncreasePercent, UBasicAttributeSet::GetMaxHealthIncreasePercentAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resource_MaxManaIncreasePercent, UBasicAttributeSet::GetMaxManaIncreasePercentAttribute());
	AddCoreAttributeMapping(LabGameplayTags::Status_Resource_MaxStaminaIncreasePercent, UBasicAttributeSet::GetMaxStaminaIncreasePercentAttribute());
}

FPrimaryAssetId UGameSettingDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("GameSetting"), GetFName());
}

void UGameSettingDefinition::GetRuntimePreloadAssetPaths(
	TArray<FSoftObjectPath>& OutAssetPaths) const
{
	TSet<FSoftObjectPath> UniquePaths;
	const auto AddPath =
		[&UniquePaths](const FSoftObjectPath& AssetPath)
		{
			if (AssetPath.IsValid() && !AssetPath.IsNull())
			{
				UniquePaths.Add(AssetPath);
			}
		};

	AddPath(MouseCursorTexture.ToSoftObjectPath());
	AddPath(AchievementData.ToSoftObjectPath());
	for (const TSoftObjectPtr<UStatusEffectDefinition>& StatusEffectDataAsset :
		StatusEffectDataAssets)
	{
		AddPath(StatusEffectDataAsset.ToSoftObjectPath());
	}

	AddPath(SoundEnabledButtonTexture.ToSoftObjectPath());
	AddPath(SoundMutedButtonTexture.ToSoftObjectPath());
	AddPath(StartupBgm.ToSoftObjectPath());
	AddPath(LobbyBgm.ToSoftObjectPath());
	AddPath(RoomListBgm.ToSoftObjectPath());
	AddPath(ShopBgm.ToSoftObjectPath());
	AddPath(GuideBgm.ToSoftObjectPath());
	AddPath(TrainingRoomBgm.ToSoftObjectPath());
	AddPath(GameplayBgm.ToSoftObjectPath());

	OutAssetPaths = UniquePaths.Array();
	OutAssetPaths.Sort(
		[](const FSoftObjectPath& Left, const FSoftObjectPath& Right)
		{
			return Left.ToString() < Right.ToString();
		});
}

#if WITH_EDITOR
EDataValidationResult UGameSettingDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	if (!CoreAttributeConfig.HasAnyData())
	{
		Context.AddError(NSLOCTEXT(
			"GameSettingDefinition",
			"MissingCoreAttributeConfig",
			"CoreAttributeConfig must contain the shared tag-to-attribute mappings."));
		Result = EDataValidationResult::Invalid;
		return Result;
	}

	TSet<FGameplayTag> SeenStatTags;
	for (const FPdAttributeTagMapping& Mapping : CoreAttributeConfig.AttributeMappings)
	{
		if (!Mapping.IsValid())
		{
			Context.AddError(NSLOCTEXT(
				"GameSettingDefinition",
				"InvalidCoreAttributeMapping",
				"CoreAttributeConfig contains an invalid stat tag or gameplay attribute."));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		if (SeenStatTags.Contains(Mapping.StatTag))
		{
			Context.AddError(FText::Format(
				NSLOCTEXT(
					"GameSettingDefinition",
					"DuplicateCoreAttributeMapping",
					"CoreAttributeConfig contains duplicate mapping for tag '{0}'."),
				FText::FromName(Mapping.StatTag.GetTagName())));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		SeenStatTags.Add(Mapping.StatTag);
	}

	return Result;
}
#endif
