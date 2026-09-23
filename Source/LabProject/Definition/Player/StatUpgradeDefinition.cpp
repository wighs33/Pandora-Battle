#include "Definition/Player/StatUpgradeDefinition.h"

#include "Common/LabGameplayTags.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatUpgradeDefinition)

DEFINE_LOG_CATEGORY_STATIC(StatUpgradeDefinitionLog, Log, All);

namespace
{
    constexpr float MaxSupportedInvestedLevel = 100.f;

#if WITH_EDITOR
    // 에디터에서 스탯 설정 오류를 발견했을 때 해당 DataAsset을 유효하지 않은 설정으로 표시한다.
    void MarkStatUpgradeInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
    {
       Result = EDataValidationResult::Invalid;
       Context.AddError(Message);
    }

    // 스탯 설정값에 NaN/무한대가 들어가 런타임 능력치 계산이 깨지는 것을 에디터 검증 단계에서 판별한다.
    bool IsFinite(const float Value)
    {
       return FMath::IsFinite(Value);
    }

    // 기본 능력치나 성장 수치가 실제 계산 가능한 유한값인지 에디터에서 검사한다.
    void ValidateFinite(
       FDataValidationContext& Context,
       EDataValidationResult& Result,
       const float Value,
       const FText& FieldName)
    {
       if (!IsFinite(Value))
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "NonFiniteValue", "{0} must be a finite value."),
             FieldName));
       }
    }

    // 비용처럼 0 이상이어야 하는 스탯 설정값이 음수로 잘못 입력되지 않았는지 검사한다.
    void ValidateNonNegative(
       FDataValidationContext& Context,
       EDataValidationResult& Result,
       const float Value,
       const FText& FieldName)
    {
       if (!IsFinite(Value) || Value < 0.f)
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "InvalidNonNegativeValue", "{0} must be a non-negative finite value."),
             FieldName));
       }
    }

    // 최대 투자 레벨처럼 반드시 0보다 커야 하는 설정값이 올바른지 검사한다.
    void ValidatePositive(
       FDataValidationContext& Context,
       EDataValidationResult& Result,
       const float Value,
       const FText& FieldName)
    {
       if (!IsFinite(Value) || Value <= 0.f)
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "InvalidPositiveValue", "{0} must be a positive finite value."),
             FieldName));
       }
    }

    // 설정값이 프로젝트에서 지원하는 상한선을 넘지 않도록 에디터에서 검사한다.
    void ValidateLessOrEqual(
       FDataValidationContext& Context,
       EDataValidationResult& Result,
       const float Value,
       const float MaxValue,
       const FText& FieldName)
    {
       if (IsFinite(Value) && Value > MaxValue)
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "ValueAboveMaximum", "{0} must be less than or equal to {1}."),
             FieldName,
             FText::AsNumber(MaxValue)));
       }
    }
#endif

    // 특정 스탯 태그와 정확히 일치하는 기본값 설정을 찾아, 개별 스탯 전용 값을 우선 적용할 때 사용한다.
    const FStatAttributeDefaultValue* FindExactAttributeValue(
       const TArray<FStatAttributeDefaultValue>& AttributeValues,
       const FGameplayTag& StatTag)
    {
       for (const FStatAttributeDefaultValue& AttributeValue : AttributeValues)
       {
          if (AttributeValue.StatTag.MatchesTagExact(StatTag))
          {
             return &AttributeValue;
          }
       }

       return nullptr;
    }

    // 정확한 스탯 설정이 없으면 상위 카테고리 설정까지 찾아, 공통 성장 규칙을 여러 하위 스탯에 적용한다.
    const FStatAttributeDefaultValue* FindMatchingAttributeValue(
       const TArray<FStatAttributeDefaultValue>& AttributeValues,
       const FGameplayTag& StatTag)
    {
       if (const FStatAttributeDefaultValue* ExactValue = FindExactAttributeValue(AttributeValues, StatTag))
       {
          return ExactValue;
       }

       for (const FStatAttributeDefaultValue& AttributeValue : AttributeValues)
       {
          if (AttributeValue.StatTag.IsValid() && StatTag.MatchesTag(AttributeValue.StatTag))
          {
             return &AttributeValue;
          }
       }

       return nullptr;
    }

    // 스탯 태그가 속한 업그레이드 카테고리의 비용·재화 규칙을 찾아 실제 스탯 투자에 사용한다.
    const FStatUpgradeRule* FindMatchingUpgradeRule(
       const TArray<FStatUpgradeRule>& UpgradeRules,
       const FGameplayTag& StatTag)
    {
       for (const FStatUpgradeRule& Rule : UpgradeRules)
       {
          if (Rule.RootTag.IsValid() && StatTag.MatchesTag(Rule.RootTag))
          {
             return &Rule;
          }
       }

       return nullptr;
    }
}

