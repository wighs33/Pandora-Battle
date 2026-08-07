#include "BasicAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/LevelingComponent.h"
#include "UI/KillLogTypes.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(BasicAttributeSet)

namespace
{
	constexpr float MaxInvestedStatLevel = 100.f;
	constexpr float MaxPercentEffectValue = 100.f;

	bool RollPercentChance(float PercentChance)
	{
		const float ClampedPercentChance = FMath::Clamp(PercentChance, 0.f, 100.f);
		return ClampedPercentChance >= 100.f
			|| (ClampedPercentChance > 0.f && FMath::FRandRange(0.f, 100.f) < ClampedPercentChance);
	}

	float CalculateCriticalDamageMultiplier(float Critical)
	{
		return 2.f + FMath::Max(Critical, 0.f) * 0.01f;
	}

	float ClampResourceAttribute(float Value, float MaxValue)
	{
		return FMath::Clamp(Value, 0.f, FMath::Max(MaxValue, 0.f));
	}

	float ClampInvestedStatLevel(float Value)
	{
		return FMath::Clamp(Value, 0.f, MaxInvestedStatLevel);
	}

	float ClampStatValue(float Value)
	{
		return FMath::Max(Value, 0.f);
	}

	float ClampPercentEffectValue(float Value)
	{
		return FMath::Clamp(Value, 0.f, MaxPercentEffectValue);
	}

	bool IsInvestedStatLevelAttribute(const FGameplayAttribute& Attribute)
	{
		return Attribute == UBasicAttributeSet::GetStrengthLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetIntelligenceLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetArcaneLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetArmorLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetRecoveryLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetFrostbiteLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetBurnLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetElectricShockLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetFirstPandoraLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetSecondPandoraLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetThirdPandoraLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetMaxHealthLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetMaxShieldLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetMaxManaLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetMaxStaminaLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetAttackSpeedLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetMovementSpeedLevelAttribute()
			|| Attribute == UBasicAttributeSet::GetCriticalLevelAttribute();
	}

	bool IsStatValueAttribute(const FGameplayAttribute& Attribute)
	{
		return Attribute == UBasicAttributeSet::GetStrengthAttribute()
			|| Attribute == UBasicAttributeSet::GetIntelligenceAttribute()
			|| Attribute == UBasicAttributeSet::GetArcaneAttribute()
			|| Attribute == UBasicAttributeSet::GetArmorAttribute()
			|| Attribute == UBasicAttributeSet::GetRecoveryAttribute()
			|| Attribute == UBasicAttributeSet::GetFrostbiteAttribute()
			|| Attribute == UBasicAttributeSet::GetBurnAttribute()
			|| Attribute == UBasicAttributeSet::GetElectricShockAttribute()
			|| Attribute == UBasicAttributeSet::GetFirstPandoraAttribute()
			|| Attribute == UBasicAttributeSet::GetSecondPandoraAttribute()
			|| Attribute == UBasicAttributeSet::GetThirdPandoraAttribute()
			|| Attribute == UBasicAttributeSet::GetMaxHealthIncreasePercentAttribute()
			|| Attribute == UBasicAttributeSet::GetMaxShieldIncreasePercentAttribute()
			|| Attribute == UBasicAttributeSet::GetMaxManaIncreasePercentAttribute()
			|| Attribute == UBasicAttributeSet::GetMaxStaminaIncreasePercentAttribute()
			|| Attribute == UBasicAttributeSet::GetAttackSpeedAttribute()
			|| Attribute == UBasicAttributeSet::GetMovementSpeedAttribute()
			|| Attribute == UBasicAttributeSet::GetCriticalAttribute();
	}

