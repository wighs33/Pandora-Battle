#include "Component/Player/StatUpgradeComponent.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Definition/Player/StatUpgradeDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatUpgradeComponent)

DEFINE_LOG_CATEGORY(StatUpgradeComponentLog);

namespace
{
	struct FConfiguredMaxResourceStat
	{
		FGameplayTag BaseStatTag;
		FGameplayTag IncreasePercentStatTag;
		FGameplayAttribute MaxAttribute;
		FGameplayAttribute CurrentAttribute;
		FGameplayAttribute IncreasePercentAttribute;
		FGameplayAttribute LevelAttribute;
	};

	struct FCompoundedPercentStat
	{
		FGameplayTag StatTag;
		FGameplayAttribute ValueAttribute;
		FGameplayAttribute LevelAttribute;
	};

	bool ResolveConfiguredMaxResourceStat(const FGameplayTag& StatTag, FConfiguredMaxResourceStat& OutStat)
	{
		OutStat = FConfiguredMaxResourceStat();

		if (StatTag.MatchesTagExact(LabGameplayTags::Status_Defense_MaxShield)
			|| StatTag.MatchesTagExact(LabGameplayTags::Status_Defense_MaxShieldIncreasePercent))
		{
			OutStat.BaseStatTag = LabGameplayTags::Status_Defense_MaxShield;
			OutStat.IncreasePercentStatTag = LabGameplayTags::Status_Defense_MaxShieldIncreasePercent;
			OutStat.MaxAttribute = UBasicAttributeSet::GetMaxShieldAttribute();
			OutStat.CurrentAttribute = UBasicAttributeSet::GetShieldAttribute();
			OutStat.IncreasePercentAttribute = UBasicAttributeSet::GetMaxShieldIncreasePercentAttribute();
			OutStat.LevelAttribute = UBasicAttributeSet::GetMaxShieldLevelAttribute();
			return true;
		}

		if (StatTag.MatchesTagExact(LabGameplayTags::Status_Resource_MaxHealth)
			|| StatTag.MatchesTagExact(LabGameplayTags::Status_Resource_MaxHealthIncreasePercent))
		{
			OutStat.BaseStatTag = LabGameplayTags::Status_Resource_MaxHealth;
			OutStat.IncreasePercentStatTag = LabGameplayTags::Status_Resource_MaxHealthIncreasePercent;
			OutStat.MaxAttribute = UBasicAttributeSet::GetMaxHealthAttribute();
			OutStat.CurrentAttribute = UBasicAttributeSet::GetHealthAttribute();
			OutStat.IncreasePercentAttribute = UBasicAttributeSet::GetMaxHealthIncreasePercentAttribute();
			OutStat.LevelAttribute = UBasicAttributeSet::GetMaxHealthLevelAttribute();
			return true;
		}

		if (StatTag.MatchesTagExact(LabGameplayTags::Status_Resource_MaxMana)
			|| StatTag.MatchesTagExact(LabGameplayTags::Status_Resource_MaxManaIncreasePercent))
		{
			OutStat.BaseStatTag = LabGameplayTags::Status_Resource_MaxMana;
			OutStat.IncreasePercentStatTag = LabGameplayTags::Status_Resource_MaxManaIncreasePercent;
			OutStat.MaxAttribute = UBasicAttributeSet::GetMaxManaAttribute();
			OutStat.CurrentAttribute = UBasicAttributeSet::GetManaAttribute();
			OutStat.IncreasePercentAttribute = UBasicAttributeSet::GetMaxManaIncreasePercentAttribute();
			OutStat.LevelAttribute = UBasicAttributeSet::GetMaxManaLevelAttribute();
			return true;
		}

		if (StatTag.MatchesTagExact(LabGameplayTags::Status_Resource_MaxStamina)
			|| StatTag.MatchesTagExact(LabGameplayTags::Status_Resource_MaxStaminaIncreasePercent))
		{
			OutStat.BaseStatTag = LabGameplayTags::Status_Resource_MaxStamina;
			OutStat.IncreasePercentStatTag = LabGameplayTags::Status_Resource_MaxStaminaIncreasePercent;
			OutStat.MaxAttribute = UBasicAttributeSet::GetMaxStaminaAttribute();
			OutStat.CurrentAttribute = UBasicAttributeSet::GetStaminaAttribute();
			OutStat.IncreasePercentAttribute = UBasicAttributeSet::GetMaxStaminaIncreasePercentAttribute();
			OutStat.LevelAttribute = UBasicAttributeSet::GetMaxStaminaLevelAttribute();
			return true;
		}

		return false;
	}

	bool ResolveCompoundedPercentStat(const FGameplayTag& StatTag, FCompoundedPercentStat& OutStat)
	{
		OutStat = FCompoundedPercentStat();

		if (StatTag.MatchesTagExact(LabGameplayTags::Status_PandoraForce_FirstPandora))
		{
			OutStat.StatTag = LabGameplayTags::Status_PandoraForce_FirstPandora;
			OutStat.ValueAttribute = UBasicAttributeSet::GetFirstPandoraAttribute();
			OutStat.LevelAttribute = UBasicAttributeSet::GetFirstPandoraLevelAttribute();
			return true;
		}

		if (StatTag.MatchesTagExact(LabGameplayTags::Status_PandoraForce_SecondPandora))
		{
			OutStat.StatTag = LabGameplayTags::Status_PandoraForce_SecondPandora;
			OutStat.ValueAttribute = UBasicAttributeSet::GetSecondPandoraAttribute();
			OutStat.LevelAttribute = UBasicAttributeSet::GetSecondPandoraLevelAttribute();
			return true;
		}

		if (StatTag.MatchesTagExact(LabGameplayTags::Status_PandoraForce_ThirdPandora))
		{
			OutStat.StatTag = LabGameplayTags::Status_PandoraForce_ThirdPandora;
			OutStat.ValueAttribute = UBasicAttributeSet::GetThirdPandoraAttribute();
			OutStat.LevelAttribute = UBasicAttributeSet::GetThirdPandoraLevelAttribute();
			return true;
		}

		return false;
	}

