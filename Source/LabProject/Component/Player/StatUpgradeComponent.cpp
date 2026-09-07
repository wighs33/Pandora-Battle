#include "Component/Player/StatUpgradeComponent.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "GameFramework/Actor.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Definition/Player/StatUpgradeDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatUpgradeComponent)

DEFINE_LOG_CATEGORY(StatUpgradeComponentLog);

UStatUpgradeComponent::UStatUpgradeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	StatUpgradeDefinition = TSoftObjectPtr<UStatUpgradeDefinition>(
		UStatUpgradeDefinition::GetDefaultDefinitionPath());
}

// 경기 시작에 필요한 투자 규칙을 미리 불러온다.
void UStatUpgradeComponent::BeginPlay()
{
	Super::BeginPlay();

	BeginStatUpgradeDefinitionPreload();
}

// 종료된 플레이어에게 이전 비동기 로딩 결과가 적용되지 않도록 정리한다.
void UStatUpgradeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseStatUpgradeDefinitionPreload();
	LoadedStatUpgradeDefinition = nullptr;
	bApplyDefaultsWhenDefinitionReady = false;

	Super::EndPlay(EndPlayReason);
}

// 투자 입력을 서버로 보내고, 서버에서 호출했으면 즉시 검증하고 반영한다.
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

	return ApplyStatChange(StatTag, 1);
}

// 환불 입력을 서버로 보내거나 서버에서 직접 처리한다.
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

	return ApplyStatChange(StatTag, -1);
}

// 경기 설정에 따라 여섯 분류의 사용 가능한 투자 포인트를 초기화한다.
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
	UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
	const UBasicAttributeSet* AttributeSet = ASC ? ASC->GetSet<UBasicAttributeSet>() : nullptr;
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

// 클라이언트가 요청한 투자를 서버 규칙으로 검증한다.
void UStatUpgradeComponent::ServerRequestStatUp_Implementation(FGameplayTag StatTag)
{
	ApplyStatChange(StatTag, 1);
}

// 클라이언트가 요청한 환불을 서버 규칙으로 검증한다.
void UStatUpgradeComponent::ServerRequestStatDown_Implementation(FGameplayTag StatTag)
{
	ApplyStatChange(StatTag, -1);
}