	bool ClampInvestedStatLevelAttribute(const FGameplayAttribute& Attribute, UBasicAttributeSet* AttributeSet)
	{
		if (!AttributeSet)
		{
			return false;
		}

		if (Attribute == UBasicAttributeSet::GetStrengthLevelAttribute()) { AttributeSet->SetStrengthLevel(ClampInvestedStatLevel(AttributeSet->GetStrengthLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetIntelligenceLevelAttribute()) { AttributeSet->SetIntelligenceLevel(ClampInvestedStatLevel(AttributeSet->GetIntelligenceLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetArcaneLevelAttribute()) { AttributeSet->SetArcaneLevel(ClampInvestedStatLevel(AttributeSet->GetArcaneLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetArmorLevelAttribute()) { AttributeSet->SetArmorLevel(ClampInvestedStatLevel(AttributeSet->GetArmorLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetRecoveryLevelAttribute()) { AttributeSet->SetRecoveryLevel(ClampInvestedStatLevel(AttributeSet->GetRecoveryLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetFrostbiteLevelAttribute()) { AttributeSet->SetFrostbiteLevel(ClampInvestedStatLevel(AttributeSet->GetFrostbiteLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetBurnLevelAttribute()) { AttributeSet->SetBurnLevel(ClampInvestedStatLevel(AttributeSet->GetBurnLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetElectricShockLevelAttribute()) { AttributeSet->SetElectricShockLevel(ClampInvestedStatLevel(AttributeSet->GetElectricShockLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetFirstPandoraLevelAttribute()) { AttributeSet->SetFirstPandoraLevel(ClampInvestedStatLevel(AttributeSet->GetFirstPandoraLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetSecondPandoraLevelAttribute()) { AttributeSet->SetSecondPandoraLevel(ClampInvestedStatLevel(AttributeSet->GetSecondPandoraLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetThirdPandoraLevelAttribute()) { AttributeSet->SetThirdPandoraLevel(ClampInvestedStatLevel(AttributeSet->GetThirdPandoraLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetMaxHealthLevelAttribute()) { AttributeSet->SetMaxHealthLevel(ClampInvestedStatLevel(AttributeSet->GetMaxHealthLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetMaxShieldLevelAttribute()) { AttributeSet->SetMaxShieldLevel(ClampInvestedStatLevel(AttributeSet->GetMaxShieldLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetMaxManaLevelAttribute()) { AttributeSet->SetMaxManaLevel(ClampInvestedStatLevel(AttributeSet->GetMaxManaLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetMaxStaminaLevelAttribute()) { AttributeSet->SetMaxStaminaLevel(ClampInvestedStatLevel(AttributeSet->GetMaxStaminaLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetAttackSpeedLevelAttribute()) { AttributeSet->SetAttackSpeedLevel(ClampInvestedStatLevel(AttributeSet->GetAttackSpeedLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetMovementSpeedLevelAttribute()) { AttributeSet->SetMovementSpeedLevel(ClampInvestedStatLevel(AttributeSet->GetMovementSpeedLevel())); return true; }
		if (Attribute == UBasicAttributeSet::GetCriticalLevelAttribute()) { AttributeSet->SetCriticalLevel(ClampInvestedStatLevel(AttributeSet->GetCriticalLevel())); return true; }

		return false;
	}

	bool ClampStatValueAttribute(const FGameplayAttribute& Attribute, UBasicAttributeSet* AttributeSet)
	{
		if (!AttributeSet)
		{
			return false;
		}

		if (Attribute == UBasicAttributeSet::GetStrengthAttribute()) { AttributeSet->SetStrength(ClampStatValue(AttributeSet->GetStrength())); return true; }
		if (Attribute == UBasicAttributeSet::GetIntelligenceAttribute()) { AttributeSet->SetIntelligence(ClampStatValue(AttributeSet->GetIntelligence())); return true; }
		if (Attribute == UBasicAttributeSet::GetArcaneAttribute()) { AttributeSet->SetArcane(ClampStatValue(AttributeSet->GetArcane())); return true; }
		if (Attribute == UBasicAttributeSet::GetArmorAttribute()) { AttributeSet->SetArmor(ClampStatValue(AttributeSet->GetArmor())); return true; }
		if (Attribute == UBasicAttributeSet::GetRecoveryAttribute()) { AttributeSet->SetRecovery(ClampStatValue(AttributeSet->GetRecovery())); return true; }
		if (Attribute == UBasicAttributeSet::GetFrostbiteAttribute()) { AttributeSet->SetFrostbite(ClampStatValue(AttributeSet->GetFrostbite())); return true; }
		if (Attribute == UBasicAttributeSet::GetBurnAttribute()) { AttributeSet->SetBurn(ClampStatValue(AttributeSet->GetBurn())); return true; }
		if (Attribute == UBasicAttributeSet::GetElectricShockAttribute()) { AttributeSet->SetElectricShock(ClampStatValue(AttributeSet->GetElectricShock())); return true; }
		if (Attribute == UBasicAttributeSet::GetFirstPandoraAttribute()) { AttributeSet->SetFirstPandora(ClampStatValue(AttributeSet->GetFirstPandora())); return true; }
		if (Attribute == UBasicAttributeSet::GetSecondPandoraAttribute()) { AttributeSet->SetSecondPandora(ClampStatValue(AttributeSet->GetSecondPandora())); return true; }
		if (Attribute == UBasicAttributeSet::GetThirdPandoraAttribute()) { AttributeSet->SetThirdPandora(ClampStatValue(AttributeSet->GetThirdPandora())); return true; }
		if (Attribute == UBasicAttributeSet::GetMaxHealthIncreasePercentAttribute()) { AttributeSet->SetMaxHealthIncreasePercent(ClampStatValue(AttributeSet->GetMaxHealthIncreasePercent())); return true; }
		if (Attribute == UBasicAttributeSet::GetMaxShieldIncreasePercentAttribute()) { AttributeSet->SetMaxShieldIncreasePercent(ClampStatValue(AttributeSet->GetMaxShieldIncreasePercent())); return true; }
		if (Attribute == UBasicAttributeSet::GetMaxManaIncreasePercentAttribute()) { AttributeSet->SetMaxManaIncreasePercent(ClampStatValue(AttributeSet->GetMaxManaIncreasePercent())); return true; }
		if (Attribute == UBasicAttributeSet::GetMaxStaminaIncreasePercentAttribute()) { AttributeSet->SetMaxStaminaIncreasePercent(ClampStatValue(AttributeSet->GetMaxStaminaIncreasePercent())); return true; }
		if (Attribute == UBasicAttributeSet::GetAttackSpeedAttribute()) { AttributeSet->SetAttackSpeed(ClampStatValue(AttributeSet->GetAttackSpeed())); return true; }
		if (Attribute == UBasicAttributeSet::GetMovementSpeedAttribute()) { AttributeSet->SetMovementSpeed(ClampStatValue(AttributeSet->GetMovementSpeed())); return true; }
		if (Attribute == UBasicAttributeSet::GetCriticalAttribute()) { AttributeSet->SetCritical(ClampStatValue(AttributeSet->GetCritical())); return true; }

		return false;
	}

	float CalculateFinalStrengthDamage(UBasicAttributeSet* AttributeSet)
	{
		if (!AttributeSet)
		{
			return 0.f;
		}

		UAbilitySystemComponent* ASC = AttributeSet->GetOwningAbilitySystemComponent();
		ACharacterBase* Character = ASC ? Cast<ACharacterBase>(ASC->GetAvatarActor()) : nullptr;
		UCombatComponent* CombatComponent = Character ? Character->GetCombatComponent() : nullptr;
		return CombatComponent ? CombatComponent->GetStrengthAdjustedWeaponDamageMagnitude(AttributeSet->GetStrength()) : 0.f;
	}

	float CalculateFinalArmor(const float TargetArmor, const float TargetFinalStrength)
	{
		return FMath::Max(TargetFinalStrength, 0.f) * ClampPercentEffectValue(TargetArmor) * 0.01f;
	}

	float CalculateArmorMitigatedDamage(const float IncomingDamageAmount, const float TargetArmor, const float TargetFinalStrength)
	{
		const float DamageReduction = CalculateFinalArmor(TargetArmor, TargetFinalStrength);
		return FMath::Max(FMath::Max(IncomingDamageAmount, 0.f) - DamageReduction, 0.f);
	}

	float CalculateStatusResistanceMitigatedDamage(const float IncomingDamageAmount, const float TargetStatusResistance)
	{
		const float DamageMultiplier = 1.f - (ClampPercentEffectValue(TargetStatusResistance) * 0.01f);
		return FMath::Max(IncomingDamageAmount, 0.f) * DamageMultiplier;
	}

	bool EffectSpecHasAssetTag(const FGameplayEffectSpec& EffectSpec, const FGameplayTag& AssetTag)
	{
		return AssetTag.IsValid()
			&& EffectSpec.Def
			&& EffectSpec.Def->GetAssetTags().HasTag(AssetTag);
	}

	bool EffectSpecHasStatusTag(const FGameplayEffectSpec& EffectSpec, const FGameplayTag& StatusTag)
	{
		if (!StatusTag.IsValid())
		{
			return false;
		}

		return EffectSpec.DynamicGrantedTags.HasTag(StatusTag)
			|| EffectSpec.GetDynamicAssetTags().HasTag(StatusTag)
			|| (EffectSpec.Def && EffectSpec.Def->GetGrantedTags().HasTag(StatusTag))
			|| (EffectSpec.Def && EffectSpec.Def->GetAssetTags().HasTag(StatusTag));
	}

	bool IsBurningStatusDamageEffectSpec(const FGameplayEffectSpec& EffectSpec)
	{
		return EffectSpecHasStatusTag(EffectSpec, LabGameplayTags::Status_Burning);
	}

	bool IsFrozenStatusDamageEffectSpec(const FGameplayEffectSpec& EffectSpec)
	{
		return EffectSpecHasStatusTag(EffectSpec, LabGameplayTags::Status_Frostbite);
	}

	bool IsElectricShockStatusDamageEffectSpec(const FGameplayEffectSpec& EffectSpec)
	{
		return EffectSpecHasStatusTag(EffectSpec, LabGameplayTags::Status_ElectricShock);
	}

	bool IsStatusDamageEffectSpec(const FGameplayEffectSpec& EffectSpec)
	{
		return IsBurningStatusDamageEffectSpec(EffectSpec)
			|| IsFrozenStatusDamageEffectSpec(EffectSpec)
			|| IsElectricShockStatusDamageEffectSpec(EffectSpec);
	}

	float ResolveStatusResistance(
		const UBasicAttributeSet* AttributeSet,
		const bool bBurningStatusDamage,
		const bool bFrozenStatusDamage,
		const bool bElectricShockStatusDamage)
	{
		if (!AttributeSet)
		{
			return 0.f;
		}

		if (bBurningStatusDamage)
		{
			return AttributeSet->GetBurnLevel();
		}

		if (bFrozenStatusDamage)
		{
			return AttributeSet->GetFrostbiteLevel();
		}

		return bElectricShockStatusDamage ? AttributeSet->GetElectricShockLevel() : 0.f;
	}

	bool TryActivateAbilityByTag(UAbilitySystemComponent* ASC, const FGameplayTag& AbilityTag)
	{
		if (!ASC || !AbilityTag.IsValid())
		{

			return false;
		}

		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(AbilityTag);
		return ASC->TryActivateAbilitiesByTag(AbilityTags, true);
	}

	void TryActivateHitReactionAbility(UAbilitySystemComponent* ASC)
	{
		if (TryActivateAbilityByTag(ASC, LabGameplayTags::GameplayAbility_HitReaction))
		{
			return;
		}

		TryActivateAbilityByTag(ASC, LabGameplayTags::Action_HitReact);
	}

	void TryActivateDeathAbility(UAbilitySystemComponent* ASC)
	{
		if (!ASC)
		{
			return;
		}

		const int32 DeadTagCount =
			ASC->GetTagCount(LabGameplayTags::State_Dead);
		if (DeadTagCount > 0)
		{
			return;
		}

		TryActivateAbilityByTag(
			ASC,
			LabGameplayTags::GameplayAbility_Death);
	}

	APlayerState* ResolvePlayerStateFromActor(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (APlayerState* PlayerState = Cast<APlayerState>(Actor))
		{
			return PlayerState;
		}

		if (APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn->GetPlayerState();
		}

		if (AController* Controller = Cast<AController>(Actor))
		{
			return Controller->PlayerState;
		}

		if (APawn* InstigatorPawn = Actor->GetInstigator())
		{
			if (APlayerState* PlayerState = InstigatorPawn->GetPlayerState())
			{
				return PlayerState;
			}
		}

		if (AActor* OwnerActor = Actor->GetOwner())
		{
			if (OwnerActor != Actor)
			{
				return ResolvePlayerStateFromActor(OwnerActor);
			}
		}

		return nullptr;
	}

	FText ResolvePlayerDisplayName(const APlayerState* PlayerState)
	{
		if (const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState))
		{
			if (!PdPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName().IsEmpty())
			{
				return PdPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName();
			}
		}

		const FString PlayerName = PlayerState ? PlayerState->GetPlayerName() : FString();
		return PlayerName.IsEmpty()
			? FText::FromString(GetNameSafe(PlayerState))
			: FText::FromString(PlayerName);
	}

	void BroadcastKillLog(UWorld* World, const FKillLogEntry& KillLogEntry)
	{
		if (!World)
		{
			return;
		}

		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (APdPlayerController* PlayerController = Cast<APdPlayerController>(Iterator->Get()))
			{
				PlayerController->Client_AddKillLogEntry(KillLogEntry);
			}
		}
	}

	void TryBroadcastKillLog(AActor* VictimActor, AActor* DamageInstigator, AActor* DamageCauser)
	{
		if (!VictimActor || !VictimActor->HasAuthority())
		{
			return;
		}

		APlayerState* VictimPlayerState = ResolvePlayerStateFromActor(VictimActor);
		if (!VictimPlayerState)
		{
			return;
		}

		APlayerState* KillerPlayerState = ResolvePlayerStateFromActor(DamageInstigator);
		if (!KillerPlayerState)
		{
			KillerPlayerState = ResolvePlayerStateFromActor(DamageCauser);
		}

		FKillLogEntry KillLogEntry;
		KillLogEntry.VictimName = ResolvePlayerDisplayName(VictimPlayerState);
		KillLogEntry.bEnvironmentKill = !KillerPlayerState;
		KillLogEntry.bSelfKill = KillerPlayerState && KillerPlayerState == VictimPlayerState;
		KillLogEntry.KillerName = KillerPlayerState
			? ResolvePlayerDisplayName(KillerPlayerState)
			: NSLOCTEXT("KillLog", "EnvironmentKillerName", "Environment");

		BroadcastKillLog(VictimActor->GetWorld(), KillLogEntry);

		if (APdPlayerState* VictimPdPlayerState = Cast<APdPlayerState>(VictimPlayerState))
		{
			VictimPdPlayerState->GetPlayerMatchComponent()->RecordDeath();
		}

		if (KillerPlayerState && KillerPlayerState != VictimPlayerState)
		{
			const float PreviousScore = KillerPlayerState->GetScore();
			KillerPlayerState->SetScore(PreviousScore + 1.0f);

			APdPlayerState* KillerPdPlayerState = Cast<APdPlayerState>(KillerPlayerState);
			ULevelingComponent* LevelingComponent = KillerPdPlayerState ? KillerPdPlayerState->GetLevelingComponent() : nullptr;
			const bool bGrantedKillExperience = LevelingComponent
				? LevelingComponent->GrantKillExperience(VictimPlayerState)
				: false;

			if (UWorld* World = VictimActor->GetWorld())
			{
				if (AExperienceGameMode* ExperienceGameMode = World->GetAuthGameMode<AExperienceGameMode>())
				{
					ExperienceGameMode->NotifyPlayerKillScored(KillerPlayerState, VictimPlayerState);
				}
			}
		}
	}
}

UBasicAttributeSet::UBasicAttributeSet()
{
	Level = 1.0f;
	Experience = 0.0f;
	MaxExperience = 100.0f;
	OffensePoint = 0.0f;
	DefensePoint = 0.0f;
	ResistancePoint = 0.0f;
	PandoraForcePoint = 0.0f;
	ResourcePoint = 0.0f;
	AgilityPoint = 0.0f;
	StrengthLevel = 0.0f;
	IntelligenceLevel = 0.0f;
	ArcaneLevel = 0.0f;
	ArmorLevel = 0.0f;
	RecoveryLevel = 0.0f;
	FrostbiteLevel = 0.0f;
	BurnLevel = 0.0f;
	ElectricShockLevel = 0.0f;
	FirstPandoraLevel = 0.0f;
	SecondPandoraLevel = 0.0f;
	ThirdPandoraLevel = 0.0f;
	MaxHealthLevel = 0.0f;
	MaxShieldLevel = 0.0f;
	MaxManaLevel = 0.0f;
	MaxStaminaLevel = 0.0f;
	AttackSpeedLevel = 0.0f;
	MovementSpeedLevel = 0.0f;
	CriticalLevel = 0.0f;
	Health = 100.0f;
	MaxHealth = 100.0f;
	Shield = 0.0f;
	MaxShield = 100.0f;
	Stamina = 100.0f;
	MaxStamina = 100.0f;
	MaxHealthIncreasePercent = 0.0f;
	MaxShieldIncreasePercent = 0.0f;
	MaxManaIncreasePercent = 0.0f;
	MaxStaminaIncreasePercent = 0.0f;
}

bool UBasicAttributeSet::ResolveAttributeFromStatTag(
	const FGameplayTag& StatTag,
	FGameplayAttribute& OutAttribute)
{
	static const TMap<FGameplayTag, FGameplayAttribute> AttributeMappings =
	{
		{ LabGameplayTags::Status_Level, GetLevelAttribute() },
		{ LabGameplayTags::Status_Experience, GetExperienceAttribute() },
		{ LabGameplayTags::Status_MaxExperience, GetMaxExperienceAttribute() },
		{ LabGameplayTags::Status_Point_Offense, GetOffensePointAttribute() },
		{ LabGameplayTags::Status_Point_Defense, GetDefensePointAttribute() },
		{ LabGameplayTags::Status_Point_Resistance, GetResistancePointAttribute() },
		{ LabGameplayTags::Status_Point_PandoraForce, GetPandoraForcePointAttribute() },
		{ LabGameplayTags::Status_Point_Resource, GetResourcePointAttribute() },
		{ LabGameplayTags::Status_Point_Agility, GetAgilityPointAttribute() },
		{ LabGameplayTags::Status_Offense_Strength, GetStrengthAttribute() },
		{ LabGameplayTags::Status_Offense_Intelligence, GetIntelligenceAttribute() },
		{ LabGameplayTags::Status_Offense_Critical, GetCriticalAttribute() },
		{ LabGameplayTags::Status_Offense_StrengthLevel, GetStrengthLevelAttribute() },
		{ LabGameplayTags::Status_Offense_IntelligenceLevel, GetIntelligenceLevelAttribute() },
		{ LabGameplayTags::Status_Offense_CriticalLevel, GetCriticalLevelAttribute() },
		{ LabGameplayTags::Status_Defense_Armor, GetArmorAttribute() },
		{ LabGameplayTags::Status_Defense_Recovery, GetRecoveryAttribute() },
		{ LabGameplayTags::Status_Defense_Shield, GetShieldAttribute() },
		{ LabGameplayTags::Status_Defense_MaxShield, GetMaxShieldAttribute() },
		{ LabGameplayTags::Status_Defense_MaxShieldIncreasePercent, GetMaxShieldIncreasePercentAttribute() },
		{ LabGameplayTags::Status_Defense_ArmorLevel, GetArmorLevelAttribute() },
		{ LabGameplayTags::Status_Defense_RecoveryLevel, GetRecoveryLevelAttribute() },
		{ LabGameplayTags::Status_Defense_MaxShieldLevel, GetMaxShieldLevelAttribute() },
		{ LabGameplayTags::Status_Resistance_Frostbite, GetFrostbiteAttribute() },
		{ LabGameplayTags::Status_Resistance_Burn, GetBurnAttribute() },
		{ LabGameplayTags::Status_Resistance_ElectricShock, GetElectricShockAttribute() },
		{ LabGameplayTags::Status_Resistance_FrostbiteLevel, GetFrostbiteLevelAttribute() },
		{ LabGameplayTags::Status_Resistance_BurnLevel, GetBurnLevelAttribute() },
		{ LabGameplayTags::Status_Resistance_ElectricShockLevel, GetElectricShockLevelAttribute() },
		{ LabGameplayTags::Status_PandoraForce_FirstPandora, GetFirstPandoraAttribute() },
		{ LabGameplayTags::Status_PandoraForce_SecondPandora, GetSecondPandoraAttribute() },
		{ LabGameplayTags::Status_PandoraForce_ThirdPandora, GetThirdPandoraAttribute() },
		{ LabGameplayTags::Status_PandoraForce_FirstPandoraLevel, GetFirstPandoraLevelAttribute() },
		{ LabGameplayTags::Status_PandoraForce_SecondPandoraLevel, GetSecondPandoraLevelAttribute() },
		{ LabGameplayTags::Status_PandoraForce_ThirdPandoraLevel, GetThirdPandoraLevelAttribute() },
		{ LabGameplayTags::Status_Resource_Health, GetHealthAttribute() },
		{ LabGameplayTags::Status_Resource_Mana, GetManaAttribute() },
		{ LabGameplayTags::Status_Resource_Stamina, GetStaminaAttribute() },
		{ LabGameplayTags::Status_Resource_MaxHealth, GetMaxHealthAttribute() },
		{ LabGameplayTags::Status_Resource_MaxMana, GetMaxManaAttribute() },
		{ LabGameplayTags::Status_Resource_MaxStamina, GetMaxStaminaAttribute() },
		{ LabGameplayTags::Status_Resource_MaxHealthIncreasePercent, GetMaxHealthIncreasePercentAttribute() },
		{ LabGameplayTags::Status_Resource_MaxManaIncreasePercent, GetMaxManaIncreasePercentAttribute() },
		{ LabGameplayTags::Status_Resource_MaxStaminaIncreasePercent, GetMaxStaminaIncreasePercentAttribute() },
		{ LabGameplayTags::Status_Resource_MaxHealthLevel, GetMaxHealthLevelAttribute() },
		{ LabGameplayTags::Status_Resource_MaxManaLevel, GetMaxManaLevelAttribute() },
		{ LabGameplayTags::Status_Resource_MaxStaminaLevel, GetMaxStaminaLevelAttribute() },
		{ LabGameplayTags::Status_Agility_AttackSpeed, GetAttackSpeedAttribute() },
		{ LabGameplayTags::Status_Agility_MovementSpeed, GetMovementSpeedAttribute() },
		{ LabGameplayTags::Status_Agility_Arcane, GetArcaneAttribute() },
		{ LabGameplayTags::Status_Agility_CriticalDamageMultiplier, GetCriticalDamageMultiplierAttribute() },
		{ LabGameplayTags::Status_Agility_AttackSpeedLevel, GetAttackSpeedLevelAttribute() },
		{ LabGameplayTags::Status_Agility_MovementSpeedLevel, GetMovementSpeedLevelAttribute() },
		{ LabGameplayTags::Status_Agility_ArcaneLevel, GetArcaneLevelAttribute() }
	};

	const FGameplayAttribute* Attribute = AttributeMappings.Find(StatTag);
	OutAttribute = Attribute ? *Attribute : FGameplayAttribute();
	return OutAttribute.IsValid();
}

void UBasicAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// =================================================================================================================

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Strength, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Level, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Experience, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxExperience, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, OffensePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, DefensePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ResistancePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, PandoraForcePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ResourcePoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, AgilityPoint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, StrengthLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, IntelligenceLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ArcaneLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ArmorLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, RecoveryLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, FrostbiteLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, BurnLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ElectricShockLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, FirstPandoraLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, SecondPandoraLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ThirdPandoraLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxHealthLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxShieldLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxManaLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxStaminaLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, AttackSpeedLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MovementSpeedLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, CriticalLevel, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Intelligence, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Arcane, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Armor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Recovery, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Frostbite, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Burn, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ElectricShock, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, FirstPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, SecondPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ThirdPandora, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, AttackSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MovementSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Critical, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, CriticalDamageMultiplier, Params);

	// =================================================================================================================

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Health, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxHealth, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Shield, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxShield, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Mana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxMana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Stamina, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxStamina, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxHealthIncreasePercent, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxShieldIncreasePercent, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxManaIncreasePercent, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxStaminaIncreasePercent, Params);
}

void UBasicAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// =================================================================================================================

	if (Data.EvaluatedData.Attribute == GetOutgoingDamageAttribute())
	{
		bLastOutgoingDamageCriticalHit = false;

		float FinalOutgoingDamage = GetOutgoingDamage();
		if (FinalOutgoingDamage <= 0.f)
		{
			SetOutgoingDamage(0.f);
			return;
		}

		const float CriticalValue = GetCritical();
		if (RollPercentChance(CriticalValue))
		{
			FinalOutgoingDamage *= CalculateCriticalDamageMultiplier(CriticalValue);
			bLastOutgoingDamageCriticalHit = true;
		}

		SetOutgoingDamage(FinalOutgoingDamage);
		return;
	}

	if (Data.EvaluatedData.Attribute == GetLevelAttribute())
	{
		SetLevel(FMath::Max(GetLevel(), 1.f));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetExperienceAttribute())
	{
		SetExperience(FMath::Max(GetExperience(), 0.f));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetMaxExperienceAttribute())
	{
		SetMaxExperience(FMath::Max(GetMaxExperience(), 0.f));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetOffensePointAttribute())
	{
		SetOffensePoint(FMath::Max(GetOffensePoint(), 0.f));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetDefensePointAttribute())
	{
		SetDefensePoint(FMath::Max(GetDefensePoint(), 0.f));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetResistancePointAttribute())
	{
		SetResistancePoint(FMath::Max(GetResistancePoint(), 0.f));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetPandoraForcePointAttribute())
	{
		SetPandoraForcePoint(FMath::Max(GetPandoraForcePoint(), 0.f));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetResourcePointAttribute())
	{
		SetResourcePoint(FMath::Max(GetResourcePoint(), 0.f));
		return;
	}

	if (Data.EvaluatedData.Attribute == GetAgilityPointAttribute())
	{
		SetAgilityPoint(FMath::Max(GetAgilityPoint(), 0.f));
		return;
	}

	if (ClampInvestedStatLevelAttribute(Data.EvaluatedData.Attribute, this))
	{
		return;
	}

	if (ClampStatValueAttribute(Data.EvaluatedData.Attribute, this))
	{
		return;
	}

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float AppliedIncomingDamage = GetIncomingDamage();
		const bool bCriticalHit = bPendingIncomingDamageCriticalHit;
		const bool bAllowHitReact = bPendingIncomingDamageAllowHitReact;
		const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetContext();
		AActor* DamageInstigator = EffectContext.GetOriginalInstigator();
		if (!DamageInstigator)
		{
			DamageInstigator = EffectContext.GetInstigator();
		}
		AActor* DamageCauser = EffectContext.GetEffectCauser();
		const bool bBurningStatusDamage = IsBurningStatusDamageEffectSpec(Data.EffectSpec);
		const bool bFrozenStatusDamage = IsFrozenStatusDamageEffectSpec(Data.EffectSpec);
		const bool bElectricShockStatusDamage = IsElectricShockStatusDamageEffectSpec(Data.EffectSpec);
		const bool bStatusDamage = bBurningStatusDamage || bFrozenStatusDamage || bElectricShockStatusDamage;
		const float TargetStatusResistance = ResolveStatusResistance(
			this,
			bBurningStatusDamage,
			bFrozenStatusDamage,
			bElectricShockStatusDamage);
		const float StatusMitigatedIncomingDamage = bStatusDamage
			? CalculateStatusResistanceMitigatedDamage(AppliedIncomingDamage, TargetStatusResistance)
			: AppliedIncomingDamage;
		const float TargetArmor = GetArmor();
		const float TargetFinalStrength = CalculateFinalStrengthDamage(this);
		const float MitigatedIncomingDamage = CalculateArmorMitigatedDamage(StatusMitigatedIncomingDamage, TargetArmor, TargetFinalStrength);

		bPendingIncomingDamageCriticalHit = false;
		bPendingIncomingDamageAllowHitReact = true;
		SetIncomingDamage(0.f);
		const bool bAllowDamageHitReact = bAllowHitReact && !bStatusDamage;
		const float HealthDamage = ApplyIncomingDamage(MitigatedIncomingDamage, bCriticalHit, DamageInstigator, DamageCauser, bAllowDamageHitReact);
		const bool bShouldHitReact =
			bAllowHitReact
			&& !FMath::IsNearlyZero(HealthDamage)
			&& !bStatusDamage
			&& EffectSpecHasAssetTag(Data.EffectSpec, LabGameplayTags::Effect_HitReaction);

		if (bShouldHitReact && GetHealth() > 0.f)
		{
			TryActivateHitReactionAbility(GetOwningAbilitySystemComponent());
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		const bool bStatusDamage = IsStatusDamageEffectSpec(Data.EffectSpec);
		const bool bShouldHitReact =
			Data.EvaluatedData.Magnitude < 0.f
			&& !bStatusDamage
			&& EffectSpecHasAssetTag(Data.EffectSpec, LabGameplayTags::Effect_HitReaction);
		const float RecoveryManaMagnitude = Data.EvaluatedData.Magnitude > 0.f
			? Data.EffectSpec.GetSetByCallerMagnitude(LabGameplayTags::Data_Mana, false, 0.f)
			: 0.f;
		if (RecoveryManaMagnitude > 0.f)
		{
			const float OldMana = GetMana();
			const float NewMana = ClampResourceAttribute(OldMana + RecoveryManaMagnitude, GetMaxMana());
			SetMana(NewMana);
			MARK_PROPERTY_DIRTY_FROM_NAME(UBasicAttributeSet, Mana, this);

		}

		SetHealth(GetHealth());
		if (bShouldHitReact && GetHealth() > 0.f)
		{
			TryActivateHitReactionAbility(GetOwningAbilitySystemComponent());
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(GetStamina());
		return;
	}

	if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		SetMana(GetMana());
		return;
	}
}

void UBasicAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// =================================================================================================================

	if (Attribute == GetHealthAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxShield());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxStamina());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxMana());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxShieldAttribute()
		|| Attribute == GetMaxStaminaAttribute() || Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetLevelAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetExperienceAttribute()
		|| Attribute == GetMaxExperienceAttribute()
		|| Attribute == GetOffensePointAttribute()
		|| Attribute == GetDefensePointAttribute()
		|| Attribute == GetResistancePointAttribute()
		|| Attribute == GetPandoraForcePointAttribute()
		|| Attribute == GetResourcePointAttribute()
		|| Attribute == GetAgilityPointAttribute()
		|| IsInvestedStatLevelAttribute(Attribute))
	{
		NewValue = IsInvestedStatLevelAttribute(Attribute)
			? ClampInvestedStatLevel(NewValue)
			: FMath::Max(NewValue, 0.f);
	}
	else if (IsStatValueAttribute(Attribute))
	{
		NewValue = ClampStatValue(NewValue);
	}
}

void UBasicAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// =================================================================================================================

	if (OldValue != NewValue)
	{

		if (FProperty* Property = Attribute.GetUProperty())
		{
			MARK_PROPERTY_DIRTY(this, Property);
		}
	}

	// =================================================================================================================

	if (Attribute == GetMaxHealthAttribute())
	{
		SetHealth(GetHealth());
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		SetShield(GetShield());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		SetStamina(GetStamina());
	}
	else if (Attribute == GetMaxManaAttribute())
	{
		SetMana(GetMana());
	}

	if (Attribute == GetHealthAttribute() && OldValue > 0.f && NewValue <= 0.f)
	{
		TryActivateDeathAbility(GetOwningAbilitySystemComponent());
	}

	if (Attribute == GetShieldAttribute())
	{
		if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
		{
			if (AActor* OwnerActor = ASC->GetOwnerActor(); OwnerActor && OwnerActor->HasAuthority())
			{
				if (OldValue <= 0.f && NewValue > 0.f)
				{
					ASC->AddGameplayCue(LabGameplayTags::GameplayCue_ShieldUp);
				}
				else if (OldValue > 0.f && NewValue <= 0.f)
				{
					ASC->RemoveGameplayCue(LabGameplayTags::GameplayCue_ShieldUp);
					ASC->ExecuteGameplayCue(LabGameplayTags::GameplayCue_ShieldDown);
				}
			}
		}
	}
}

float UBasicAttributeSet::ConsumeOutgoingDamage()
{
	const float ConsumedOutgoingDamage = GetOutgoingDamage();
	SetOutgoingDamage(0.f);
	return ConsumedOutgoingDamage;
}

bool UBasicAttributeSet::ConsumeOutgoingDamageCriticalHit()
{
	const bool bConsumedCriticalHit = bLastOutgoingDamageCriticalHit;
	bLastOutgoingDamageCriticalHit = false;
	return bConsumedCriticalHit;
}

void UBasicAttributeSet::SetPendingIncomingDamageCriticalHit(bool bCriticalHit)
{
	bPendingIncomingDamageCriticalHit = bCriticalHit;
}

void UBasicAttributeSet::SetPendingIncomingDamageAllowHitReact(bool bAllowHitReact)
{
	bPendingIncomingDamageAllowHitReact = bAllowHitReact;
}

float UBasicAttributeSet::ApplyIncomingDamage(float IncomingDamageAmount, bool bCriticalHit, AActor* DamageInstigator, AActor* DamageCauser, bool bAllowHitReact)
{
	const float FinalDamage = FMath::Max(IncomingDamageAmount, 0.f);

	if (FinalDamage <= 0.f)
	{
		return 0.f;
	}

	float RemainingHealthDamage = FinalDamage;
	float ShieldDamage = 0.0f;
	const float CurrentShield = GetShield();
	if (CurrentShield > 0.f)
	{
		ShieldDamage = FMath::Min(FinalDamage, CurrentShield);
		SetShield(FMath::Max(CurrentShield - FinalDamage, 0.f));
		RemainingHealthDamage = FMath::Max(FinalDamage - ShieldDamage, 0.f);
	}

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (ACharacterBase* Character = Cast<ACharacterBase>(ASC->GetAvatarActor()))
		{
			const float DisplayDamage = ShieldDamage > 0.f ? ShieldDamage : RemainingHealthDamage;
			Character->HandleDamageTaken(DisplayDamage, bCriticalHit, bAllowHitReact, DamageInstigator, DamageCauser);
		}
	}

	if (RemainingHealthDamage <= 0.f)
	{

		return 0.f;
	}

	const float OldHealth = GetHealth();
	const float NewHealth = FMath::Max(OldHealth - RemainingHealthDamage, 0.f);
	SetHealth(NewHealth);
	if (OldHealth > 0.f && NewHealth <= 0.f)
	{
		TryBroadcastKillLog(GetOwningActor(), DamageInstigator, DamageCauser);
	}

	if (OldHealth > 0.f && NewHealth <= 0.f)
	{
		TryActivateDeathAbility(GetOwningAbilitySystemComponent());
	}

	return RemainingHealthDamage;
}

void UBasicAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Strength, OldValue);
}

void UBasicAttributeSet::OnRep_Level(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Level, OldValue);
}

void UBasicAttributeSet::OnRep_Experience(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Experience, OldValue);
}

void UBasicAttributeSet::OnRep_MaxExperience(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxExperience, OldValue);
}

void UBasicAttributeSet::OnRep_OffensePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, OffensePoint, OldValue);
}

void UBasicAttributeSet::OnRep_DefensePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, DefensePoint, OldValue);
}

void UBasicAttributeSet::OnRep_ResistancePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ResistancePoint, OldValue);
}

void UBasicAttributeSet::OnRep_PandoraForcePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, PandoraForcePoint, OldValue);
}

void UBasicAttributeSet::OnRep_ResourcePoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ResourcePoint, OldValue);
}

void UBasicAttributeSet::OnRep_AgilityPoint(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, AgilityPoint, OldValue);
}

void UBasicAttributeSet::OnRep_StrengthLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, StrengthLevel, OldValue);
}

void UBasicAttributeSet::OnRep_IntelligenceLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, IntelligenceLevel, OldValue);
}

