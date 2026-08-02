#include "AbilitySystem/Ability/DeathAbility.h"

#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"
#include "UObject/ConstructorHelpers.h"
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

	static ConstructorHelpers::FClassFinder<UGameplayEffect> DeathEffectFinder(TEXT("/Game/GAS/Effect/GE_Death"));
	if (DeathEffectFinder.Succeeded())
	{
		DeathEffectClass = DeathEffectFinder.Class;
	}
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