	FGameplayTag ResolveStatEffectTagForUpgrade(const FGameplayTag& StatTag)
	{
		FConfiguredMaxResourceStat ResourceStat;
		return ResolveConfiguredMaxResourceStat(StatTag, ResourceStat)
			? ResourceStat.IncreasePercentStatTag
			: StatTag;
	}

	double CalculateCompoundedMultiplier(float PerUpgradePercent, float InvestmentLevel)
	{
		return PerUpgradePercent > UE_KINDA_SMALL_NUMBER
			? FMath::Pow(1.0 + static_cast<double>(PerUpgradePercent) * 0.01, static_cast<double>(FMath::Max(InvestmentLevel, 0.f)))
			: 1.0;
	}

	float CalculateCompoundedIncreasePercent(float PerUpgradePercent, float InvestmentLevel)
	{
		return static_cast<float>((CalculateCompoundedMultiplier(PerUpgradePercent, InvestmentLevel) - 1.0) * 100.0);
	}

	void AddStatMagnitude(TMap<FGameplayTag, float>& StatMagnitudes, const FGameplayTag& StatTag, const float Magnitude)
	{
		if (!StatTag.IsValid() || FMath::IsNearlyZero(Magnitude))
		{
			return;
		}

		float& ExistingMagnitude = StatMagnitudes.FindOrAdd(StatTag);
		ExistingMagnitude += Magnitude;
		if (FMath::IsNearlyZero(ExistingMagnitude))
		{
			StatMagnitudes.Remove(StatTag);
		}
	}
}

UStatUpgradeComponent::UStatUpgradeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	StatUpgradeDefinition = TSoftObjectPtr<UStatUpgradeDefinition>(
		UStatUpgradeDefinition::GetDefaultDefinitionPath());
}

void UStatUpgradeComponent::BeginPlay()
{
	Super::BeginPlay();

	BeginStatUpgradeDefinitionPreload();
	StartRecoveryHealthRegen();
}

void UStatUpgradeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopRecoveryHealthRegen();
	UnbindRecoveryAttributeChanged();
	ReleaseStatUpgradeDefinitionPreload();
	LoadedStatUpgradeDefinition = nullptr;
	bApplyDefaultsWhenDefinitionReady = false;

	Super::EndPlay(EndPlayReason);
}

bool UStatUpgradeComponent::RequestStatUp(FGameplayTag StatTag)
{
	AActor* OwnerActor = GetOwner();

	if (!OwnerActor || !StatTag.IsValid())
	{
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		ServerRequestStatUp(StatTag);
		return true;
	}

	return ApplyStatUpInternal(StatTag);
}

bool UStatUpgradeComponent::RequestStatDown(FGameplayTag StatTag)
{
	AActor* OwnerActor = GetOwner();

	if (!OwnerActor || !StatTag.IsValid())
	{
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		ServerRequestStatDown(StatTag);
		return true;
	}

	return ApplyStatDownInternal(StatTag);
}

bool UStatUpgradeComponent::SetPointsForAllCategories(const float Value)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor
		|| !OwnerActor->HasAuthority()
		|| !FMath::IsFinite(Value)
		|| Value < 0.0f)
	{
		return false;
	}

	APdPlayerState* PlayerState = Cast<APdPlayerState>(OwnerActor);
	UPdAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetPdAbilitySystemComponent() : nullptr;
	UBasicAttributeSet* AttributeSet = PlayerState ? PlayerState->GetPdAttributeSet() : nullptr;
	if (!ASC || !AttributeSet)
	{
		return false;
	}

	const FGameplayAttribute PointAttributes[] = {
		UBasicAttributeSet::GetOffensePointAttribute(),
		UBasicAttributeSet::GetDefensePointAttribute(),
		UBasicAttributeSet::GetResistancePointAttribute(),
		UBasicAttributeSet::GetPandoraForcePointAttribute(),
		UBasicAttributeSet::GetResourcePointAttribute(),
		UBasicAttributeSet::GetAgilityPointAttribute()
	};

	for (const FGameplayAttribute& PointAttribute : PointAttributes)
	{
		ASC->SetNumericAttributeBase(PointAttribute, Value);
		if (FProperty* Property = PointAttribute.GetUProperty())
		{
			MARK_PROPERTY_DIRTY(AttributeSet, Property);
		}
	}

	ASC->ForceReplication();
	PlayerState->ForceNetUpdate();
	return true;
}

void UStatUpgradeComponent::ServerRequestStatUp_Implementation(FGameplayTag StatTag)
{
	ApplyStatUpInternal(StatTag);
}

void UStatUpgradeComponent::ServerRequestStatDown_Implementation(FGameplayTag StatTag)
{
	ApplyStatDownInternal(StatTag);
}

