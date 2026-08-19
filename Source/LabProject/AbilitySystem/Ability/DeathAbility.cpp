#include "AbilitySystem/Ability/DeathAbility.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include UE_INLINE_GENERATED_CPP_BY_NAME(DeathAbility)

UDeathAbility::UDeathAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::GameplayAbility_Death);
	SetAssetTags(AbilityAssetTags);

	CancelAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_Attack);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_Punch);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_RangedAttack);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_HitReact);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_Equip);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_Unequip);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Attack);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Punch);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_RangedAttack);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_HitReact);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Equip);
	BlockAbilitiesWithTag.AddTag(LabGameplayTags::Action_Unequip);

}

#if WITH_EDITOR
EDataValidationResult UDeathAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!DeathEffectClass)
	{
		Context.AddError(NSLOCTEXT(
			"DeathAbility",
			"MissingDeathEffect",
			"DeathEffectClass must be configured on the Death Gameplay Ability asset."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

bool UDeathAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	static_cast<void>(Handle);
	static_cast<void>(SourceTags);
	static_cast<void>(TargetTags);
	static_cast<void>(OptionalRelevantTags);

	// Death is a terminal state transition and must not be rejected by an
	// active skill's general GameplayAbility block tag.
	const UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	return AbilitySystemComponent
		&& AbilitySystemComponent->GetNumericAttribute(
			UBasicAttributeSet::GetHealthAttribute()) <= 0.0f
		&& !AbilitySystemComponent->HasMatchingGameplayTag(
			LabGameplayTags::State_Dead);
}

void UDeathAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);

	if (!HasAuthority(&ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!DeathEffectClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	BP_ApplyGameplayEffectToOwner(DeathEffectClass, FMath::Max(GetAbilityLevel(), 1), 1);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