void UBasicAttributeSet::OnRep_ArcaneLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ArcaneLevel, OldValue);
}

void UBasicAttributeSet::OnRep_ArmorLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ArmorLevel, OldValue);
}

void UBasicAttributeSet::OnRep_RecoveryLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, RecoveryLevel, OldValue);
}

void UBasicAttributeSet::OnRep_FrostbiteLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, FrostbiteLevel, OldValue);
}

void UBasicAttributeSet::OnRep_BurnLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, BurnLevel, OldValue);
}

void UBasicAttributeSet::OnRep_ElectricShockLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ElectricShockLevel, OldValue);
}

void UBasicAttributeSet::OnRep_FirstPandoraLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, FirstPandoraLevel, OldValue);
}

void UBasicAttributeSet::OnRep_SecondPandoraLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, SecondPandoraLevel, OldValue);
}

void UBasicAttributeSet::OnRep_ThirdPandoraLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ThirdPandoraLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MaxHealthLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxHealthLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MaxShieldLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxShieldLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MaxManaLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxManaLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MaxStaminaLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxStaminaLevel, OldValue);
}

void UBasicAttributeSet::OnRep_AttackSpeedLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, AttackSpeedLevel, OldValue);
}

void UBasicAttributeSet::OnRep_MovementSpeedLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MovementSpeedLevel, OldValue);
}