bool UStatUpgradeComponent::ApplyStatUpInternal(FGameplayTag StatTag)
{
	AActor* OwnerActor = GetOwner();

	if (!OwnerActor || !OwnerActor->HasAuthority() || !StatTag.IsValid())
	{
		return false;
	}

	UStatUpgradeDefinition* LoadedDefinition = LoadStatUpgradeDefinition();

	const TSubclassOf<UGameplayEffect> EquipmentStatGameplayEffectClass =
		GetEquipmentStatGameplayEffectClass();
	if (!LoadedDefinition || !EquipmentStatGameplayEffectClass)
	{
		return false;
	}

	APdPlayerState* PlayerState = Cast<APdPlayerState>(OwnerActor);
	UPdAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetPdAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return false;
	}

	FGameplayTag StatLevelTag;
	if (!ResolveStatLevelTag(StatTag, StatLevelTag))
	{
		return false;
	}

	FGameplayAttribute StatLevelAttribute;
	if (!ASC->ResolveAttributeFromTag(StatLevelTag, StatLevelAttribute))
	{
		return false;
	}

	const float CurrentStatLevel = ASC->GetNumericAttribute(StatLevelAttribute);
	const float ConfiguredMaxInvestedLevel = LoadedDefinition->GetMaxInvestedLevel();
	const float MaxInvestedLevel = FMath::IsFinite(ConfiguredMaxInvestedLevel)
		? FMath::Max(ConfiguredMaxInvestedLevel, 0.f)
		: 0.f;
	if (CurrentStatLevel >= MaxInvestedLevel - UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	float Magnitude = 0.f;
	EEnum_Operation Operation = EEnum_Operation::Add;
	FGameplayTag CostPointTag;
	float Cost = 0.f;
	if (!ResolveStatUpButtonSettings(StatTag, Magnitude, Operation, CostPointTag, Cost))
	{
		return false;
	}

	if (Operation != EEnum_Operation::Add)
	{
		return false;
	}

	if (Cost > 0.f)
	{
		if (!CostPointTag.IsValid())
		{
			return false;
		}

		FGameplayAttribute CostPointAttribute;
		if (!ASC->ResolveAttributeFromTag(CostPointTag, CostPointAttribute))
		{
			return false;
		}

		const float CurrentPoint = ASC->GetNumericAttribute(CostPointAttribute);
		if (CurrentPoint + UE_KINDA_SMALL_NUMBER < Cost)
		{
			return false;
		}
	}

	FConfiguredMaxResourceStat ResourceStat;
	const bool bUsesConfiguredMaxResource = ResolveConfiguredMaxResourceStat(StatTag, ResourceStat);
	FCompoundedPercentStat CompoundedPercentStat;
	const bool bUsesCompoundedPercent = ResolveCompoundedPercentStat(StatTag, CompoundedPercentStat);
	const bool bUsesCompoundedFormula = bUsesConfiguredMaxResource || bUsesCompoundedPercent;
	const float EffectMagnitude = bUsesCompoundedFormula
		? CalculateCompoundedIncreasePercent(Magnitude, CurrentStatLevel + 1.f)
			- CalculateCompoundedIncreasePercent(Magnitude, CurrentStatLevel)
		: Magnitude;
	if (FMath::IsNearlyZero(EffectMagnitude))
	{
		return false;
	}

	const FGameplayTag EffectStatTag = ResolveStatEffectTagForUpgrade(StatTag);
	FGameplayAttribute EffectAttribute;
	if (!ASC->ResolveAttributeFromTag(EffectStatTag, EffectAttribute))
	{
		return false;
	}

	TMap<FGameplayTag, float> StatMagnitudes;
	AddStatMagnitude(StatMagnitudes, EffectStatTag, EffectMagnitude);
	FGameplayTag PairedCurrentResourceStatTag;
	if (!bUsesConfiguredMaxResource && ResolvePairedCurrentResourceStatTag(StatTag, PairedCurrentResourceStatTag))
	{
		FGameplayAttribute PairedCurrentResourceAttribute;
		if (!ASC->ResolveAttributeFromTag(PairedCurrentResourceStatTag, PairedCurrentResourceAttribute))
		{
			return false;
		}

		AddStatMagnitude(StatMagnitudes, PairedCurrentResourceStatTag, Magnitude);
	}
	AddStatMagnitude(StatMagnitudes, CostPointTag, -Cost);
	AddStatMagnitude(StatMagnitudes, StatLevelTag, 1.f);

	if (!ApplyStatUpgradeEffects(EquipmentStatGameplayEffectClass, StatMagnitudes, EEnum_Operation::Add))
	{
		UE_LOG(StatUpgradeComponentLog, Error, TEXT("[StatUpgrade] Failed to apply stat up effects. stat=%s"),
			*StatTag.ToString());
		return false;
	}

	if (bUsesConfiguredMaxResource)
	{
		RecalculateConfiguredMaxResource(StatTag, CurrentStatLevel, Magnitude);
	}
	else if (bUsesCompoundedPercent)
	{
		RecalculateCompoundedPercentStat(StatTag, Magnitude);
	}

	return true;
}

