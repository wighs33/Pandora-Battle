#include "Component/AbilitySystem/Ability/AbilityResourceRuntime.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/Ability/AbilitySourceRuntime.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityResourceRuntime)

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

FGameplayTag GetPandoraSkillCooldownTagFromSourceTags(
	const FGameplayTagContainer& SourceTags)
{
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill1))
	{
		return LabGameplayTags::Cooldown_Skill1;
	}
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill2))
	{
		return LabGameplayTags::Cooldown_Skill2;
	}
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill3))
	{
		return LabGameplayTags::Cooldown_Skill3;
	}
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill4))
	{
		return LabGameplayTags::Cooldown_Skill4;
	}

	return FGameplayTag();
}
}

const FGameplayTagContainer* UAbilityResourceRuntime::BuildCooldownTags(
	const UPdGameplayAbility& Ability,
	const FGameplayTagContainer* ParentCooldownTags) const
{
	RuntimeCooldownTags.Reset();
	if (ParentCooldownTags)
	{
		RuntimeCooldownTags.AppendTags(*ParentCooldownTags);
	}

	if (!Ability.IsInstantiated())
	{
		return RuntimeCooldownTags.IsEmpty()
			? nullptr
			: &RuntimeCooldownTags;
	}

	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	if (SkillDataAsset)
	{
		// SkillDefinition is the only cooldown configuration source for skills.
		// Ignore legacy CooldownGameplayEffectClass tags authored on the GA.
		RuntimeCooldownTags.Reset();
		if (SkillDataAsset->Time.CooldownDuration <= 0.0)
		{
			return nullptr;
		}

		const FGameplayTag DefaultCooldownTag = LabGameplayTags::Cooldown;
		const FGameplayTag SkillSlotCooldownTag =
			GetSkillSlotCooldownTag(Ability);
		if (SkillSlotCooldownTag.IsValid())
		{
			RuntimeCooldownTags.RemoveTag(DefaultCooldownTag);
			RuntimeCooldownTags.AddTag(SkillSlotCooldownTag);
		}
		else if (DefaultCooldownTag.IsValid())
		{
			RuntimeCooldownTags.AddTag(DefaultCooldownTag);
		}
	}

	return RuntimeCooldownTags.IsEmpty() ? nullptr : &RuntimeCooldownTags;
}