// 플레이어 스탯의 기본값·성장량·투자 비용 규칙을 담는 설정 에셋의 기본 생성자다.
UStatUpgradeDefinition::UStatUpgradeDefinition()
{
}

// AssetManager가 이 스탯 설정 에셋을 고유하게 식별하고 로드할 수 있도록 Primary Asset ID를 제공한다.
FPrimaryAssetId UStatUpgradeDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("StatUpgradeDefinition"), GetFName());
}

// 게임 시작 시 사용할 기본 스탯 설정 에셋의 경로를 GameInstance 설정에서 가져온다.
FSoftObjectPath UStatUpgradeDefinition::GetDefaultDefinitionPath()
{
    return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
       .StatUpgrade.ToSoftObjectPath();
}

// 플레이어가 특정 스탯에 포인트를 투자할 때 적용할 비용·재화 규칙을 찾아준다.
const FStatUpgradeRule* UStatUpgradeDefinition::FindUpgradeRuleForStat(const FGameplayTag& StatTag) const
{
    return StatTag.IsValid() ? FindMatchingUpgradeRule(UpgradeRules, StatTag) : nullptr;
}

// 캐릭터 초기화 시 적용할 실제 시작 능력치를 계산하고, 별도 시작값이 없는 HP·MP·스태미나 등은 최대치로 채우도록 목록을 만든다.
bool UStatUpgradeDefinition::CalculateInitialAttributeValues(TArray<TPair<FGameplayTag, float>>& OutValues,
    TArray<FPairedResourceStatTag>& OutResourcesToFill) const
{
    OutValues.Reset();
    OutResourcesToFill.Reset();

    // 같은 태그는 우선순위가 뒤인 값으로 덮어쓰고, 처음 등장한 순서는 유지한다.
    TArray<FStatAttributeDefaultValue> OrderedDefaults = AttributeDefaultValues;
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
    if (OrderedTags.IsEmpty())
    {
       return false;
    }

    // 시작 투자분도 투자·환불과 같은 공식을 사용한다.
    TSet<FGameplayTag> CurrentResourceTags;
    for (const FStatUpgradeBinding& Binding : GetStatBindings())
    {
       if (Binding.IsMaxResource())
       {
          CurrentResourceTags.Add(Binding.CurrentResourceTag);
       }
       const float* ConfiguredLevel = InitialValues.Find(Binding.LevelTag);
       if (!Binding.bCompounded || !ConfiguredLevel || *ConfiguredLevel == 0.f)
       {
          continue;
       }
       float Magnitude = 0.f;
       if (!FMath::IsFinite(*ConfiguredLevel) || *ConfiguredLevel < 0.f
          || *ConfiguredLevel > FMath::FloorToFloat(GetMaxInvestedLevel())
          || !TryGetUpgradeMagnitude(Binding, Magnitude))
       {
          return false;
       }
       const float Investment = CalculateInvestmentValue(Magnitude, *ConfiguredLevel, true);
       InitialValues.FindOrAdd(Binding.GetEffectTag()) += Investment;
       OrderedTags.AddUnique(Binding.GetEffectTag());
       if (Binding.IsMaxResource())
       {
          float BaseValue = 0.f;
          if (!TryGetResourceBaseValue(Binding, BaseValue))
          {
             return false;
          }
          InitialValues.Add(Binding.StatTag, BaseValue * (1.f + Investment * 0.01f));
          OrderedTags.AddUnique(Binding.StatTag);
       }
    }

    TArray<FPairedResourceStatTag> ResourcesToFill;
    for (const FPairedResourceStatTag& Pair : PairedResourceStatTags)
    {
       if (!Pair.IsValid())
       {
          continue;
       }
       CurrentResourceTags.Add(Pair.CurrentStatTag);
       if (!InitialValues.Contains(Pair.CurrentStatTag))
       {
          ResourcesToFill.Add(Pair);
       }
    }
    OrderedTags.StableSort([&CurrentResourceTags](FGameplayTag Left, FGameplayTag Right) {
       return !CurrentResourceTags.Contains(Left) && CurrentResourceTags.Contains(Right);
    });

    TArray<TPair<FGameplayTag, float>> Values;
    for (FGameplayTag Tag : OrderedTags)
    {
       const float Value = InitialValues.FindChecked(Tag);
       if (!FMath::IsFinite(Value))
       {
          return false;
       }
       Values.Emplace(Tag, Value);
    }
    OutValues = MoveTemp(Values);
    OutResourcesToFill = MoveTemp(ResourcesToFill);
    return true;
}

