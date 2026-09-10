#include "Component/AbilitySystem/AbilityAttributeManager.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Definition/Player/StatUpgradeDefinition.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityAttributeManager)

// 속성 태그 연결 설정을 등록하고 해제할 때 사용할 핸들을 반환한다. 나중에 등록한 설정이 우선한다.
int32 UAbilityAttributeManager::AddAttributeConfig(const FAttributeConfig& AttributeConfig)
{
	if (!AttributeConfig.HasAnyData())
	{
		return INDEX_NONE;
	}

	const int32 AttributeConfigHandle = NextAttributeConfigHandle++;
	ActiveAttributeConfigs.Add(FRegisteredAttributeConfig{AttributeConfigHandle, AttributeConfig});
	return AttributeConfigHandle;
}

// 해당 핸들의 설정만 제거한다. 다른 설정의 우선순위와 이미 적용된 능력치 값은 유지한다.
void UAbilityAttributeManager::RemoveAttributeConfig(const int32 AttributeConfigHandle)
{
	if (AttributeConfigHandle == INDEX_NONE)
	{
		return;
	}

	ActiveAttributeConfigs.RemoveAll(
		[AttributeConfigHandle](const FRegisteredAttributeConfig& Entry) { return Entry.Handle == AttributeConfigHandle; });
}

// 최근 등록한 설정부터 검색해 스탯 태그에 연결된 실제 GAS 속성을 찾는다.
bool UAbilityAttributeManager::ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const
{
	OutAttribute = FGameplayAttribute();
	if (!StatTag.IsValid())
	{
		return false;
	}

	for (int32 Index = ActiveAttributeConfigs.Num() - 1; Index >= 0; --Index)
	{
		for (const FAttributeTagMapping& Entry : ActiveAttributeConfigs[Index].Config.AttributeMappings)
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

// 서버에서 기본값·시작 투자분을 검증한 뒤 최대 자원, 현재 자원 순서로 초기화한다. 같은 대상·정의는 중복 적용하지 않는다.
bool UAbilityAttributeManager::ApplyConfiguredAttributeDefaults(UPdAbilitySystemComponent& ASC, const UStatUpgradeDefinition& Definition)
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

	// 1. 우선순위대로 기본값을 모은다. 같은 태그는 나중 값으로 덮어쓰고 적용 순서는 유지한다.
	TArray<FStatAttributeDefaultValue> OrderedDefaults = Definition.GetAttributeDefaultValues();
	OrderedDefaults.StableSort(
		[](const FStatAttributeDefaultValue& Left, const FStatAttributeDefaultValue& Right) { return Left.Priority < Right.Priority; });
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

	// 2. 시작부터 투자된 레벨이 있다면 같은 투자 공식으로 기본값에 투자분을 더한다.
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

	// 3. 현재 자원 태그와, 별도 초기값이 없어 최대값으로 채울 자원 쌍을 찾는다.
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
		if (!ASC.ResolveAttributeFromTag(Pair.MaxStatTag, MaxAttribute) || !ASC.ResolveAttributeFromTag(Pair.CurrentStatTag, CurrentAttribute)
			|| !ASC.HasAttributeSetForAttribute(MaxAttribute) || !ASC.HasAttributeSetForAttribute(CurrentAttribute))
		{
			return false;
		}
		PairedResources.Emplace(MaxAttribute, CurrentAttribute);
	}

	// 4. 변경 전에 모든 태그와 값을 검증한다. 최대값을 먼저 설정해 현재 자원이 이전 최대값으로 잘리지 않게 한다.
	OrderedTags.StableSort([&CurrentResourceTags](FGameplayTag Left, FGameplayTag Right) {
		return !CurrentResourceTags.Contains(Left) && CurrentResourceTags.Contains(Right);
	});
	TArray<TPair<FGameplayAttribute, float>> ResolvedDefaults;
	// 기능별 연결이 없어도 BasicAttributeSet의 기본 연결로 초기화할 수 있도록 ASC에서 조회한다.
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

	// 5. 속성 변경 알림의 재진입을 막도록 먼저 기록한 뒤 GAS에 값을 적용한다.
	ConfiguredDefaultsAttributeSet = AttributeSet;
	ConfiguredDefaultsDefinitionPath = DefinitionPath;
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
		if (!ApplyAttributeDefaultValue(ASC, Pair.Value, ASC.GetNumericAttribute(Pair.Key)))
		{
			ClearConfiguredAttributeDefaultsApplied(AttributeSet, DefinitionPath);
			return false;
		}
	}
	return true;
}

