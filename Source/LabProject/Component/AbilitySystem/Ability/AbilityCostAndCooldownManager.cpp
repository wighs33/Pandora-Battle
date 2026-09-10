#include "Component/AbilitySystem/Ability/AbilityCostAndCooldownManager.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Pandora/PandoraSkillSource.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityCostAndCooldownManager)

namespace
{
TSubclassOf<UGameplayEffect> ResolveAbilityCostEffect(
	const UObject* WorldContextObject)
{
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(
			WorldContextObject);
	return SettingDefinition
		? SettingDefinition->AbilityCostGameplayEffectClass
		: nullptr;
}

bool SetAbilityCostMagnitudes(
	FGameplayEffectSpecHandle& SpecHandle,
	const float ManaCost,
	const float StaminaCost)
{
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		LabGameplayTags::Data_ManaCost,
		-FMath::Max(ManaCost, 0.0f));
	SpecHandle.Data->SetSetByCallerMagnitude(
		LabGameplayTags::Data_StaminaCost,
		-FMath::Max(StaminaCost, 0.0f));
	return true;
}

float ResolveActionStaminaCost(const UObject* WorldContextObject)
{
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(
			WorldContextObject);
	if (!SettingDefinition)
	{
		SettingDefinition = GetDefault<UGameSettingDefinition>();
	}

	return SettingDefinition
		? FMath::Max(SettingDefinition->ActionStaminaCost, 0.0f)
		: 0.0f;
}

bool IsPlayerControlledAvatar(const FGameplayAbilityActorInfo* ActorInfo)
{
	const APawn* AvatarPawn =
		ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	return AvatarPawn && AvatarPawn->IsPlayerControlled();
}

bool AbilitySpecHasExactAssetTag(
	const FGameplayAbilitySpec* AbilitySpec,
	const FGameplayTag& AbilityTag)
{
	return AbilitySpec
		&& AbilitySpec->Ability
		&& AbilityTag.IsValid()
		&& AbilitySpec->Ability->GetAssetTags().HasTagExact(AbilityTag);
}