// 한 스탯에 투자할 수 있는 최대 레벨을 안전한 범위로 제한해 업그레이드 UI와 계산의 상한으로 사용한다.
float UStatUpgradeDefinition::GetMaxInvestedLevel() const
{
    return FMath::IsFinite(MaxInvestedLevel) ? FMath::Clamp(MaxInvestedLevel, 0.f, MaxSupportedInvestedLevel) : 0.f;
}

// 게임에서 투자 가능한 각 스탯을 실제 능력치 태그·투자 레벨 태그·증가율 태그와 연결하는 고정 매핑을 제공한다.
TConstArrayView<FStatUpgradeBinding> UStatUpgradeDefinition::GetStatBindings()
{
    static const FStatUpgradeBinding Bindings[] =
    {
       { LabGameplayTags::Status_Offense_Strength, LabGameplayTags::Status_Offense_StrengthLevel },
       { LabGameplayTags::Status_Offense_Intelligence, LabGameplayTags::Status_Offense_IntelligenceLevel },
       { LabGameplayTags::Status_Offense_Critical, LabGameplayTags::Status_Offense_CriticalLevel },
       { LabGameplayTags::Status_Defense_Armor, LabGameplayTags::Status_Defense_ArmorLevel },
       { LabGameplayTags::Status_Defense_Recovery, LabGameplayTags::Status_Defense_RecoveryLevel },
       { LabGameplayTags::Status_Resistance_Frostbite, LabGameplayTags::Status_Resistance_FrostbiteLevel },
       { LabGameplayTags::Status_Resistance_Burn, LabGameplayTags::Status_Resistance_BurnLevel },
       { LabGameplayTags::Status_Resistance_ElectricShock, LabGameplayTags::Status_Resistance_ElectricShockLevel },
       { LabGameplayTags::Status_Agility_AttackSpeed, LabGameplayTags::Status_Agility_AttackSpeedLevel },
       { LabGameplayTags::Status_Agility_MovementSpeed, LabGameplayTags::Status_Agility_MovementSpeedLevel },
       { LabGameplayTags::Status_Agility_Arcane, LabGameplayTags::Status_Agility_ArcaneLevel },
       { LabGameplayTags::Status_PandoraForce_FirstPandora, LabGameplayTags::Status_PandoraForce_FirstPandoraLevel, {}, {}, true },
       { LabGameplayTags::Status_PandoraForce_SecondPandora, LabGameplayTags::Status_PandoraForce_SecondPandoraLevel, {}, {}, true },
       { LabGameplayTags::Status_PandoraForce_ThirdPandora, LabGameplayTags::Status_PandoraForce_ThirdPandoraLevel, {}, {}, true },
       { LabGameplayTags::Status_Defense_MaxShield, LabGameplayTags::Status_Defense_MaxShieldLevel,
          LabGameplayTags::Status_Defense_MaxShieldIncreasePercent, LabGameplayTags::Status_Defense_Shield, true },
       { LabGameplayTags::Status_Resource_MaxHealth, LabGameplayTags::Status_Resource_MaxHealthLevel,
          LabGameplayTags::Status_Resource_MaxHealthIncreasePercent, LabGameplayTags::Status_Resource_Health, true },
       { LabGameplayTags::Status_Resource_MaxMana, LabGameplayTags::Status_Resource_MaxManaLevel,
          LabGameplayTags::Status_Resource_MaxManaIncreasePercent, LabGameplayTags::Status_Resource_Mana, true },
       { LabGameplayTags::Status_Resource_MaxStamina, LabGameplayTags::Status_Resource_MaxStaminaLevel,
          LabGameplayTags::Status_Resource_MaxStaminaIncreasePercent, LabGameplayTags::Status_Resource_Stamina, true },
    };
    return MakeArrayView(Bindings);
}