bool UStatUpgradeComponent::ApplyStatDownInternal(FGameplayTag StatTag)
{
	AActor* OwnerActor = GetOwner();

	if (!OwnerActor || !OwnerActor->HasAuthority() || !StatTag.IsValid())
	{
		return false;
	}

	UStatUpgradeDefinition* LoadedDefinition = LoadStatUpgradeDefinition();

	const TSubclassOf<UGameplayEffect> EquipmentStatGameplayEffectClass =
		GetEquipmentStatGameplayEffectClass();
	if (!LoadedDefinition || !EquipmentStatGameplayEffectClass)
	{
		return false;
	}

	APdPlayerState* PlayerState = Cast<APdPlayerState>(OwnerActor);
	UPdAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetPdAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return false;
	}

	float Magnitude = 0.f;
	EEnum_Operation Operation = EEnum_Operation::Add;
	FGameplayTag CostPointTag;
	float Cost = 0.f;
	if (!ResolveStatUpButtonSettings(StatTag, Magnitude, Operation, CostPointTag, Cost))
	{
		return false;
	}

	if (Operation != EEnum_Operation::Add)
	{
		return false;
	}

	FGameplayTag StatLevelTag;
	if (!ResolveStatLevelTag(StatTag, StatLevelTag))
	{
		return false;
	}

	FGameplayAttribute StatLevelAttribute;
	if (!ASC->ResolveAttributeFromTag(StatLevelTag, StatLevelAttribute))
	{
		return false;
	}

	const float CurrentStatLevel = ASC->GetNumericAttribute(StatLevelAttribute);
	if (CurrentStatLevel < 1.f)
	{
		return false;
	}

	if (Cost > 0.f && !CostPointTag.IsValid())
	{
		return false;
	}
	if (Cost > 0.f)
	{
		FGameplayAttribute CostPointAttribute;
		if (!ASC->ResolveAttributeFromTag(CostPointTag, CostPointAttribute))
		{
			return false;
		}
	}

	FConfiguredMaxResourceStat ResourceStat;
	const bool bUsesConfiguredMaxResource = ResolveConfiguredMaxResourceStat(StatTag, ResourceStat);
	FCompoundedPercentStat CompoundedPercentStat;
	const bool bUsesCompoundedPercent = ResolveCompoundedPercentStat(StatTag, CompoundedPercentStat);
	const bool bUsesCompoundedFormula = bUsesConfiguredMaxResource || bUsesCompoundedPercent;
	const float EffectMagnitude = bUsesCompoundedFormula
		? CalculateCompoundedIncreasePercent(Magnitude, CurrentStatLevel - 1.f)
			- CalculateCompoundedIncreasePercent(Magnitude, CurrentStatLevel)
		: -Magnitude;
	if (FMath::IsNearlyZero(EffectMagnitude))
	{
		return false;
	}

	const FGameplayTag EffectStatTag = ResolveStatEffectTagForUpgrade(StatTag);
	FGameplayAttribute EffectAttribute;
	if (!ASC->ResolveAttributeFromTag(EffectStatTag, EffectAttribute))
	{
		return false;
	}

	TMap<FGameplayTag, float> StatMagnitudes;
	AddStatMagnitude(StatMagnitudes, EffectStatTag, EffectMagnitude);
	FGameplayTag PairedCurrentResourceStatTag;
	if (!bUsesConfiguredMaxResource && ResolvePairedCurrentResourceStatTag(StatTag, PairedCurrentResourceStatTag))
	{
		FGameplayAttribute PairedCurrentResourceAttribute;
		if (!ASC->ResolveAttributeFromTag(PairedCurrentResourceStatTag, PairedCurrentResourceAttribute))
		{
			return false;
		}

		AddStatMagnitude(StatMagnitudes, PairedCurrentResourceStatTag, -Magnitude);
	}
	AddStatMagnitude(StatMagnitudes, CostPointTag, Cost);
	AddStatMagnitude(StatMagnitudes, StatLevelTag, -1.f);

	if (!ApplyStatUpgradeEffects(EquipmentStatGameplayEffectClass, StatMagnitudes, EEnum_Operation::Add))
	{
		UE_LOG(StatUpgradeComponentLog, Error, TEXT("[StatUpgrade] Failed to apply stat down effects. stat=%s"),
			*StatTag.ToString());
		return false;
	}

	if (bUsesConfiguredMaxResource)
	{
		RecalculateConfiguredMaxResource(StatTag, CurrentStatLevel, Magnitude);
	}
	else if (bUsesCompoundedPercent)
	{
		RecalculateCompoundedPercentStat(StatTag, Magnitude);
	}

	return true;
}

UStatUpgradeDefinition* UStatUpgradeComponent::LoadStatUpgradeDefinition()
{
	if (LoadedStatUpgradeDefinition)
	{
		return LoadedStatUpgradeDefinition;
	}

	if (StatUpgradeDefinition.IsNull())
	{
		return nullptr;
	}

	LoadedStatUpgradeDefinition = StatUpgradeDefinition.Get();
	if (!LoadedStatUpgradeDefinition
		&& !StatUpgradeDefinitionLoadHandle.IsValid())
	{
		BeginStatUpgradeDefinitionPreload();
	}
	return LoadedStatUpgradeDefinition;
}

void UStatUpgradeComponent::BeginStatUpgradeDefinitionPreload()
{
	ReleaseStatUpgradeDefinitionPreload();
	LoadedStatUpgradeDefinition = nullptr;

	if (StatUpgradeDefinition.IsNull())
	{
		return;
	}

	if (UStatUpgradeDefinition* LoadedDefinition = StatUpgradeDefinition.Get())
	{
		LoadedStatUpgradeDefinition = LoadedDefinition;
		if (bApplyDefaultsWhenDefinitionReady)
		{
			bApplyDefaultsWhenDefinitionReady = false;
			ApplyConfiguredAttributeDefaults();
		}
		return;
	}

	const uint32 RequestGeneration = StatUpgradeDefinitionLoadGeneration;
	TSharedPtr<FStreamableHandle> NewLoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			StatUpgradeDefinition.ToSoftObjectPath(),
			FStreamableDelegate::CreateWeakLambda(
				this,
				[this, RequestGeneration]()
				{
					HandleStatUpgradeDefinitionPreloaded(RequestGeneration);
				}));

	if (NewLoadHandle.IsValid()
		&& RequestGeneration == StatUpgradeDefinitionLoadGeneration
		&& !LoadedStatUpgradeDefinition)
	{
		StatUpgradeDefinitionLoadHandle = MoveTemp(NewLoadHandle);
	}
	else if (NewLoadHandle.IsValid())
	{
		NewLoadHandle->ReleaseHandle();
	}
}