// 서버에서 GAS 기본값을 변경하고 Push Model Dirty 표시와 소유 액터의 복제 갱신을 요청한다.
bool UAbilityAttributeManager::ApplyAttributeDefaultValue(
	UPdAbilitySystemComponent& AbilitySystemComponent, const FGameplayAttribute& Attribute, const float DefaultValue) const
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

// 현재 AttributeSet과 설정 정의가 이미 초기화한 조합인지 확인해 중복·재진입 적용을 막는다.
bool UAbilityAttributeManager::HasAppliedConfiguredAttributeDefaults(
	const UAttributeSet* AttributeSet, const FSoftObjectPath& DefinitionPath) const
{
	return AttributeSet && !DefinitionPath.IsNull() && ConfiguredDefaultsAttributeSet.Get() == AttributeSet
		&& ConfiguredDefaultsDefinitionPath == DefinitionPath;
}

// 실패한 초기화의 기록만 비운다. 콜백에서 새로 적용한 다른 설정의 기록은 보존한다.
void UAbilityAttributeManager::ClearConfiguredAttributeDefaultsApplied(
	const UAttributeSet* AttributeSet, const FSoftObjectPath& DefinitionPath)
{
	// 이전 설정의 해제 요청이 새로 적용한 설정의 기록을 지우지 않도록 한다.
	if (!HasAppliedConfiguredAttributeDefaults(AttributeSet, DefinitionPath))
	{
		return;
	}

	ConfiguredDefaultsAttributeSet.Reset();
	ConfiguredDefaultsDefinitionPath.Reset();
}

// 유효한 스탯 변화량과 연산 종류를 SetByCaller에 담아 서버에서 능력치 효과를 적용한다.
bool UAbilityAttributeManager::ApplyStatUpEffectByTags(UPdAbilitySystemComponent& AbilitySystemComponent,
	TSubclassOf<UGameplayEffect> GameplayEffectClass, const TMap<FGameplayTag, float>& StatMagnitudes, const EEnum_Operation Operation,
	const float Level) const
{
	if (!AbilitySystemComponent.IsOwnerActorAuthoritative())
	{
		return false;
	}

	FGameplayTag OperationSetByCallerTag;
	if (!GameplayEffectClass || StatMagnitudes.IsEmpty()
		|| !ResolveStatUpOperationSetByCallerTag(AbilitySystemComponent, OperationSetByCallerTag))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent.MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent.MakeOutgoingSpec(GameplayEffectClass, Level, EffectContext);
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

	SpecHandle.Data->SetSetByCallerMagnitude(OperationSetByCallerTag, static_cast<float>(Operation));

	const FActiveGameplayEffectHandle AppliedHandle = AbilitySystemComponent.ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return AppliedHandle.WasSuccessfullyApplied();
}

// 프로젝트 태그 설정에서 피해량을 전달할 SetByCaller 태그를 찾는다.
bool UAbilityAttributeManager::ResolveDamageMagnitudeSetByCallerTag(
	const UPdAbilitySystemComponent& AbilitySystemComponent, FGameplayTag& OutTag) const
{
	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(&AbilitySystemComponent);
	OutTag = TagConfig ? TagConfig->GetSetByCallerDamageMagnitudeTag() : FGameplayTag();
	return OutTag.IsValid();
}

// 프로젝트 태그 설정에서 스탯 연산 종류를 전달할 SetByCaller 태그를 찾는다.
bool UAbilityAttributeManager::ResolveStatUpOperationSetByCallerTag(
	const UPdAbilitySystemComponent& AbilitySystemComponent, FGameplayTag& OutTag) const
{
	const UProjectTagConfig* TagConfig = UProjectTagConfig::Get(&AbilitySystemComponent);
	OutTag = TagConfig ? TagConfig->GetSetByCallerStatUpOperationTag() : FGameplayTag();
	return OutTag.IsValid();
}
