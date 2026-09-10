#include "Component/AbilitySystem/AbilityAttributeManager.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Definition/Player/StatUpgradeDefinition.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityAttributeManager)

int32 UAbilityAttributeManager::AddAttributeConfig(const FAttributeConfig& AttributeConfig)
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

void UAbilityAttributeManager::RemoveAttributeConfig(const int32 AttributeConfigHandle)
{
	if (AttributeConfigHandle == INDEX_NONE)
	{
		return;
	}

	ActiveAttributeConfigs.Remove(AttributeConfigHandle);
	AttributeConfigOrder.Remove(AttributeConfigHandle);
}

bool UAbilityAttributeManager::HasAppliedConfiguredAttributeDefaults(
	const UAttributeSet* AttributeSet,
	const FSoftObjectPath& DefinitionPath) const
{
	return AttributeSet
		&& !DefinitionPath.IsNull()
		&& ConfiguredDefaultsAttributeSet.Get() == AttributeSet
		&& ConfiguredDefaultsDefinitionPath == DefinitionPath;
}

void UAbilityAttributeManager::MarkConfiguredAttributeDefaultsApplied(
	UAttributeSet* AttributeSet,
	const FSoftObjectPath& DefinitionPath)
{
	ConfiguredDefaultsAttributeSet = AttributeSet;
	ConfiguredDefaultsDefinitionPath = DefinitionPath;
}

void UAbilityAttributeManager::ClearConfiguredAttributeDefaultsApplied(
	const UAttributeSet* AttributeSet,
	const FSoftObjectPath& DefinitionPath)
{
	// 이전 설정의 해제 요청이 새로 적용한 설정의 기록을 지우지 않도록 한다.
	if (!HasAppliedConfiguredAttributeDefaults(AttributeSet, DefinitionPath))
	{
		return;
	}

	ConfiguredDefaultsAttributeSet.Reset();
	ConfiguredDefaultsDefinitionPath.Reset();
}