bool UAbilityResourceRuntime::CheckCost(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: (SourceRuntime
				? SourceRuntime->ResolveCurrentAbilitySpec(Ability)
				: nullptr);
	const USkillDefinition* SkillDataAsset = SourceRuntime
		? SourceRuntime->ResolveSkillDataAsset(
			Ability,
			AbilitySpec,
			ActorInfo)
		: nullptr;
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

void UAbilityResourceRuntime::ApplyCost(
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

	const UAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: (SourceRuntime
				? SourceRuntime->ResolveCurrentAbilitySpec(Ability)
				: nullptr);
	const USkillDefinition* SkillDataAsset = SourceRuntime
		? SourceRuntime->ResolveSkillDataAsset(
			Ability,
			AbilitySpec,
			ActorInfo)
		: nullptr;
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

bool UAbilityResourceRuntime::CheckConfiguredCooldown(
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
	const UAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: (SourceRuntime
				? SourceRuntime->ResolveCurrentAbilitySpec(Ability)
				: nullptr);
	const USkillDefinition* SkillDataAsset = SourceRuntime
		? SourceRuntime->ResolveSkillDataAsset(
			Ability,
			AbilitySpec,
			ActorInfo)
		: nullptr;
	if (!AbilitySystemComponent
		|| !SkillDataAsset
		|| SkillDataAsset->Time.CooldownDuration <= 0.0)
	{
		return true;
	}

	bOutHandled = true;
	FGameplayTagContainer CooldownTags;
	if (ParentCooldownTags)
	{
		CooldownTags.AppendTags(*ParentCooldownTags);
	}

	const FGameplayTag DefaultCooldownTag = LabGameplayTags::Cooldown;
	const FGameplayTag SkillSlotCooldownTag = AbilitySpec
		? GetPandoraSkillCooldownTagFromSourceTags(
			AbilitySpec->GetDynamicSpecSourceTags())
		: FGameplayTag();
	if (SkillSlotCooldownTag.IsValid())
	{
		CooldownTags.RemoveTag(DefaultCooldownTag);
		CooldownTags.AddTag(SkillSlotCooldownTag);
	}
	else if (DefaultCooldownTag.IsValid())
	{
		CooldownTags.AddTag(DefaultCooldownTag);
	}

	if (CooldownTags.IsEmpty()
		|| !AbilitySystemComponent->HasAnyMatchingGameplayTags(CooldownTags))
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

bool UAbilityResourceRuntime::ShouldDeferCooldown(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: (SourceRuntime
				? SourceRuntime->ResolveCurrentAbilitySpec(Ability)
				: nullptr);
	const USkillDefinition* SkillDataAsset = SourceRuntime
		? SourceRuntime->ResolveSkillDataAsset(
			Ability,
			AbilitySpec,
			ActorInfo)
		: nullptr;
	return SkillDataAsset
		&& SkillDataAsset->Time.CooldownDuration > 0.0;
}

bool UAbilityResourceRuntime::ApplyConfiguredCooldownImmediately(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
	const FGameplayAbilitySpec* AbilitySpec =
		AbilitySystemComponent && Handle.IsValid()
			? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
			: (SourceRuntime
				? SourceRuntime->ResolveCurrentAbilitySpec(Ability)
				: nullptr);
	const USkillDefinition* SkillDataAsset = SourceRuntime
		? SourceRuntime->ResolveSkillDataAsset(
			Ability,
			AbilitySpec,
			ActorInfo)
		: nullptr;
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

	FGameplayTagContainer DynamicCooldownTags;
	BuildDynamicCooldownGrantedTags(Ability, DynamicCooldownTags);
	Ability.ApplySharedCooldownEffect(
		Handle,
		ActorInfo,
		ActivationInfo,
		EffectiveDuration,
		DynamicCooldownTags,
		SourceRuntime && SourceRuntime->IsPandoraSkillSpec(AbilitySpec));
	return true;
}

void UAbilityResourceRuntime::AppendCooldownRemovalPolicyTags(
	FGameplayEffectSpecHandle& CooldownSpecHandle,
	const FGameplayTagContainer& RemovalPolicyTags,
	const bool bPandoraCooldown) const
{
	if (!CooldownSpecHandle.IsValid()
		|| !CooldownSpecHandle.Data.IsValid())
	{
		return;
	}

	FGameplayTagContainer PolicyTags = RemovalPolicyTags;
	if (bPandoraCooldown)
	{
		PolicyTags.AddTag(
			LabGameplayTags::Effect_Policy_RemoveOnPandoraReset);
	}

	if (!PolicyTags.IsEmpty())
	{
		CooldownSpecHandle.Data->AppendDynamicAssetTags(PolicyTags);
	}
}

bool UAbilityResourceRuntime::TryCommitAdditionalActionStaminaCost(
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

FGameplayTag UAbilityResourceRuntime::GetSkillSlotCooldownTag(
	const UPdGameplayAbility& Ability) const
{
	const UAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
	const FGameplayAbilitySpec* AbilitySpec = SourceRuntime
		? SourceRuntime->ResolveCurrentAbilitySpec(Ability)
		: nullptr;
	return AbilitySpec
		? GetPandoraSkillCooldownTagFromSourceTags(
			AbilitySpec->GetDynamicSpecSourceTags())
		: FGameplayTag();
}

void UAbilityResourceRuntime::BuildDynamicCooldownGrantedTags(
	const UPdGameplayAbility& Ability,
	FGameplayTagContainer& OutCooldownTags) const
{
	OutCooldownTags.Reset();

	const FGameplayTag SkillSlotCooldownTag =
		GetSkillSlotCooldownTag(Ability);
	if (SkillSlotCooldownTag.IsValid())
	{
		OutCooldownTags.AddTag(SkillSlotCooldownTag);
		return;
	}

	const FGameplayTag DefaultCooldownTag = LabGameplayTags::Cooldown;
	if (DefaultCooldownTag.IsValid())
	{
		OutCooldownTags.AddTag(DefaultCooldownTag);
	}
}

float UAbilityResourceRuntime::GetSkillCooldownReductionPercent(
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

bool UAbilityResourceRuntime::ConsumePendingCooldown(
	const bool bAbilityWasCancelled)
{
	const bool bWasPending = bApplySkillCooldownWhenAbilityEnds;
	bApplySkillCooldownWhenAbilityEnds = false;
	return bWasPending && !bAbilityWasCancelled;
}