float ResolveEquippedWeaponAttackStaminaCost(
	const FGameplayAbilityActorInfo* ActorInfo)
{
	const ACharacterBase* Character = ActorInfo
		? Cast<ACharacterBase>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const UEquipmentComponent* EquipmentComponent = Character
		? Character->GetEquipmentComponent()
		: nullptr;
	if (const UItemDefinition* WeaponDefinition = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponDefinition()
		: nullptr)
	{
		return WeaponDefinition->GetSafeAttackStaminaCost();
	}

	return ResolveActionStaminaCost(
		ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
}

float GetActionStaminaCost(
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilitySpec* AbilitySpec,
	const USkillDefinition* SkillDataAsset)
{
	if (!IsPlayerControlledAvatar(ActorInfo))
	{
		return 0.0f;
	}

	if (SkillDataAsset
		|| AbilitySpecHasExactAssetTag(
			AbilitySpec,
			LabGameplayTags::Action_Punch))
	{
		return ResolveActionStaminaCost(
			ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	}

	return AbilitySpecHasExactAssetTag(
		AbilitySpec,
		LabGameplayTags::Action_Attack)
		? ResolveEquippedWeaponAttackStaminaCost(ActorInfo)
		: 0.0f;
}

float GetManaCostFromSkillDataAsset(
	const USkillDefinition* SkillDataAsset)
{
	return SkillDataAsset
		? static_cast<float>(FMath::Max(SkillDataAsset->ManaCost, 0.0))
		: 0.0f;
}

void AddCostFailureTag(FGameplayTagContainer* OptionalRelevantTags)
{
	if (!OptionalRelevantTags)
	{
		return;
	}

	const FGameplayTag& FailCostTag =
		UAbilitySystemGlobals::Get().ActivateFailCostTag;
	if (FailCostTag.IsValid())
	{
		OptionalRelevantTags->AddTag(FailCostTag);
	}
}

}

const FGameplayTagContainer* UAbilityCostAndCooldownManager::BuildCooldownTags(
	const UPdGameplayAbility& Ability,
	const FGameplayTagContainer* ParentCooldownTags) const
{
	CachedCooldownTags.Reset();
	if (ParentCooldownTags)
	{
		CachedCooldownTags.AppendTags(*ParentCooldownTags);
	}

	if (!Ability.IsInstantiated())
	{
		return CachedCooldownTags.IsEmpty()
			? nullptr
			: &CachedCooldownTags;
	}

	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	if (SkillDataAsset)
	{
		// SkillDefinition is the only cooldown configuration source for skills.
		// Ignore legacy CooldownGameplayEffectClass tags authored on the GA.
		CachedCooldownTags.Reset();
		if (SkillDataAsset->Time.CooldownDuration <= 0.0)
		{
			return nullptr;
		}

		CachedCooldownTags.AddTag(LabGameplayTags::Cooldown);
	}

	return CachedCooldownTags.IsEmpty() ? nullptr : &CachedCooldownTags;
}

bool UAbilityCostAndCooldownManager::CheckCost(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: Ability.GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset =
		Ability.ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	const float ManaCost = GetManaCostFromSkillDataAsset(SkillDataAsset);
	const float StaminaCost =
		GetActionStaminaCost(ActorInfo, AbilitySpec, SkillDataAsset);
	if (ManaCost <= 0.0f && StaminaCost <= 0.0f)
	{
		return true;
	}

	const UBasicAttributeSet* BasicAttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;
	if (!AbilitySystemComponent
		|| !BasicAttributeSet
		|| !ResolveAbilityCostEffect(
			ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr))
	{
		AddCostFailureTag(OptionalRelevantTags);
		return false;
	}

	const bool bHasEnoughMana =
		BasicAttributeSet->GetMana() + UE_SMALL_NUMBER >= ManaCost;
	const bool bHasEnoughStamina =
		BasicAttributeSet->GetStamina() + UE_SMALL_NUMBER >= StaminaCost;
	if (bHasEnoughMana && bHasEnoughStamina)
	{
		return true;
	}

	AddCostFailureTag(OptionalRelevantTags);
	return false;
}

void UAbilityCostAndCooldownManager::ApplyCost(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo) const
{
	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilitySystemComponent
		|| !Ability.HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		return;
	}

	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: Ability.GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset =
		Ability.ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	const float ManaCost = GetManaCostFromSkillDataAsset(SkillDataAsset);
	const float StaminaCost =
		GetActionStaminaCost(ActorInfo, AbilitySpec, SkillDataAsset);
	const UBasicAttributeSet* BasicAttributeSet =
		AbilitySystemComponent->GetSet<UBasicAttributeSet>();
	if ((ManaCost <= 0.0f && StaminaCost <= 0.0f)
		|| !BasicAttributeSet)
	{
		return;
	}

	const float AppliedManaCost = FMath::Min(
		FMath::Max(BasicAttributeSet->GetMana(), 0.0f),
		ManaCost);
	const float AppliedStaminaCost = FMath::Min(
		FMath::Max(BasicAttributeSet->GetStamina(), 0.0f),
		StaminaCost);
	const TSubclassOf<UGameplayEffect> CostEffectClass =
		ResolveAbilityCostEffect(
			ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!CostEffectClass)
	{
		return;
	}

	FGameplayEffectSpecHandle CostSpecHandle =
		Ability.MakeOutgoingGameplayEffectSpec(
			Handle,
			ActorInfo,
			ActivationInfo,
			CostEffectClass,
			1.0f);
	if (!SetAbilityCostMagnitudes(
		CostSpecHandle,
		AppliedManaCost,
		AppliedStaminaCost))
	{
		return;
	}

	Ability.ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		CostSpecHandle);
}

bool UAbilityCostAndCooldownManager::CheckConfiguredCooldown(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* ParentCooldownTags,
	FGameplayTagContainer* OptionalRelevantTags,
	bool& bOutHandled) const
{
	bOutHandled = false;

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: Ability.GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset =
		Ability.ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	if (!AbilitySystemComponent || !SkillDataAsset)
	{
		return true;
	}

	bOutHandled = true;
	if (SkillDataAsset->Time.CooldownDuration <= 0.0)
	{
		return true;
	}
	const FGameplayTag DefaultCooldownTag = LabGameplayTags::Cooldown;
	const UPandoraSkillSource* Source = AbilitySpec ? Cast<UPandoraSkillSource>(AbilitySpec->SourceObject.Get()) : nullptr;
	bool bOnCooldown = false;
	if (Source)
	{
		float Remaining = 0.0f;
		float Duration = 0.0f;
		GetPandoraCooldown(*AbilitySystemComponent, *Source, Remaining, Duration);
		bOnCooldown = Remaining > 0.0f;
	}
	else
	{
		FGameplayTagContainer CooldownTags(DefaultCooldownTag);
		if (ParentCooldownTags)
		{
			CooldownTags.AppendTags(*ParentCooldownTags);
		}
		bOnCooldown = AbilitySystemComponent->HasAnyMatchingGameplayTags(CooldownTags);
	}
	if (!bOnCooldown)
	{
		return true;
	}

	if (OptionalRelevantTags)
	{
		const FGameplayTag& FailCooldownTag =
			UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
		OptionalRelevantTags->AddTag(
			FailCooldownTag.IsValid()
				? FailCooldownTag
				: DefaultCooldownTag);
	}

	return false;
}

bool UAbilityCostAndCooldownManager::ShouldDeferCooldown(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: Ability.GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset =
		Ability.ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	return SkillDataAsset
		&& SkillDataAsset->Time.CooldownDuration > 0.0;
}