// 선택한 실제 스탯 또는 증가율 태그가 어느 투자 레벨과 연결되는지 찾아 스탯 강화 처리에 사용한다.
const FStatUpgradeBinding* UStatUpgradeDefinition::FindStatBinding(const FGameplayTag& StatTag)
{
    for (const FStatUpgradeBinding& Binding : GetStatBindings())
    {
       if (StatTag == Binding.StatTag || (Binding.PercentTag.IsValid() && StatTag == Binding.PercentTag))
       {
          return &Binding;
       }
    }
    return nullptr;
}

// 실제 능력치 태그를 대응하는 '투자 레벨' 태그로 변환해 UI나 업그레이드 시스템이 현재 투자 단계를 조회할 수 있게 한다.
bool UStatUpgradeDefinition::TryResolveDefaultStatLevelTag(const FGameplayTag& StatTag, FGameplayTag& OutLevelTag)
{
    const FStatUpgradeBinding* Binding = FindStatBinding(StatTag);
    OutLevelTag = Binding ? Binding->LevelTag : FGameplayTag();
    return Binding != nullptr;
}

// 해당 스탯에 1레벨 투자할 때 실제로 얼마나 증가하는지 설정값에서 가져온다.
bool UStatUpgradeDefinition::TryGetUpgradeMagnitude(const FStatUpgradeBinding& Binding, float& OutMagnitude) const
{
    return (TryGetAttributeValuePerUpgrade(Binding.GetEffectTag(), OutMagnitude)
       || TryGetAttributeValuePerUpgrade(Binding.StatTag, OutMagnitude))
       && FMath::IsFinite(OutMagnitude) && OutMagnitude > 0.f;
}

// HP·MP·스태미나 같은 최대 자원의 순수 기본값을 구한다. 장비나 버프가 섞인 현재값은 사용하지 않는다.
bool UStatUpgradeDefinition::TryGetResourceBaseValue(const FStatUpgradeBinding& Binding, float& OutValue) const
{
    if (!TryGetExactAttributeDefaultValue(Binding.StatTag, OutValue))
    {
       FGameplayAttribute Attribute;
       if (!UBasicAttributeSet::ResolveAttributeFromStatTag(Binding.StatTag, Attribute))
       {
          return false;
       }
       OutValue = Attribute.GetNumericValue(GetDefault<UBasicAttributeSet>());
    }
    return FMath::IsFinite(OutValue) && OutValue > 0.f;
}

// 투자 레벨과 레벨당 증가량을 이용해 현재까지 누적된 실제 스탯 증가량을 계산한다.
float UStatUpgradeDefinition::CalculateInvestmentValue(float Magnitude, float InvestmentLevel, bool bCompounded)
{
    return bCompounded
       ? static_cast<float>((FMath::Pow(1.0 + static_cast<double>(Magnitude) * 0.01, static_cast<double>(InvestmentLevel)) - 1.0) * 100.0)
       : Magnitude * InvestmentLevel;
}

