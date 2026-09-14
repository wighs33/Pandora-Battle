#include "Definition/AbilitySystem/StatusEffectDefinition.h"

#include "AbilitySystemComponent.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectDefinition)

FPrimaryAssetId UStatusEffectDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("StatusEffect"), GetFName());
}

void UStatusEffectDefinition::SynchronizeStackEffectStackLimit() const
{
	UGameplayEffect* StackGameplayEffect = StackGameplayEffectClass
		? StackGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;

	if (StackGameplayEffect)
	{
		// GAS가 허용하는 최대 중첩 수를 상태 이상 발동 기준인 MaxStackCount에 맞추고, 최소 1스택을 보장한다.
		StackGameplayEffect->StackLimitCount = FMath::Max(MaxStackCount, 1);
	}
}

bool UStatusEffectDefinition::CanStack(const UAbilitySystemComponent* TargetAbilitySystemComponent) const
{
	return TargetAbilitySystemComponent
		&& (!StatusEffectTag.IsValid() || !TargetAbilitySystemComponent->HasMatchingGameplayTag(StatusEffectTag));
}

void UStatusEffectDefinition::RemoveStacks(UAbilitySystemComponent* TargetAbilitySystemComponent) const
{
	if (!TargetAbilitySystemComponent || !StackTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer DebuffTags;
	DebuffTags.AddTag(StackTag);
	// 대상에게 DebuffTag를 부여하는 활성 GameplayEffect를 모두 제거하여 해당 상태 이상의 누적 스택을 초기화한다.
	TargetAbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(DebuffTags);
}