bool UAbilityCostAndCooldownManager::ApplyConfiguredCooldownImmediately(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: Ability.GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset =
		Ability.ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	if (!SkillDataAsset)
	{
		return false;
	}

	const float ConfiguredDuration = static_cast<float>(
		FMath::Max(SkillDataAsset->Time.CooldownDuration, 0.0));
	if (ConfiguredDuration <= 0.0f)
	{
		return true;
	}
	float EffectiveDuration = ConfiguredDuration;
	const float ArcaneReductionPercent =
		GetSkillCooldownReductionPercent(Ability);
	const bool bHasArcaneCooldownReduction =
		ArcaneReductionPercent > 0.0f;
	if (EffectiveDuration > 0.0f && bHasArcaneCooldownReduction)
	{
		const float ReductionAlpha =
			FMath::Clamp(ArcaneReductionPercent, 0.0f, 100.0f) / 100.0f;
		EffectiveDuration =
			FMath::Max(EffectiveDuration * (1.0f - ReductionAlpha), 0.0f);
	}

	if (EffectiveDuration <= 0.0f)
	{
		return true;
	}

	const FGameplayTagContainer DynamicCooldownTags(LabGameplayTags::Cooldown);
	Ability.ApplySharedCooldownEffect(
		Handle,
		ActorInfo,
		ActivationInfo,
		EffectiveDuration,
		DynamicCooldownTags);
	return true;
}

bool UAbilityCostAndCooldownManager::TryCommitAdditionalActionStaminaCost(
	const UPdGameplayAbility& Ability) const
{
	const FGameplayAbilityActorInfo* ActorInfo =
		Ability.GetCurrentActorInfo();
	if (!IsPlayerControlledAvatar(ActorInfo))
	{
		return true;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UBasicAttributeSet* BasicAttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;
	if (!AbilitySystemComponent || !BasicAttributeSet)
	{
		return false;
	}

	const float ActionStaminaCost =
		ResolveEquippedWeaponAttackStaminaCost(ActorInfo);
	if (ActionStaminaCost <= 0.0f)
	{
		return true;
	}

	if (BasicAttributeSet->GetStamina() + UE_SMALL_NUMBER
		< ActionStaminaCost)
	{
		return false;
	}

	// Autonomous proxies only validate replicated stamina; the server spends it.
	if (!ActorInfo->IsNetAuthority())
	{
		return true;
	}

	const TSubclassOf<UGameplayEffect> CostEffectClass =
		ResolveAbilityCostEffect(
			ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!CostEffectClass)
	{
		return false;
	}

	FGameplayEffectSpecHandle CostSpecHandle =
		Ability.MakeOutgoingGameplayEffectSpec(
			Ability.GetCurrentAbilitySpecHandle(),
			ActorInfo,
			Ability.GetCurrentActivationInfo(),
			CostEffectClass,
			1.0f);
	if (!SetAbilityCostMagnitudes(
		CostSpecHandle,
		0.0f,
		ActionStaminaCost))
	{
		return false;
	}

	return Ability.ApplyGameplayEffectSpecToOwner(
		Ability.GetCurrentAbilitySpecHandle(),
		ActorInfo,
		Ability.GetCurrentActivationInfo(),
		CostSpecHandle).WasSuccessfullyApplied();
}

// 시전 가능 여부와 스킬바가 GAS의 동일한 효과를 조회한다. 별도의 쿨다운 사본은 보관하지 않는다.
void UAbilityCostAndCooldownManager::GetPandoraCooldown(const UAbilitySystemComponent& ASC,
	const UPandoraSkillSource& Source, float& OutRemaining, float& OutDuration)
{
	OutRemaining = 0.0f;
	OutDuration = 0.0f;
	for (const TPair<float, float>& Time : ASC.GetActiveEffectsTimeRemainingAndDuration(Source.MakeCooldownQuery()))
	{
		if (Time.Key > OutRemaining)
		{
			OutRemaining = Time.Key;
			OutDuration = Time.Value;
		}
	}
}

float UAbilityCostAndCooldownManager::GetSkillCooldownReductionPercent(
	const UPdGameplayAbility& Ability) const
{
	const UPdAbilitySystemComponent* AbilitySystemComponent =
		Ability.GetPdAbilitySystemComponentFromActorInfo();
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;
	return AttributeSet
		? FMath::Max(AttributeSet->GetArcane(), 0.0f)
		: 0.0f;
}

bool UAbilityCostAndCooldownManager::ConsumePendingCooldown(
	const bool bAbilityWasCancelled)
{
	const bool bWasPending = bApplySkillCooldownWhenAbilityEnds;
	bApplySkillCooldownWhenAbilityEnds = false;
	return bWasPending && !bAbilityWasCancelled;
}
