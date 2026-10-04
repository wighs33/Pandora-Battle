#include "Definition/AbilitySystem/StatusEffectDefinition.h"

#include "AbilitySystemComponent.h"
#include "Common/LabGameplayTags.h"
#include "GameplayEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectDefinition)

FPrimaryAssetId UStatusEffectDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("StatusEffect"), GetFName());
}

#if WITH_EDITOR
// 스택 GE의 StackLimitCount가 발동 기준과 다르면 GAS가 스택을 먼저 잘라 상태 이상이 발동하지 않으므로 저장 전에 막는다.
EDataValidationResult UStatusEffectDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const UGameplayEffect* StackGameplayEffect = StackGameplayEffectClass
		? StackGameplayEffectClass->GetDefaultObject<UGameplayEffect>()
		: nullptr;
	if (!StackGameplayEffect)
	{
		Context.AddError(NSLOCTEXT("StatusEffectDefinition", "MissingStackEffect",
			"Debuff Gameplay Effect Class is required to accumulate status effect stacks."));
		return EDataValidationResult::Invalid;
	}

	if (StackGameplayEffect->GetStackLimitCount() != MaxStackCount)
	{
		Context.AddError(FText::Format(NSLOCTEXT("StatusEffectDefinition", "StackLimitMismatch",
			"{0} StackLimitCount is {1}, but MaxStackCount is {2}. Set them to the same value."),
			FText::FromString(StackGameplayEffectClass->GetName()),
			FText::AsNumber(StackGameplayEffect->GetStackLimitCount()), FText::AsNumber(MaxStackCount)));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif

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
