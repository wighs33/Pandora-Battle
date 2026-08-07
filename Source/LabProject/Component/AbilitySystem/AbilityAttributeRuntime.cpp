#include "Component/AbilitySystem/AbilityAttributeRuntime.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityAttributeRuntime)

namespace
{
bool SetAttributeDataDefaultValue(
	UAttributeSet* AttributeSet,
	const FGameplayAttribute& Attribute,
	const float DefaultValue)
{
	if (!AttributeSet || !Attribute.IsValid())
	{
		return false;
	}

	if (FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(AttributeSet))
	{
		AttributeData->SetBaseValue(DefaultValue);
		AttributeData->SetCurrentValue(DefaultValue);
		return true;
	}

	float NewValue = DefaultValue;
	Attribute.SetNumericValueChecked(NewValue, AttributeSet);
	return true;
}
}

int32 UAbilityAttributeRuntime::AddAttributeConfig(const FAttributeConfig& AttributeConfig)
{
	if (!AttributeConfig.HasAnyData())
	{
		return INDEX_NONE;
	}

	const int32 AttributeConfigHandle = NextAttributeConfigHandle++;
	ActiveAttributeConfigs.Add(AttributeConfigHandle, AttributeConfig);
	AttributeConfigOrder.Add(AttributeConfigHandle);
	return AttributeConfigHandle;
}

void UAbilityAttributeRuntime::RemoveAttributeConfig(const int32 AttributeConfigHandle)
{
	if (AttributeConfigHandle == INDEX_NONE)
	{
		return;
	}

	ActiveAttributeConfigs.Remove(AttributeConfigHandle);
	AttributeConfigOrder.Remove(AttributeConfigHandle);
}

bool UAbilityAttributeRuntime::ApplyAttributeDefaultValue(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayAttribute& Attribute,
	const float DefaultValue) const
{
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative() || !Attribute.IsValid())
	{
		return false;
	}

	const TSubclassOf<UAttributeSet> AttributeSetClass = const_cast<UClass*>(Attribute.GetAttributeSetClass());
	if (!AttributeSetClass)
	{
		return false;
	}

	UAttributeSet* AttributeSet = const_cast<UAttributeSet*>(AbilitySystemComponent.GetAttributeSet(AttributeSetClass));
	FProperty* Property = Attribute.GetUProperty();
	if (!AttributeSet || !Property)
	{
		return false;
	}

	AbilitySystemComponent.SetNumericAttributeBase(Attribute, DefaultValue);
	if (!SetAttributeDataDefaultValue(AttributeSet, Attribute, DefaultValue))
	{
		return false;
	}

	MARK_PROPERTY_DIRTY(AttributeSet, Property);

	if (AActor* OwningActor = AbilitySystemComponent.GetOwner())
	{
		OwningActor->ForceNetUpdate();
	}

	return true;
}

bool UAbilityAttributeRuntime::ApplyStatUpEffectByTag(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const FGameplayTag StatTag,
	const float Magnitude,
	const EEnum_Operation Operation,
	const float Level) const
{
	if (!StatTag.IsValid() || FMath::IsNearlyZero(Magnitude))
	{
		return false;
	}

	TMap<FGameplayTag, float> StatMagnitudes;
	StatMagnitudes.Add(StatTag, Magnitude);
	return ApplyStatUpEffectByTags(
		AbilitySystemComponent,
		GameplayEffectClass,
		StatMagnitudes,
		Operation,
		Level);
}

bool UAbilityAttributeRuntime::ApplyStatUpEffectByTags(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const TMap<FGameplayTag, float>& StatMagnitudes,
	const EEnum_Operation Operation,
	const float Level) const
{
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative())
	{
		return false;
	}

	FGameplayTag OperationSetByCallerTag;
	if (!GameplayEffectClass
		|| StatMagnitudes.IsEmpty()
		|| !ResolveStatUpOperationSetByCallerTag(AbilitySystemComponent, OperationSetByCallerTag))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent.MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle =
		AbilitySystemComponent.MakeOutgoingSpec(GameplayEffectClass, Level, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	bool bAddedAnyMagnitude = false;
	for (const TPair<FGameplayTag, float>& Pair : StatMagnitudes)
	{
		if (!Pair.Key.IsValid() || FMath::IsNearlyZero(Pair.Value))
		{
			continue;
		}

		SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
		bAddedAnyMagnitude = true;
	}

	if (!bAddedAnyMagnitude)
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		OperationSetByCallerTag,
		static_cast<float>(Operation));

	const FActiveGameplayEffectHandle AppliedHandle =
		AbilitySystemComponent.ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return AppliedHandle.WasSuccessfullyApplied();
}

bool UAbilityAttributeRuntime::ResolveAttributeFromTag(
	const FGameplayTag& StatTag,
	FGameplayAttribute& OutAttribute) const
{
	OutAttribute = FGameplayAttribute();
	if (!StatTag.IsValid())
	{
		return false;
	}

	for (int32 Index = AttributeConfigOrder.Num() - 1; Index >= 0; --Index)
	{
		const FAttributeConfig* AttributeConfig =
			ActiveAttributeConfigs.Find(AttributeConfigOrder[Index]);
		if (!AttributeConfig)
		{
			continue;
		}

		for (const FAttributeTagMapping& Entry : AttributeConfig->AttributeMappings)
		{
			if (Entry.StatTag.MatchesTagExact(StatTag) && Entry.Attribute.IsValid())
			{
				OutAttribute = Entry.Attribute;
				return true;
			}
		}
	}

	return false;
}

bool UAbilityAttributeRuntime::ResolveDamageMagnitudeSetByCallerTag(
	const UPdAbilitySystemComponent& AbilitySystemComponent,
	FGameplayTag& OutTag) const
{
	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(&AbilitySystemComponent);
	if (!TagConfig)
	{
		OutTag = FGameplayTag();
		return false;
	}

	OutTag = TagConfig->GetSetByCallerDamageMagnitudeTag();
	return OutTag.IsValid();
}

bool UAbilityAttributeRuntime::ResolveStatUpOperationSetByCallerTag(
	const UPdAbilitySystemComponent& AbilitySystemComponent,
	FGameplayTag& OutTag) const
{
	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(&AbilitySystemComponent);
	if (!TagConfig)
	{
		OutTag = FGameplayTag();
		return false;
	}

	OutTag = TagConfig->GetSetByCallerStatUpOperationTag();
	return OutTag.IsValid();
}