// 특정 스탯의 1레벨당 증가량을 찾는다. 개별 설정이 없으면 상위 태그의 공통 성장값까지 탐색한다.
bool UStatUpgradeDefinition::TryGetAttributeValuePerUpgrade(const FGameplayTag& StatTag, float& OutValue) const
{
    if (!StatTag.IsValid())
    {
       return false;
    }

    for (const FStatAttributeDefaultValue& AttributeValue : AttributeDefaultValues)
    {
       if (AttributeValue.StatTag.MatchesTagExact(StatTag))
       {
          OutValue = AttributeValue.ValuePerUpgrade;
          return true;
       }
    }

    for (const FStatAttributeDefaultValue& AttributeValue : AttributeDefaultValues)
    {
       if (AttributeValue.StatTag.IsValid() && StatTag.MatchesTag(AttributeValue.StatTag))
       {
          OutValue = AttributeValue.ValuePerUpgrade;
          return true;
       }
    }

    return false;
}

// 스탯 강화 계산에서 사용할 1레벨당 증가량을 반환하고, 설정 누락 시 로그를 남긴 뒤 안전한 기본값을 사용한다.
float UStatUpgradeDefinition::GetAttributeValuePerUpgrade(const FGameplayTag& StatTag) const
{
    float Value = 1.f;
    if (TryGetAttributeValuePerUpgrade(StatTag, Value))
    {
       return Value;
    }

    UE_LOG(
       StatUpgradeDefinitionLog,
       Error,
       TEXT("[StatUpgrade] Missing ValuePerUpgrade for stat '%s' in '%s'. Falling back to 1."),
       *StatTag.ToString(),
       *GetPathName());

    return 1.f;
}

// 특정 스탯에 직접 지정된 시작 기본값만 가져온다. 상위 카테고리의 공통값은 사용하지 않는다.
bool UStatUpgradeDefinition::TryGetExactAttributeDefaultValue(const FGameplayTag& StatTag, float& OutValue) const
{
    const FStatAttributeDefaultValue* AttributeValue = StatTag.IsValid()
       ? FindExactAttributeValue(AttributeDefaultValues, StatTag)
       : nullptr;
    if (!AttributeValue)
    {
       return false;
    }

    OutValue = AttributeValue->DefaultValue;
    return true;
}