void UBasicAttributeSet::OnRep_CriticalLevel(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, CriticalLevel, OldValue);
}

void UBasicAttributeSet::OnRep_Intelligence(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Intelligence, OldValue);
}

void UBasicAttributeSet::OnRep_Arcane(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Arcane, OldValue);
}

void UBasicAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Armor, OldValue);
}

void UBasicAttributeSet::OnRep_Recovery(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Recovery, OldValue);
}

void UBasicAttributeSet::OnRep_Frostbite(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Frostbite, OldValue);
}

void UBasicAttributeSet::OnRep_Burn(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Burn, OldValue);
}

void UBasicAttributeSet::OnRep_ElectricShock(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ElectricShock, OldValue);
}

void UBasicAttributeSet::OnRep_FirstPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, FirstPandora, OldValue);
}

void UBasicAttributeSet::OnRep_SecondPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, SecondPandora, OldValue);
}

void UBasicAttributeSet::OnRep_ThirdPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ThirdPandora, OldValue);
}

void UBasicAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, AttackSpeed, OldValue);
}

void UBasicAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MovementSpeed, OldValue);
}

void UBasicAttributeSet::OnRep_Critical(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Critical, OldValue);
}

void UBasicAttributeSet::OnRep_CriticalDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, CriticalDamageMultiplier, OldValue);
}

void UBasicAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Health, OldValue);
}

void UBasicAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxHealth, OldValue);
}

void UBasicAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Shield, OldValue);
}

void UBasicAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxShield, OldValue);
}

void UBasicAttributeSet::OnRep_MaxShieldIncreasePercent(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxShieldIncreasePercent, OldValue);
}

void UBasicAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Mana, OldValue);
}

void UBasicAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxMana, OldValue);
}

void UBasicAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Stamina, OldValue);
}

void UBasicAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxStamina, OldValue);
}

void UBasicAttributeSet::OnRep_MaxHealthIncreasePercent(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxHealthIncreasePercent, OldValue);
}

void UBasicAttributeSet::OnRep_MaxManaIncreasePercent(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxManaIncreasePercent, OldValue);
}

void UBasicAttributeSet::OnRep_MaxStaminaIncreasePercent(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxStaminaIncreasePercent, OldValue);
}