void UStatUpgradeComponent::HandleStatUpgradeDefinitionPreloaded(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != StatUpgradeDefinitionLoadGeneration)
	{
		return;
	}

	LoadedStatUpgradeDefinition = StatUpgradeDefinition.Get();
	if (!LoadedStatUpgradeDefinition)
	{
		UE_LOG(
			StatUpgradeComponentLog,
			Error,
			TEXT("Stat upgrade definition '%s' did not resolve after asynchronous preload."),
			*StatUpgradeDefinition.ToString());
		return;
	}

	if (bApplyDefaultsWhenDefinitionReady)
	{
		bApplyDefaultsWhenDefinitionReady = false;
		ApplyConfiguredAttributeDefaults();
	}
}

void UStatUpgradeComponent::ReleaseStatUpgradeDefinitionPreload()
{
	++StatUpgradeDefinitionLoadGeneration;
	if (StatUpgradeDefinitionLoadHandle.IsValid())
	{
		StatUpgradeDefinitionLoadHandle->CancelHandle();
		StatUpgradeDefinitionLoadHandle->ReleaseHandle();
		StatUpgradeDefinitionLoadHandle.Reset();
	}
}

bool UStatUpgradeComponent::ApplyConfiguredAttributeDefaults()
{
	UStatUpgradeDefinition* LoadedDefinition = LoadStatUpgradeDefinition();
	if (!LoadedDefinition)
	{
		bApplyDefaultsWhenDefinitionReady = !StatUpgradeDefinition.IsNull();
		return false;
	}
	bApplyDefaultsWhenDefinitionReady = false;
	if (LoadedDefinition->GetAttributeDefaultValues().IsEmpty())
	{
		return false;
	}

	UPdAbilitySystemComponent* ASC = GetOwnerPdAbilitySystemComponent();
	if (!ASC)
	{
		return false;
	}
	UAttributeSet* DefaultAttributeSet = const_cast<UAttributeSet*>(
		ASC->GetAttributeSet(UBasicAttributeSet::StaticClass()));
	const FSoftObjectPath DefinitionPath =
		StatUpgradeDefinition.ToSoftObjectPath();
	if (!ASC->IsOwnerActorAuthoritative()
		|| !DefaultAttributeSet
		|| DefinitionPath.IsNull())
	{
		return false;
	}

	if (ASC->HasAppliedConfiguredAttributeDefaults(
		DefaultAttributeSet,
		DefinitionPath))
	{
		return true;
	}

	TArray<FStatAttributeDefaultValue> OrderedDefaults = LoadedDefinition->GetAttributeDefaultValues();
	OrderedDefaults.StableSort([](const FStatAttributeDefaultValue& Left, const FStatAttributeDefaultValue& Right)
	{
		return Left.Priority < Right.Priority;
	});

	TArray<TPair<FGameplayAttribute, float>> ResolvedDefaults;
	ResolvedDefaults.Reserve(OrderedDefaults.Num());
	for (const FStatAttributeDefaultValue& AttributeDefault : OrderedDefaults)
	{
		if (!AttributeDefault.IsValid())
		{
			continue;
		}

		FGameplayAttribute Attribute;
		if (!ASC->ResolveAttributeFromTag(AttributeDefault.StatTag, Attribute))
		{
			UE_LOG(
				StatUpgradeComponentLog,
				Warning,
				TEXT("Cannot apply configured attribute defaults because stat tag '%s' is not mapped on %s."),
				*AttributeDefault.StatTag.ToString(),
				*GetNameSafe(ASC));
			return false;
		}

		ResolvedDefaults.Emplace(
			Attribute,
			AttributeDefault.DefaultValue);
	}
	if (ResolvedDefaults.IsEmpty())
	{
		return false;
	}

	TArray<TPair<FGameplayAttribute, FGameplayAttribute>>
		ResolvedPairedResources;
	for (const FPairedResourceStatTag& Pair :
		LoadedDefinition->GetPairedResourceStatTags())
	{
		float ExplicitCurrentValue = 0.0f;
		if (!Pair.IsValid()
			|| LoadedDefinition->TryGetExactAttributeDefaultValue(
				Pair.CurrentStatTag,
				ExplicitCurrentValue))
		{
			continue;
		}

		FGameplayAttribute MaxAttribute;
		FGameplayAttribute CurrentAttribute;
		if (!ASC->ResolveAttributeFromTag(
				Pair.MaxStatTag,
				MaxAttribute)
			|| !ASC->ResolveAttributeFromTag(
				Pair.CurrentStatTag,
				CurrentAttribute))
		{
			UE_LOG(
				StatUpgradeComponentLog,
				Warning,
				TEXT("Cannot apply configured attribute defaults because resource pair '%s'/'%s' is not mapped on %s."),
				*Pair.CurrentStatTag.ToString(),
				*Pair.MaxStatTag.ToString(),
				*GetNameSafe(ASC));
			return false;
		}
		ResolvedPairedResources.Emplace(
			MaxAttribute,
			CurrentAttribute);
	}

	ASC->MarkConfiguredAttributeDefaultsApplied(
		DefaultAttributeSet,
		DefinitionPath);

	for (const TPair<FGameplayAttribute, float>& ResolvedDefault :
		ResolvedDefaults)
	{
		if (!ASC->ApplyAttributeDefaultValue(
			ResolvedDefault.Key,
			ResolvedDefault.Value))
		{
			ASC->ClearConfiguredAttributeDefaultsApplied(
				DefaultAttributeSet,
				DefinitionPath);
			return false;
		}
	}

	RecalculateConfiguredMaxResources();
	RecalculateCompoundedPercentStats();

	for (const TPair<FGameplayAttribute, FGameplayAttribute>& ResolvedPair :
		ResolvedPairedResources)
	{
		const float MaxValue =
			ASC->GetNumericAttribute(ResolvedPair.Key);
		if (!FMath::IsNearlyEqual(
			ASC->GetNumericAttribute(ResolvedPair.Value),
			MaxValue)
			&& !ASC->ApplyAttributeDefaultValue(
				ResolvedPair.Value,
				MaxValue))
		{
			ASC->ClearConfiguredAttributeDefaultsApplied(
				DefaultAttributeSet,
				DefinitionPath);
			return false;
		}
	}

	return true;
}

