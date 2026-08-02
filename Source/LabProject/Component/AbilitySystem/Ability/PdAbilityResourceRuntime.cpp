#include "Component/AbilitySystem/Ability/PdAbilityResourceRuntime.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystem/SkillCooldownGameplayEffect.h"
#include "AbilitySystemGlobals.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/Ability/PdAbilitySourceRuntime.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAbilityResourceRuntime)

namespace
{
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

bool IsGasCostedPrimaryAttackAbilitySpec(
	const FGameplayAbilitySpec* AbilitySpec)
{
	return AbilitySpecHasExactAssetTag(
			AbilitySpec,
			LabGameplayTags::Action_Attack)
		|| AbilitySpecHasExactAssetTag(
			AbilitySpec,
			LabGameplayTags::Action_Punch);
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

	const UObject* WorldContextObject =
		ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const bool bUsesActionStamina =
		SkillDataAsset
		|| IsGasCostedPrimaryAttackAbilitySpec(AbilitySpec);
	return bUsesActionStamina
		? ResolveActionStaminaCost(WorldContextObject)
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

const FGameplayTagContainer* UPdAbilityResourceRuntime::BuildCooldownTags(
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
	if (SkillDataAsset
		&& (SkillDataAsset->Time.CooldownDuration > 0.0
			|| Ability.GetCooldownGameplayEffect()))
	{
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

bool UPdAbilityResourceRuntime::CheckCost(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UPdAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
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
	if (!AbilitySystemComponent || !BasicAttributeSet)
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

void UPdAbilityResourceRuntime::ApplyCost(
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

	const UPdAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
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
	if ((ManaCost <= 0.0f && StaminaCost <= 0.0f)
		|| !AbilitySystemComponent->GetSet<UBasicAttributeSet>())
	{
		return;
	}

	if (ManaCost > 0.0f)
	{
		const float CurrentMana =
			AbilitySystemComponent->GetNumericAttribute(
				UBasicAttributeSet::GetManaAttribute());
		const float AppliedManaCost = FMath::Min(CurrentMana, ManaCost);
		if (AppliedManaCost > 0.0f)
		{
			AbilitySystemComponent->ApplyModToAttribute(
				UBasicAttributeSet::GetManaAttribute(),
				EGameplayModOp::Additive,
				-AppliedManaCost);
		}
	}

	if (StaminaCost <= 0.0f)
	{
		return;
	}

	const float CurrentStamina =
		AbilitySystemComponent->GetNumericAttribute(
			UBasicAttributeSet::GetStaminaAttribute());
	const float AppliedStaminaCost =
		FMath::Min(CurrentStamina, StaminaCost);
	if (AppliedStaminaCost > 0.0f)
	{
		AbilitySystemComponent->ApplyModToAttribute(
			UBasicAttributeSet::GetStaminaAttribute(),
			EGameplayModOp::Additive,
			-AppliedStaminaCost);
	}
}

bool UPdAbilityResourceRuntime::CheckConfiguredCooldown(
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
	const UPdAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
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

bool UPdAbilityResourceRuntime::ShouldDeferCooldown(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UPdAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
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

bool UPdAbilityResourceRuntime::ApplyConfiguredCooldownImmediately(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo,
	const FGameplayTagContainer& RemovalPolicyTags) const
{
	const UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UPdAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
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

	UGameplayEffect* CooldownGameplayEffect =
		Ability.GetCooldownGameplayEffect();
	const float ConfiguredDuration = static_cast<float>(
		FMath::Max(SkillDataAsset->Time.CooldownDuration, 0.0));
	const bool bUsingGeneratedCooldownEffect = ConfiguredDuration > 0.0f;
	const TSubclassOf<UGameplayEffect> CooldownEffectClass =
		bUsingGeneratedCooldownEffect
			? USkillCooldownGameplayEffect::StaticClass()
			: (CooldownGameplayEffect
				? CooldownGameplayEffect->GetClass()
				: nullptr);
	if (!CooldownEffectClass)
	{
		return true;
	}

	FGameplayEffectSpecHandle CooldownSpecHandle =
		Ability.MakeOutgoingGameplayEffectSpec(
			Handle,
			ActorInfo,
			ActivationInfo,
			CooldownEffectClass,
			Ability.GetAbilityLevel(Handle, ActorInfo));
	if (!CooldownSpecHandle.IsValid()
		|| !CooldownSpecHandle.Data.IsValid())
	{
		return true;
	}

	const float OriginalDuration = CooldownSpecHandle.Data->GetDuration();
	float EffectiveDuration = ConfiguredDuration > 0.0f
		? ConfiguredDuration
		: OriginalDuration;
	const float ArcaneReductionPercent =
		GetSkillCooldownReductionPercent(Ability);
	const bool bHasConfiguredCooldownDuration =
		ConfiguredDuration > 0.0f;
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

	if (bUsingGeneratedCooldownEffect
		|| bHasConfiguredCooldownDuration
		|| bHasArcaneCooldownReduction)
	{
		CooldownSpecHandle.Data->SetDuration(EffectiveDuration, true);
	}

	FGameplayTagContainer DynamicCooldownTags;
	BuildDynamicCooldownGrantedTags(Ability, DynamicCooldownTags);
	CooldownSpecHandle.Data->DynamicGrantedTags.AppendTags(
		DynamicCooldownTags);
	CooldownSpecHandle.Data->AppendDynamicAssetTags(DynamicCooldownTags);
	AppendCooldownRemovalPolicyTags(
		CooldownSpecHandle,
		RemovalPolicyTags,
		SourceRuntime && SourceRuntime->IsPandoraSkillSpec(AbilitySpec));

	Ability.ApplyGameplayEffectSpecToOwner(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownSpecHandle);
	return true;
}

void UPdAbilityResourceRuntime::AppendCooldownRemovalPolicyTags(
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

bool UPdAbilityResourceRuntime::TryCommitAdditionalActionStaminaCost(
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
		ResolveActionStaminaCost(ActorInfo->AvatarActor.Get());
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

	AbilitySystemComponent->ApplyModToAttribute(
		UBasicAttributeSet::GetStaminaAttribute(),
		EGameplayModOp::Additive,
		-ActionStaminaCost);
	return true;
}

FGameplayTag UPdAbilityResourceRuntime::GetSkillSlotCooldownTag(
	const UPdGameplayAbility& Ability) const
{
	const UPdAbilitySourceRuntime* SourceRuntime = Ability.GetSourceRuntime();
	const FGameplayAbilitySpec* AbilitySpec = SourceRuntime
		? SourceRuntime->ResolveCurrentAbilitySpec(Ability)
		: nullptr;
	return AbilitySpec
		? GetPandoraSkillCooldownTagFromSourceTags(
			AbilitySpec->GetDynamicSpecSourceTags())
		: FGameplayTag();
}

void UPdAbilityResourceRuntime::BuildDynamicCooldownGrantedTags(
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

float UPdAbilityResourceRuntime::GetSkillCooldownReductionPercent(
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

bool UPdAbilityResourceRuntime::ConsumePendingCooldown(
	const bool bAbilityWasCancelled)
{
	const bool bWasPending = bApplySkillCooldownWhenAbilityEnds;
	bApplySkillCooldownWhenAbilityEnds = false;
	return bWasPending && !bAbilityWasCancelled;
}