// 투자·환불의 변경량을 먼저 검증한 뒤 비용, 투자 레벨, 투자분을 한 GE로 반영한다.
bool UStatUpgradeComponent::ApplyStatChange(FGameplayTag StatTag, const int32 LevelDelta)
{
	AActor* OwnerActor = GetOwner();
	UPdAbilitySystemComponent* ASC = Cast<UPdAbilitySystemComponent>(GetOwnerAbilitySystemComponent());
	if (!OwnerActor || !OwnerActor->HasAuthority() || !ASC || bApplyingStatChange)
	{
		return false;
	}
	TGuardValue<bool> ApplyingChange(bApplyingStatChange, true);

	const UStatUpgradeDefinition* Definition = LoadStatUpgradeDefinition();
	const FStatUpgradeBinding* Binding = UStatUpgradeDefinition::FindStatBinding(StatTag);
	const FStatUpgradeRule* Rule = Definition && Binding ? Definition->FindUpgradeRuleForStat(StatTag) : nullptr;
	const UGameSettingDefinition* Settings = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	if (!Rule || !Settings || !Settings->EquipmentStatGameplayEffectClass || !ASC->GetSet<UBasicAttributeSet>())
	{
		return false;
	}

	FGameplayAttribute LevelAttribute;
	float Magnitude = 0.f;
	if (!ASC->ResolveAttributeFromTag(Binding->LevelTag, LevelAttribute)
		|| !Definition->TryGetUpgradeMagnitude(*Binding, Magnitude))
	{
		return false;
	}

	const float OldLevel = ASC->GetNumericAttributeBase(LevelAttribute);
	const float NewLevel = OldLevel + LevelDelta;
	if (!FMath::IsFinite(NewLevel) || NewLevel < 0.f
		|| (LevelDelta > 0 && NewLevel > FMath::FloorToFloat(Definition->GetMaxInvestedLevel())))
	{
		return false;
	}

	const float Cost = Rule->Cost;
	if (!FMath::IsFinite(Cost) || Cost < 0.f)
	{
		return false;
	}
	if (Cost > 0.f)
	{
		FGameplayAttribute PointAttribute;
		if (!ASC->ResolveAttributeFromTag(Rule->CostPointTag, PointAttribute)
			|| (LevelDelta > 0 && ASC->GetNumericAttributeBase(PointAttribute) + UE_KINDA_SMALL_NUMBER < Cost))
		{
			return false;
		}
	}

	const float OldInvestment = UStatUpgradeDefinition::CalculateInvestmentValue(Magnitude, OldLevel, Binding->bCompounded);
	const float NewInvestment = UStatUpgradeDefinition::CalculateInvestmentValue(Magnitude, NewLevel, Binding->bCompounded);
	const float InvestmentDelta = NewInvestment - OldInvestment;
	if (!FMath::IsFinite(InvestmentDelta) || FMath::IsNearlyZero(InvestmentDelta))
	{
		return false;
	}

	TMap<FGameplayTag, float> StatMagnitudes;
	StatMagnitudes.Add(Binding->GetEffectTag(), InvestmentDelta);
	StatMagnitudes.Add(Binding->LevelTag, static_cast<float>(LevelDelta));
	if (Cost > 0.f)
	{
		StatMagnitudes.FindOrAdd(Rule->CostPointTag) -= Cost * LevelDelta;
	}

	FGameplayAttribute MaxAttribute;
	FGameplayAttribute CurrentAttribute;
	float OldMax = 0.f;
	float OldCurrent = 0.f;
	if (Binding->IsMaxResource())
	{
		float ResourceBase = 0.f;
		if (!Definition->TryGetResourceBaseValue(*Binding, ResourceBase)
			|| !ASC->ResolveAttributeFromTag(Binding->StatTag, MaxAttribute)
			|| !ASC->ResolveAttributeFromTag(Binding->CurrentResourceTag, CurrentAttribute))
		{
			return false;
		}

		// 기본값 × 투자배율 + 장비보너스. 장비가 더한 값과 지속 중인 버프는 덮어쓰지 않는다.
		StatMagnitudes.Add(Binding->StatTag, ResourceBase * InvestmentDelta * 0.01f);
		OldMax = ASC->GetNumericAttribute(MaxAttribute);
		OldCurrent = ASC->GetNumericAttribute(CurrentAttribute);
	}

	for (const TPair<FGameplayTag, float>& Change : StatMagnitudes)
	{
		FGameplayAttribute Attribute;
		if (!FMath::IsFinite(Change.Value) || !ASC->ResolveAttributeFromTag(Change.Key, Attribute)
			|| !ASC->HasAttributeSetForAttribute(Attribute)
			|| !FMath::IsFinite(ASC->GetNumericAttributeBase(Attribute) + Change.Value))
		{
			return false;
		}
	}

	if (!ASC->ApplyStatUpEffectByTags(Settings->EquipmentStatGameplayEffectClass, StatMagnitudes))
	{
		UE_LOG(StatUpgradeComponentLog, Error, TEXT("Failed to apply stat investment change: %s"), *StatTag.ToString());
		return false;
	}

	// 최대 자원이 달라져도 현재 자원의 비율을 유지한다. 최종 최대값은 GAS에서 다시 읽는다.
	if (Binding->IsMaxResource())
	{
		const float NewMax = FMath::Max(ASC->GetNumericAttribute(MaxAttribute), 0.f);
		const float NewCurrent = OldMax <= UE_KINDA_SMALL_NUMBER || OldCurrent >= OldMax - 1.f
			? NewMax : FMath::Clamp(OldCurrent * NewMax / OldMax, 0.f, NewMax);
		ASC->SetNumericAttributeBase(CurrentAttribute, NewCurrent);
	}
	return true;
}

// 준비된 투자 규칙을 반환하고, 아직 로딩 전이라면 비동기 로딩을 시작한다.
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

// 현재 정의만 로딩하고 이전 요청의 완료 콜백은 세대 번호로 구분한다.
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

// 정의 준비를 기다리던 초기화를 이어가고 완료된 로딩 핸들을 해제한다.
void UStatUpgradeComponent::HandleStatUpgradeDefinitionPreloaded(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != StatUpgradeDefinitionLoadGeneration)
	{
		return;
	}

	LoadedStatUpgradeDefinition = StatUpgradeDefinition.Get();
	if (StatUpgradeDefinitionLoadHandle.IsValid())
	{
		StatUpgradeDefinitionLoadHandle->ReleaseHandle();
		StatUpgradeDefinitionLoadHandle.Reset();
	}
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

// 취소된 로딩이 나중에 완료되어도 현재 플레이어 상태를 변경하지 못하게 한다.
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

// 정의의 비동기 준비를 연결하고, 실제 기본값 적용은 기존 속성 Runtime에 위임한다.
bool UStatUpgradeComponent::ApplyConfiguredAttributeDefaults()
{
	UStatUpgradeDefinition* Definition = LoadStatUpgradeDefinition();
	if (!Definition)
	{
		bApplyDefaultsWhenDefinitionReady = !StatUpgradeDefinition.IsNull();
		return false;
	}
	bApplyDefaultsWhenDefinitionReady = false;
	UPdAbilitySystemComponent* ASC = Cast<UPdAbilitySystemComponent>(GetOwnerAbilitySystemComponent());
	TGuardValue<bool> ApplyingDefaults(bApplyingStatChange, true);
	return ASC && ASC->ApplyConfiguredAttributeDefaults(*Definition);
}

// 플레이어 상태가 소유한 ASC를 조회한다.
UAbilitySystemComponent* UStatUpgradeComponent::GetOwnerAbilitySystemComponent() const
{
	const APdPlayerState* PlayerState = Cast<APdPlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
}