bool UStatUpgradeComponent::ResolveStatUpButtonSettings(const FGameplayTag& StatTag, float& OutMagnitude, EEnum_Operation& OutOperation,
	FGameplayTag& OutCostPointTag, float& OutCost) const
{
	OutMagnitude = 0.f;
	OutOperation = EEnum_Operation::Add;
	OutCostPointTag = FGameplayTag();
	OutCost = 0.f;

	if (!StatTag.IsValid())
	{
		return false;
	}

	const UStatUpgradeDefinition* LoadedDefinition = LoadedStatUpgradeDefinition;
	if (!LoadedDefinition || LoadedDefinition->GetUpgradeRules().IsEmpty())
	{
		return false;
	}

	const FStatUpgradeRule* Setting = LoadedDefinition->FindUpgradeRuleForStat(StatTag);
	if (!Setting)
	{
		return false;
	}

	const FGameplayTag EffectStatTag = ResolveStatEffectTagForUpgrade(StatTag);
	if (!LoadedDefinition->TryGetAttributeValuePerUpgrade(EffectStatTag, OutMagnitude))
	{
		OutMagnitude = LoadedDefinition->GetAttributeValuePerUpgrade(StatTag);
	}
	OutOperation = EEnum_Operation::Add;
	OutCostPointTag = Setting->CostPointTag;
	OutCost = FMath::Max(Setting->Cost, 0.f);
	return true;
}

bool UStatUpgradeComponent::ResolvePairedCurrentResourceStatTag(const FGameplayTag& StatTag, FGameplayTag& OutPairedStatTag) const
{
	OutPairedStatTag = FGameplayTag();

	if (!StatTag.IsValid())
	{

		return false;
	}

	const UStatUpgradeDefinition* LoadedDefinition = LoadedStatUpgradeDefinition;
	if (!LoadedDefinition || LoadedDefinition->GetPairedResourceStatTags().IsEmpty())
	{
		return false;
	}

	for (const FPairedResourceStatTag& Pair : LoadedDefinition->GetPairedResourceStatTags())
	{
		if (Pair.MaxStatTag.IsValid() && Pair.CurrentStatTag.IsValid() && StatTag == Pair.MaxStatTag)
		{
			OutPairedStatTag = Pair.CurrentStatTag;

			return true;
		}
	}

	return false;
}

bool UStatUpgradeComponent::ResolveStatLevelTag(const FGameplayTag& StatTag, FGameplayTag& OutStatLevelTag) const
{
	OutStatLevelTag = FGameplayTag();
	if (!StatTag.IsValid())
	{
		return false;
	}

	return UStatUpgradeDefinition::TryResolveDefaultStatLevelTag(StatTag, OutStatLevelTag);
}

bool UStatUpgradeComponent::RecalculateConfiguredMaxResources()
{
	bool bRecalculatedAny = false;
	bRecalculatedAny |= RecalculateConfiguredMaxResource(LabGameplayTags::Status_Defense_MaxShield);
	bRecalculatedAny |= RecalculateConfiguredMaxResource(LabGameplayTags::Status_Resource_MaxHealth);
	bRecalculatedAny |= RecalculateConfiguredMaxResource(LabGameplayTags::Status_Resource_MaxMana);
	bRecalculatedAny |= RecalculateConfiguredMaxResource(LabGameplayTags::Status_Resource_MaxStamina);
	return bRecalculatedAny;
}