#if WITH_EDITOR
// 에디터에서 스탯 DataAsset의 태그 중복, 잘못된 비용, 자원 연결, 비정상 수치 등을 미리 검사해 런타임 오류를 막는다.
EDataValidationResult UStatUpgradeDefinition::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (Result == EDataValidationResult::NotValidated)
    {
       Result = EDataValidationResult::Valid;
    }

    ValidatePositive(Context, Result, MaxInvestedLevel, NSLOCTEXT("StatUpgradeDefinition", "MaxInvestedLevelField", "MaxInvestedLevel"));
    ValidateLessOrEqual(Context, Result, MaxInvestedLevel, MaxSupportedInvestedLevel, NSLOCTEXT("StatUpgradeDefinition", "MaxInvestedLevelField", "MaxInvestedLevel"));

    TSet<FGameplayTag> UpgradeRootTags;
    for (int32 EntryIndex = 0; EntryIndex < UpgradeRules.Num(); ++EntryIndex)
    {
       const FStatUpgradeRule& Rule = UpgradeRules[EntryIndex];
       if (!Rule.IsValid())
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "InvalidUpgradeRule", "UpgradeRules entry {0} requires RootTag."),
             FText::AsNumber(EntryIndex)));
          continue;
       }

       if (UpgradeRootTags.Contains(Rule.RootTag))
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "DuplicateUpgradeRule", "UpgradeRules entry {0} duplicates RootTag '{1}'."),
             FText::AsNumber(EntryIndex),
             FText::FromString(Rule.RootTag.ToString())));
          continue;
       }

       UpgradeRootTags.Add(Rule.RootTag);

       ValidateNonNegative(
          Context,
          Result,
          Rule.Cost,
          FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "UpgradeRuleCostField", "UpgradeRules entry {0} Cost"),
             FText::AsNumber(EntryIndex)));

       if (Rule.Cost > 0.f && !Rule.CostPointTag.IsValid())
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "MissingCostPointTag", "UpgradeRules entry {0} has Cost but no CostPointTag."),
             FText::AsNumber(EntryIndex)));
       }
       else if (Rule.Cost <= 0.f && Rule.CostPointTag.IsValid())
       {
          Context.AddWarning(FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "UnusedCostPointTag", "UpgradeRules entry {0} has CostPointTag, but Cost is zero."),
             FText::AsNumber(EntryIndex)));
       }

       const bool bHasAttributeValueInCategory = AttributeDefaultValues.ContainsByPredicate(
          [&Rule](const FStatAttributeDefaultValue& AttributeValue)
          {
             return AttributeValue.StatTag.IsValid()
                && AttributeValue.StatTag.MatchesTag(Rule.RootTag);
          });
       if (!bHasAttributeValueInCategory)
       {
          Context.AddWarning(FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "MissingAttributeValueCategory", "UpgradeRules entry {0} has no child Attribute Values entries."),
             FText::AsNumber(EntryIndex)));
       }
    }

    if (UpgradeRules.IsEmpty())
    {
       Context.AddWarning(NSLOCTEXT("StatUpgradeDefinition", "NoUpgradeRules", "StatUpgradeDefinition has no upgrade rules."));
    }

    TSet<FGameplayTag> PairedMaxStatTags;
    for (int32 EntryIndex = 0; EntryIndex < PairedResourceStatTags.Num(); ++EntryIndex)
    {
       const FPairedResourceStatTag& Pair = PairedResourceStatTags[EntryIndex];
       if (!Pair.IsValid())
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "InvalidPairedResource", "PairedResourceStatTags entry {0} requires both MaxStatTag and CurrentStatTag."),
             FText::AsNumber(EntryIndex)));
       }
       else if (Pair.MaxStatTag == Pair.CurrentStatTag)
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "SelfPairedResource", "PairedResourceStatTags entry {0} cannot use the same tag for max and current resource."),
             FText::AsNumber(EntryIndex)));
       }
       else if (PairedMaxStatTags.Contains(Pair.MaxStatTag))
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "DuplicatePairedResource", "PairedResourceStatTags entry {0} duplicates MaxStatTag '{1}'."),
             FText::AsNumber(EntryIndex),
             FText::FromString(Pair.MaxStatTag.ToString())));
       }
       else
       {
          PairedMaxStatTags.Add(Pair.MaxStatTag);
       }
    }

    TSet<FGameplayTag> AttributeDefaultTags;
    for (int32 EntryIndex = 0; EntryIndex < AttributeDefaultValues.Num(); ++EntryIndex)
    {
       const FStatAttributeDefaultValue& AttributeDefault = AttributeDefaultValues[EntryIndex];
       if (!AttributeDefault.IsValid())
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "InvalidAttributeValue", "Attribute Values entry {0} requires StatTag."),
             FText::AsNumber(EntryIndex)));
          continue;
       }

       if (AttributeDefaultTags.Contains(AttributeDefault.StatTag))
       {
          MarkStatUpgradeInvalid(Context, Result, FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "DuplicateAttributeValue", "Attribute Values entry {0} duplicates StatTag '{1}'."),
             FText::AsNumber(EntryIndex),
             FText::FromString(AttributeDefault.StatTag.ToString())));
          continue;
       }

       AttributeDefaultTags.Add(AttributeDefault.StatTag);
       ValidateFinite(
          Context,
          Result,
          AttributeDefault.DefaultValue,
          FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "AttributeDefaultValueField", "Attribute Values entry {0} DefaultValue"),
             FText::AsNumber(EntryIndex)));
       ValidateFinite(
          Context,
          Result,
          AttributeDefault.ValuePerUpgrade,
          FText::Format(
             NSLOCTEXT("StatUpgradeDefinition", "AttributeValuePerUpgradeField", "Attribute Values entry {0} ValuePerUpgrade"),
             FText::AsNumber(EntryIndex)));
    }

    return Result;
}
#endif