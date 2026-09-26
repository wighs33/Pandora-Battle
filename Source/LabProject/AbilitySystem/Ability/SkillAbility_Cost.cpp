#include "AbilitySystem/Ability/SkillAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "Pandora/PandoraSkillSource.h"

const FGameplayTagContainer* USkillAbility::GetCooldownTags() const
{
	const USkillDefinition* SkillDefinition = IsInstantiated() ? GetSourceSkillDataAsset() : nullptr;
	if (!SkillDefinition)
	{
		const FGameplayTagContainer* ParentCooldownTags = Super::GetCooldownTags();
		return ParentCooldownTags && !ParentCooldownTags->IsEmpty() ? ParentCooldownTags : nullptr;
	}

	const float BaseDuration = static_cast<float>(FMath::Max(SkillDefinition->Time.CooldownDuration, 0.0));
	static const FGameplayTagContainer SkillCooldownTags(LabGameplayTags::Cooldown);
	return BaseDuration > 0.0f ? &SkillCooldownTags : nullptr;
}

float USkillAbility::GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const
{
	float Remaining = 0.0f;
	float Duration = 0.0f;
	GetCooldownTimeRemainingAndDuration(GetCurrentAbilitySpecHandle(), ActorInfo, Remaining, Duration);
	return Remaining;
}

void USkillAbility::GetCooldownTimeRemainingAndDuration(
	FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& TimeRemaining, float& CooldownDuration) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	const UPandoraSkillSource* Source = Spec ? Cast<UPandoraSkillSource>(Spec->SourceObject.Get()) : nullptr;
	if (Source)
	{
		Source->GetCooldownTimeRemainingAndDuration(TimeRemaining, CooldownDuration);
		return;
	}
	if (Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora))
	{
		TimeRemaining = 0.0f;
		CooldownDuration = 0.0f;
		return;
	}
	Super::GetCooldownTimeRemainingAndDuration(Handle, ActorInfo, TimeRemaining, CooldownDuration);
}

bool USkillAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent && Handle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
		: GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset = ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	if (!AbilitySystemComponent || !SkillDataAsset)
	{
		return Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
	}

	const float BaseDuration = static_cast<float>(FMath::Max(SkillDataAsset->Time.CooldownDuration, 0.0));
	if (BaseDuration <= 0.0f)
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
		Source->GetCooldownTimeRemainingAndDuration(Remaining, Duration);
		bOnCooldown = Remaining > 0.0f;
	}
	else
	{
		FGameplayTagContainer CooldownTags(DefaultCooldownTag);
		const FGameplayTagContainer* ParentCooldownTags = Super::GetCooldownTags();
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
		const FGameplayTag& FailCooldownTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
		OptionalRelevantTags->AddTag(FailCooldownTag.IsValid() ? FailCooldownTag : DefaultCooldownTag);
	}

	return false;
}

void USkillAbility::ApplySkillCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent && Handle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
		: GetCurrentAbilitySpec();
	const USkillDefinition* SkillDataAsset = ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	if (!SkillDataAsset)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
		return;
	}

	// 스킬 쿨다운은 서버에서만 적용하고, 클라이언트는 복제된 효과를 조회한다.
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsOwnerActorAuthoritative())
	{
		return;
	}

	const float BaseDuration = static_cast<float>(FMath::Max(SkillDataAsset->Time.CooldownDuration, 0.0));
	if (BaseDuration <= 0.0f)
	{
		return;
	}
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent->GetSet<UBasicAttributeSet>();
	const float EffectiveDuration = AttributeSet ? AttributeSet->CalculateCooldownDuration(BaseDuration) : BaseDuration;

	if (EffectiveDuration <= 0.0f)
	{
		return;
	}

	const FGameplayTagContainer DynamicCooldownTags(LabGameplayTags::Cooldown);
	ApplySharedCooldownEffect(Handle, ActorInfo, ActivationInfo, EffectiveDuration, DynamicCooldownTags);
	return;
}

void USkillAbility::GetResourceCosts(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, float& ManaCost, float& StaminaCost) const
{
	Super::GetResourceCosts(Handle, ActorInfo, ManaCost, StaminaCost);
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC && Handle.IsValid() ? ASC->FindAbilitySpecFromHandle(Handle) : GetCurrentAbilitySpec();
	const USkillDefinition* Skill = ResolveSourceSkillDataAsset(Spec ? Spec->SourceObject.Get() : nullptr);
	if (!Skill) return;
	ManaCost = static_cast<float>(FMath::Max(Skill->ManaCost, 0.0));
	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	StaminaCost = AvatarPawn && AvatarPawn->IsPlayerControlled() ? GetDefaultActionStaminaCost(AvatarPawn) : 0.0f;
}