bool UStatUpgradeComponent::RecalculateConfiguredMaxResource(const FGameplayTag& ResourceStatTag, TOptional<float> PreviousInvestmentLevel,
	TOptional<float> PerUpgradePercent)
{
	FConfiguredMaxResourceStat ResourceStat;
	if (!ResolveConfiguredMaxResourceStat(ResourceStatTag, ResourceStat))
	{
		return false;
	}

	const UStatUpgradeDefinition* LoadedDefinition = LoadedStatUpgradeDefinition
		? LoadedStatUpgradeDefinition.Get()
		: LoadStatUpgradeDefinition();
	UPdAbilitySystemComponent* ASC = GetOwnerPdAbilitySystemComponent();
	if (!LoadedDefinition || !ASC)
	{
		return false;
	}

	const float IncreasePercent = FMath::Max(ASC->GetNumericAttribute(ResourceStat.IncreasePercentAttribute), 0.f);
	const float OldMaxResource = FMath::Max(ASC->GetNumericAttribute(ResourceStat.MaxAttribute), 0.f);
	const float OldCurrentResource = FMath::Max(ASC->GetNumericAttribute(ResourceStat.CurrentAttribute), 0.f);
	const float InvestmentLevel = ResourceStat.LevelAttribute.IsValid()
		? FMath::Max(ASC->GetNumericAttribute(ResourceStat.LevelAttribute), 0.f)
		: 0.f;
	float StepPercent = PerUpgradePercent.IsSet() ? PerUpgradePercent.GetValue() : 0.f;
	if (StepPercent <= UE_KINDA_SMALL_NUMBER
		&& !LoadedDefinition->TryGetAttributeValuePerUpgrade(ResourceStat.BaseStatTag, StepPercent))
	{
		LoadedDefinition->TryGetAttributeValuePerUpgrade(ResourceStat.IncreasePercentStatTag, StepPercent);
	}
	StepPercent = FMath::Max(StepPercent, 0.f);

	float BaseMaxResource = 0.f;
	if (!LoadedDefinition->TryGetExactAttributeDefaultValue(ResourceStat.BaseStatTag, BaseMaxResource))
	{
		const float PreviousLevelValue = FMath::Max(
			PreviousInvestmentLevel.IsSet() ? PreviousInvestmentLevel.GetValue() : InvestmentLevel,
			0.f);
		const double PreviousMultiplier = CalculateCompoundedMultiplier(StepPercent, PreviousLevelValue);
		BaseMaxResource = PreviousMultiplier > UE_KINDA_SMALL_NUMBER
			? static_cast<float>(static_cast<double>(OldMaxResource) / PreviousMultiplier)
			: OldMaxResource;
	}

	if (BaseMaxResource <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const double ResourceMultiplier = CalculateCompoundedMultiplier(StepPercent, InvestmentLevel);
	const float CompoundedIncreasePercent = CalculateCompoundedIncreasePercent(StepPercent, InvestmentLevel);
	const float NewMaxResource = static_cast<float>(static_cast<double>(BaseMaxResource) * ResourceMultiplier);
	const bool bWasEffectivelyFull = OldMaxResource <= UE_KINDA_SMALL_NUMBER
		|| OldCurrentResource >= OldMaxResource - 1.f;
	const float NewCurrentResource = bWasEffectivelyFull
		? NewMaxResource
		: FMath::Clamp(OldCurrentResource * (NewMaxResource / OldMaxResource), 0.f, NewMaxResource);

	bool bAppliedAny = false;
	if (!FMath::IsNearlyEqual(IncreasePercent, CompoundedIncreasePercent))
	{
		bAppliedAny |= ASC->ApplyAttributeDefaultValue(ResourceStat.IncreasePercentAttribute, CompoundedIncreasePercent);
	}

	if (!FMath::IsNearlyEqual(OldMaxResource, NewMaxResource))
	{
		bAppliedAny |= ASC->ApplyAttributeDefaultValue(ResourceStat.MaxAttribute, NewMaxResource);
	}

	if (!FMath::IsNearlyEqual(OldCurrentResource, NewCurrentResource))
	{
		bAppliedAny |= ASC->ApplyAttributeDefaultValue(ResourceStat.CurrentAttribute, NewCurrentResource);
	}

	return bAppliedAny;
}

bool UStatUpgradeComponent::RecalculateCompoundedPercentStats()
{
	bool bRecalculatedAny = false;
	bRecalculatedAny |= RecalculateCompoundedPercentStat(LabGameplayTags::Status_PandoraForce_FirstPandora);
	bRecalculatedAny |= RecalculateCompoundedPercentStat(LabGameplayTags::Status_PandoraForce_SecondPandora);
	bRecalculatedAny |= RecalculateCompoundedPercentStat(LabGameplayTags::Status_PandoraForce_ThirdPandora);
	return bRecalculatedAny;
}

bool UStatUpgradeComponent::RecalculateCompoundedPercentStat(const FGameplayTag& StatTag, TOptional<float> PerUpgradePercent)
{
	FCompoundedPercentStat CompoundedStat;
	if (!ResolveCompoundedPercentStat(StatTag, CompoundedStat))
	{
		return false;
	}

	const UStatUpgradeDefinition* LoadedDefinition = LoadedStatUpgradeDefinition
		? LoadedStatUpgradeDefinition.Get()
		: LoadStatUpgradeDefinition();
	UPdAbilitySystemComponent* ASC = GetOwnerPdAbilitySystemComponent();
	if (!LoadedDefinition || !ASC)
	{
		return false;
	}

	float StepPercent = PerUpgradePercent.IsSet() ? PerUpgradePercent.GetValue() : 0.f;
	if (StepPercent <= UE_KINDA_SMALL_NUMBER
		&& !LoadedDefinition->TryGetAttributeValuePerUpgrade(CompoundedStat.StatTag, StepPercent))
	{
		return false;
	}

	const float CurrentPercent = FMath::Max(ASC->GetNumericAttribute(CompoundedStat.ValueAttribute), 0.f);
	const float InvestmentLevel = CompoundedStat.LevelAttribute.IsValid()
		? FMath::Max(ASC->GetNumericAttribute(CompoundedStat.LevelAttribute), 0.f)
		: 0.f;
	const float CompoundedPercent = CalculateCompoundedIncreasePercent(StepPercent, InvestmentLevel);
	if (FMath::IsNearlyEqual(CurrentPercent, CompoundedPercent))
	{
		return false;
	}

	return ASC->ApplyAttributeDefaultValue(CompoundedStat.ValueAttribute, CompoundedPercent);
}

bool UStatUpgradeComponent::ApplyStatUpgradeEffects(TSubclassOf<UGameplayEffect> GameplayEffectClass, const TMap<FGameplayTag, float>& StatMagnitudes,
	EEnum_Operation Operation, float Level)
{
	if (!GameplayEffectClass || StatMagnitudes.IsEmpty())
	{
		return false;
	}

	UPdAbilitySystemComponent* ASC = GetOwnerPdAbilitySystemComponent();
	if (!ASC)
	{
		return false;
	}

	return ASC->ApplyStatUpEffectByTags(GameplayEffectClass, StatMagnitudes, Operation, Level);
}

UPdAbilitySystemComponent* UStatUpgradeComponent::GetOwnerPdAbilitySystemComponent() const
{
	const APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetPdAbilitySystemComponent() : nullptr;
}

TSubclassOf<UGameplayEffect> UStatUpgradeComponent::GetEquipmentStatGameplayEffectClass() const
{
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	return SettingDefinition ? SettingDefinition->EquipmentStatGameplayEffectClass : nullptr;
}

void UStatUpgradeComponent::BindRecoveryAttributeChanged()
{
	UPdAbilitySystemComponent* ASC = GetOwnerPdAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (!RecoveryAttributeChangedDelegateHandle.IsValid())
	{
		RecoveryAttributeChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetRecoveryAttribute())
			.AddUObject(this, &ThisClass::HandleRecoveryAttributeChanged);
	}

	if (!RecoveryMaxHealthAttributeChangedDelegateHandle.IsValid())
	{
		RecoveryMaxHealthAttributeChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute())
			.AddUObject(this, &ThisClass::HandleRecoveryAttributeChanged);
	}

	if (!RecoveryMaxManaAttributeChangedDelegateHandle.IsValid())
	{
		RecoveryMaxManaAttributeChangedDelegateHandle = ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaAttribute())
			.AddUObject(this, &ThisClass::HandleRecoveryAttributeChanged);
	}
}