// 초기 능력치와 시작 투자분을 한 번만 설정한다. 최종값 계산과 변경 알림은 GAS에 맡긴다.
bool UAbilityAttributeManager::ApplyConfiguredAttributeDefaults(
	UPdAbilitySystemComponent& ASC, const UStatUpgradeDefinition& Definition)
{
	UAttributeSet* AttributeSet = const_cast<UBasicAttributeSet*>(ASC.GetSet<UBasicAttributeSet>());
	const FSoftObjectPath DefinitionPath(Definition.GetPathName());
	if (!ASC.IsOwnerActorAuthoritative() || !AttributeSet || Definition.GetAttributeDefaultValues().IsEmpty())
	{
		return false;
	}
	if (HasAppliedConfiguredAttributeDefaults(AttributeSet, DefinitionPath))
	{
		return true;
	}

	TArray<FStatAttributeDefaultValue> OrderedDefaults = Definition.GetAttributeDefaultValues();
	OrderedDefaults.StableSort([](const FStatAttributeDefaultValue& Left, const FStatAttributeDefaultValue& Right)
	{
		return Left.Priority < Right.Priority;
	});
	TMap<FGameplayTag, float> InitialValues;
	TArray<FGameplayTag> OrderedTags;
	for (const FStatAttributeDefaultValue& Entry : OrderedDefaults)
	{
		if (Entry.IsValid())
		{
			InitialValues.Add(Entry.StatTag, Entry.DefaultValue);
			OrderedTags.AddUnique(Entry.StatTag);
		}
	}

	// 시작부터 투자된 레벨이 있다면 같은 투자 공식으로 기본값에 투자분을 더한다.
	for (const FStatUpgradeBinding& Binding : UStatUpgradeDefinition::GetStatBindings())
	{
		const float* ConfiguredLevel = InitialValues.Find(Binding.LevelTag);
		if (!Binding.bCompounded || !ConfiguredLevel || *ConfiguredLevel == 0.f)
		{
			continue;
		}
		float Magnitude = 0.f;
		if (!FMath::IsFinite(*ConfiguredLevel) || *ConfiguredLevel < 0.f
			|| *ConfiguredLevel > FMath::FloorToFloat(Definition.GetMaxInvestedLevel())
			|| !Definition.TryGetUpgradeMagnitude(Binding, Magnitude))
		{
			return false;
		}
		const float Investment = UStatUpgradeDefinition::CalculateInvestmentValue(Magnitude, *ConfiguredLevel, true);
		InitialValues.FindOrAdd(Binding.GetEffectTag()) += Investment;
		OrderedTags.AddUnique(Binding.GetEffectTag());
		if (Binding.IsMaxResource())
		{
			float BaseValue = 0.f;
			if (!Definition.TryGetResourceBaseValue(Binding, BaseValue))
			{
				return false;
			}
			InitialValues.Add(Binding.StatTag, BaseValue * (1.f + Investment * 0.01f));
			OrderedTags.AddUnique(Binding.StatTag);
		}
	}

	TSet<FGameplayTag> CurrentResourceTags;
	for (const FStatUpgradeBinding& Binding : UStatUpgradeDefinition::GetStatBindings())
	{
		if (Binding.IsMaxResource())
		{
			CurrentResourceTags.Add(Binding.CurrentResourceTag);
		}
	}
	TArray<TPair<FGameplayAttribute, FGameplayAttribute>> PairedResources;
	for (const FPairedResourceStatTag& Pair : Definition.GetPairedResourceStatTags())
	{
		if (!Pair.IsValid())
		{
			continue;
		}
		CurrentResourceTags.Add(Pair.CurrentStatTag);
		if (InitialValues.Contains(Pair.CurrentStatTag))
		{
			continue;
		}
		FGameplayAttribute MaxAttribute;
		FGameplayAttribute CurrentAttribute;
		if (!ASC.ResolveAttributeFromTag(Pair.MaxStatTag, MaxAttribute)
			|| !ASC.ResolveAttributeFromTag(Pair.CurrentStatTag, CurrentAttribute)
			|| !ASC.HasAttributeSetForAttribute(MaxAttribute) || !ASC.HasAttributeSetForAttribute(CurrentAttribute))
		{
			return false;
		}
		PairedResources.Emplace(MaxAttribute, CurrentAttribute);
	}

	// 최대값을 먼저 설정해야 현재 체력·마나가 이전 최대값으로 잘리지 않는다.
	OrderedTags.StableSort([&CurrentResourceTags](FGameplayTag Left, FGameplayTag Right)
	{
		return !CurrentResourceTags.Contains(Left) && CurrentResourceTags.Contains(Right);
	});
	TArray<TPair<FGameplayAttribute, float>> ResolvedDefaults;
	for (FGameplayTag Tag : OrderedTags)
	{
		FGameplayAttribute Attribute;
		const float Value = InitialValues.FindChecked(Tag);
		if (!FMath::IsFinite(Value) || !ASC.ResolveAttributeFromTag(Tag, Attribute) || !ASC.HasAttributeSetForAttribute(Attribute))
		{
			return false;
		}
		ResolvedDefaults.Emplace(Attribute, Value);
	}
	if (ResolvedDefaults.IsEmpty())
	{
		return false;
	}

	// 속성 변경 알림을 통한 재진입에서도 초기화가 중복되지 않도록 먼저 기록한다.
	MarkConfiguredAttributeDefaultsApplied(AttributeSet, DefinitionPath);
	for (const TPair<FGameplayAttribute, float>& Entry : ResolvedDefaults)
	{
		if (!ApplyAttributeDefaultValue(ASC, Entry.Key, Entry.Value))
		{
			ClearConfiguredAttributeDefaultsApplied(AttributeSet, DefinitionPath);
			return false;
		}
	}
	for (const TPair<FGameplayAttribute, FGameplayAttribute>& Pair : PairedResources)
	{
		ApplyAttributeDefaultValue(ASC, Pair.Value, ASC.GetNumericAttribute(Pair.Key));
	}
	return true;
}

bool UAbilityAttributeManager::ApplyAttributeDefaultValue(
	UPdAbilitySystemComponent& AbilitySystemComponent,
	const FGameplayAttribute& Attribute,
	const float DefaultValue) const
{
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative() || !Attribute.IsValid() || !FMath::IsFinite(DefaultValue))
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

	MARK_PROPERTY_DIRTY(AttributeSet, Property);

	if (AActor* OwningActor = AbilitySystemComponent.GetOwner())
	{
		OwningActor->ForceNetUpdate();
	}

	return true;
}

bool UAbilityAttributeManager::ApplyStatUpEffectByTags(
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

bool UAbilityAttributeManager::ResolveAttributeFromTag(
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

bool UAbilityAttributeManager::ResolveDamageMagnitudeSetByCallerTag(
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

bool UAbilityAttributeManager::ResolveStatUpOperationSetByCallerTag(
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