void UStatUpgradeComponent::UnbindRecoveryAttributeChanged()
{
	if (!RecoveryAttributeChangedDelegateHandle.IsValid()
		&& !RecoveryMaxHealthAttributeChangedDelegateHandle.IsValid()
		&& !RecoveryMaxManaAttributeChangedDelegateHandle.IsValid())
	{
		return;
	}

	if (UPdAbilitySystemComponent* ASC = GetOwnerPdAbilitySystemComponent())
	{
		if (RecoveryAttributeChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetRecoveryAttribute())
				.Remove(RecoveryAttributeChangedDelegateHandle);
		}

		if (RecoveryMaxHealthAttributeChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute())
				.Remove(RecoveryMaxHealthAttributeChangedDelegateHandle);
		}

		if (RecoveryMaxManaAttributeChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxManaAttribute())
				.Remove(RecoveryMaxManaAttributeChangedDelegateHandle);
		}
	}

	RecoveryAttributeChangedDelegateHandle.Reset();
	RecoveryMaxHealthAttributeChangedDelegateHandle.Reset();
	RecoveryMaxManaAttributeChangedDelegateHandle.Reset();
}

void UStatUpgradeComponent::HandleRecoveryAttributeChanged(const FOnAttributeChangeData& Data)
{
	(void)Data;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	StopRecoveryHealthRegen();
	StartRecoveryHealthRegen();
}

void UStatUpgradeComponent::StartRecoveryHealthRegen()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	BindRecoveryAttributeChanged();
	StopRecoveryHealthRegen();
	ApplyRecoveryHealthRegenEffect();
}

void UStatUpgradeComponent::StopRecoveryHealthRegen()
{
	if (!RecoveryHealthRegenEffectHandle.IsValid())
	{
		return;
	}

	if (UPdAbilitySystemComponent* ASC = GetOwnerPdAbilitySystemComponent())
	{
		ASC->RemoveActiveGameplayEffect(RecoveryHealthRegenEffectHandle);
	}

	RecoveryHealthRegenEffectHandle.Invalidate();
}

void UStatUpgradeComponent::ApplyRecoveryHealthRegenEffect()
{
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const TSubclassOf<UGameplayEffect> RecoveryHealEffectClass =
		SettingDefinition
			? SettingDefinition->RecoveryHealGameplayEffectClass
			: nullptr;
	if (!RecoveryHealEffectClass)
	{
		StopRecoveryHealthRegen();
		return;
	}

	UPdAbilitySystemComponent* ASC = GetOwnerPdAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (RecoveryHealthRegenEffectHandle.IsValid())
	{
		return;
	}

	const float RecoveryPercent = FMath::Max(
		ASC->GetNumericAttribute(UBasicAttributeSet::GetRecoveryAttribute()),
		0.0f);
	if (RecoveryPercent <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(RecoveryHealEffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}

	const float MaxHealth = FMath::Max(ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxHealthAttribute()), 0.0f);
	const float MaxMana = FMath::Max(ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxManaAttribute()), 0.0f);
	const float RecoveryHealValue = MaxHealth * RecoveryPercent * 0.01f;
	const float RecoveryManaValue = MaxMana * RecoveryPercent * 0.01f;
	SpecHandle.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_Heal, RecoveryHealValue);
	SpecHandle.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_Mana, RecoveryManaValue);

	RecoveryHealthRegenEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	if (!RecoveryHealthRegenEffectHandle.WasSuccessfullyApplied())
	{
		RecoveryHealthRegenEffectHandle.Invalidate();
	}
}
