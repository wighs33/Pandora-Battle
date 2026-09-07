#include "Component/AbilitySystem/PdAbilitySystemComponent.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/AbilityAttributeRuntime.h"
#include "Component/AbilitySystem/AbilityCollectionRuntime.h"
#include "Engine/World.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAbilitySystemComponent)

// 캐릭터의 스탯·능력·입력과 GAS를 연결하는 공통 창구다.
// 스탯 처리는 AttributeRuntime에, 능력 목록과 입력 처리는 CollectionRuntime에 맡긴다.

// 캐릭터의 능력 상태를 네트워크로 공유하도록 설정하고, 스탯과 능력 목록을 관리할 내부 객체를 준비한다.
UPdAbilitySystemComponent::UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeRuntime = CreateDefaultSubobject<UAbilityAttributeRuntime>(TEXT("AttributeRuntime"));
	CollectionRuntime = CreateDefaultSubobject<UAbilityCollectionRuntime>(TEXT("CollectionRuntime"));
}

// 능력이 부여되면 판도라 출처 정보가 사라지지 않도록 보관하고, 스킬바 등 구독자에게 변경을 알린다.
void UPdAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	CollectionRuntime->CachePandoraSkillRuntimeContext(*this, AbilitySpec.SourceObject.Get());

	Super::OnGiveAbility(AbilitySpec);
	NotifyAbilitiesChanged();
}

// 능력이 회수될 때 남은 연출을 정리하고, 다른 능력도 사용하지 않는 판도라 출처 정보의 보관을 해제한다.
// 제거 이후에는 스킬바 등 구독자에게 목록 변경을 알린다.
void UPdAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	UPandoraSkillRuntimeContext* RemovedRuntimeContext = Cast<UPandoraSkillRuntimeContext>(AbilitySpec.SourceObject.Get());
	const FGameplayAbilitySpecHandle RemovedHandle = AbilitySpec.Handle;
	PendingSourceActivations.RemoveAll([RemovedHandle](const FPendingAbilityInfo& Pending) { return Pending.Handle == RemovedHandle; });

	for (UGameplayAbility* AbilityInstance : AbilitySpec.GetAbilityInstances())
	{
		if (!IsValid(AbilityInstance))
		{
			continue;
		}

		if (UPdGameplayAbility* PdAbilityInstance = Cast<UPdGameplayAbility>(AbilityInstance))
		{
			PdAbilityInstance->SuppressPendingCooldownForRuntimeReset();
			PdAbilityInstance->CleanupConfiguredPresentation();
		}
	}

	Super::OnRemoveAbility(AbilitySpec);

	CollectionRuntime->ReleasePandoraSkillRuntimeContextIfUnused(*this, RemovedRuntimeContext, RemovedHandle);

	NotifyAbilitiesChanged();
}

// 짧게 눌렀다 뗀 Press 스킬이 서버 응답 지연으로 조준 대기에 갇히지 않도록, 활성화 후 입력 해제 보정을 예약한다.
void UPdAbilitySystemComponent::NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	Super::NotifyAbilityActivated(Handle, Ability);

	const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(Handle);
	const UPdGameplayAbility* PdAbility = Cast<UPdGameplayAbility>(Ability);
	if (!AbilityActorInfo.IsValid()
		|| !AbilityActorInfo->IsLocallyControlled()
		|| !AbilitySpec
		|| AbilitySpec->InputPressed
		|| (PdAbility && !PdAbility->ShouldAutoConfirmOnInputRelease())
		|| !AbilitySpec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Skill_Type_Press))
	{
		return;
	}

	// 서버의 활성화 응답보다 입력 해제가 먼저 도착할 수 있다.
	// 타기팅 Task가 만들어진 다음 틱에 해제를 재전달해 확정 대기가 남지 않게 한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::ReplayReleasedPressInputAfterActivation, Handle));
	}
}

// 예약한 다음 틱에 대상 스킬의 상태를 다시 확인하고, 아직 필요한 시전 확정과 입력 해제 처리를 전달한다.
void UPdAbilitySystemComponent::ReplayReleasedPressInputAfterActivation(const FGameplayAbilitySpecHandle AbilityHandle)
{
	CollectionRuntime->ReplayReleasedPressInputAfterActivation(*this, AbilityHandle);
}

// 부여 목록뿐 아니라 입력 연결이나 출처가 뒤늦게 도착한 경우에도 스킬바를 갱신한다.
void UPdAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		CollectionRuntime->CachePandoraSkillRuntimeContext(*this, Spec.SourceObject.Get());
	}
	ReplaySourceReadyActivations();
	NotifyAbilitiesChanged();
}

// 게임피처나 적 설정이 제공한 '스탯 태그 → 실제 속성' 연결 규칙을 등록하고, 나중에 해제할 때 쓸 번호를 돌려준다.
int32 UPdAbilitySystemComponent::AddAttributeConfig(const FAttributeConfig& AttributeConfig)
{
	return AttributeRuntime->AddAttributeConfig(AttributeConfig);
}

// 기능이 해제될 때 해당 기능이 등록한 스탯 연결 규칙만 제거한다. 이미 적용된 스탯 수치를 되돌리는 함수는 아니다.
void UPdAbilitySystemComponent::RemoveAttributeConfig(const int32 AttributeConfigHandle)
{
	AttributeRuntime->RemoveAttributeConfig(AttributeConfigHandle);
}

// 속성 초기화가 투자 컴포넌트의 계산 로직에 의존하지 않도록 기존 속성 Runtime에 맡긴다.
bool UPdAbilitySystemComponent::ApplyConfiguredAttributeDefaults(const UStatUpgradeDefinition& Definition)
{
	return AttributeRuntime->ApplyConfiguredAttributeDefaults(*this, Definition);
}

// 속성 기본값을 설정하고, 버프를 포함한 현재값 계산은 GAS에 맡긴다.
bool UPdAbilitySystemComponent::ApplyAttributeDefaultValue(const FGameplayAttribute& Attribute, const float DefaultValue)
{
	return AttributeRuntime->ApplyAttributeDefaultValue(*this, Attribute, DefaultValue);
}

// 스탯 컴포넌트가 다시 초기화되어도 성장한 수치를 기본값으로 덮어쓰지 않도록, 같은 설정의 적용 기록을 확인한다.
bool UPdAbilitySystemComponent::HasAppliedConfiguredAttributeDefaults(
	const UAttributeSet* AttributeSet,
	const FSoftObjectPath& DefinitionPath) const
{
	return AttributeRuntime->HasAppliedConfiguredAttributeDefaults(AttributeSet, DefinitionPath);
}

// 어떤 AttributeSet에 어떤 기본 스탯 설정을 적용하는지 기록해 중복 초기화를 막는다. 여기서 수치를 변경하지는 않는다.
void UPdAbilitySystemComponent::MarkConfiguredAttributeDefaultsApplied(UAttributeSet* AttributeSet, const FSoftObjectPath& DefinitionPath)
{
	AttributeRuntime->MarkConfiguredAttributeDefaultsApplied(AttributeSet, DefinitionPath);
}

// 기본값 적용 실패 등으로 다시 초기화해야 할 때, 대상과 일치하는 적용 기록만 지운다. 현재 스탯 수치는 유지한다.
void UPdAbilitySystemComponent::ClearConfiguredAttributeDefaultsApplied(const UAttributeSet* AttributeSet, const FSoftObjectPath& DefinitionPath)
{
	AttributeRuntime->ClearConfiguredAttributeDefaultsApplied(AttributeSet, DefinitionPath);
}

// 공격·스킬 입력 태그에 연결된 능력의 실행을 시도하거나, 이미 실행 중인 능력에 추가 누름 입력을 전달한다.
void UPdAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	CollectionRuntime->AbilityInputTagPressed(*this, InputTag);
}

// 키를 놓았다는 입력은 처음 누른 능력에 전달한다. 중간에 판도라를 바꿨더라도 원래 시전의 입력을 해제한다.
void UPdAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	CollectionRuntime->AbilityInputTagReleased(*this, InputTag);
}

// 장비 교체·스킬 사용 등의 상태 판단에 필요한, 지정 태그 중 하나와 일치하는 첫 번째 실행 중 능력을 찾는다.
const FGameplayAbilitySpec* UPdAbilitySystemComponent::FindActiveAbilitySpecByTags(const FGameplayTagContainer& AbilityTags) const
{
	return CollectionRuntime->FindActiveAbilitySpecByTags(*this, AbilityTags);
}

// 동작 간 충돌을 판단할 때, 지정한 종류의 능력이 실행 중인지만 확인한다. 부여되어 있지만 쉬고 있는 능력은 제외한다.
bool UPdAbilitySystemComponent::HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const
{
	return FindActiveAbilitySpecByTags(AbilityTags) != nullptr;
}

// 특정 스킬 클래스의 동작이 진행 중인지 확인한다. 파생 스킬까지 같은 동작으로 볼지는 호출자가 선택한다.
bool UPdAbilitySystemComponent::HasActiveAbilityOfClass(const TSubclassOf<UGameplayAbility> AbilityClass, const bool bIncludeChildClasses) const
{
	return CollectionRuntime->HasActiveAbilityOfClass(*this, AbilityClass, bIncludeChildClasses);
}

// 여러 스킬을 하나의 동작 범주로 묶어 실행 여부를 확인한다. 적 AI는 근접·원거리·주먹 공격 중 하나라도 진행 중인지 판단한다.
bool UPdAbilitySystemComponent::HasActiveAbilityOfAnyClass(
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
	const bool bIncludeChildClasses) const
{
	return CollectionRuntime->HasActiveAbilityOfAnyClass(*this, AbilityClasses, bIncludeChildClasses);
}

// 하나의 스탯 태그와 변화량을 받아, 지정한 연산 방식의 GameplayEffect를 서버에서 자신에게 적용한다.
bool UPdAbilitySystemComponent::ApplyStatUpEffectByTag(
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const FGameplayTag StatTag,
	const float Magnitude,
	const EEnum_Operation Operation,
	const float Level)
{
	return AttributeRuntime->ApplyStatUpEffectByTag(*this, GameplayEffectClass, StatTag, Magnitude, Operation, Level);
}

// 여러 스탯의 변화량을 하나의 GameplayEffect에 담아 서버에서 자신에게 적용한다. 각 변화량과 연산 방식은 태그로 전달한다.
bool UPdAbilitySystemComponent::ApplyStatUpEffectByTags(
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const TMap<FGameplayTag, float>& StatMagnitudes,
	const EEnum_Operation Operation,
	const float Level)
{
	return AttributeRuntime->ApplyStatUpEffectByTags(*this, GameplayEffectClass, StatMagnitudes, Operation, Level);
}

// 서버에서 캐릭터가 사용할 능력을 중복 없이 부여하고, 나중에 회수할 핸들을 돌려준다.
// 자동 실행 대상으로 설정된 능력은 부여 후 실행도 시도한다.
TArray<FGameplayAbilitySpecHandle> UPdAbilitySystemComponent::GrantAbilities(
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
	const int32 AbilityLevel,
	UObject* SourceObject)
{
	return CollectionRuntime->GrantAbilities(*this, AbilityClasses, AbilityLevel, SourceObject);
}

// 기능 해제나 구성 변경 시 서버에서 지정한 능력의 부여를 회수한다. 시전만 중단하는 리셋과 달리 능력 목록에서도 제거한다.
void UPdAbilitySystemComponent::RemoveAbilities(const TArray<FGameplayAbilitySpecHandle>& AbilityHandles)
{
	CollectionRuntime->RemoveAbilities(*this, AbilityHandles);
}

// 부활 후에도 계속 작동해야 하는 자동 실행 능력을 다시 켜도록 시도한다. 이미 실행 중이거나 아직 사망 상태라면 건드리지 않는다.
void UPdAbilitySystemComponent::ReactivateAutoActivatedAbilities()
{
	CollectionRuntime->ReactivateAutoActivatedAbilities(*this);
}

// 체력 같은 스탯 태그를 GAS가 읽고 변경할 실제 속성으로 바꾼다. 기능별 연결 규칙을 우선하고, 없으면 기본 스탯 연결을 사용한다.
bool UPdAbilitySystemComponent::ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const
{
	if (AttributeRuntime->ResolveAttributeFromTag(StatTag, OutAttribute))
	{
		return true;
	}

	return UBasicAttributeSet::ResolveAttributeFromStatTag(StatTag, OutAttribute);
}

// 공격 피해량을 GameplayEffect에 전달할 때 사용할 공통 태그를 찾는다. 피해량 자체를 계산하거나 적용하지는 않는다.
bool UPdAbilitySystemComponent::ResolveDamageMagnitudeSetByCallerTag(FGameplayTag& OutTag) const
{
	return AttributeRuntime->ResolveDamageMagnitudeSetByCallerTag(*this, OutTag);
}

// 스탯 효과에 더하기·곱하기 등 어떤 연산을 할지 전달하기 위한 공통 태그를 찾는다.
bool UPdAbilitySystemComponent::ResolveStatUpOperationSetByCallerTag(FGameplayTag& OutTag) const
{
	return AttributeRuntime->ResolveStatUpOperationSetByCallerTag(*this, OutTag);
}

// 능력 구성이나 리셋 결과를 스킬바 등 구독자에게 알린다. 델리게이트 구독자와 GAS 이벤트 수신자가 각자 표시·상태를 갱신한다.
void UPdAbilitySystemComponent::NotifyAbilitiesChanged()
{
	OnAbilitiesChanged.Broadcast();
	OnAbilitiesChangedNative.Broadcast();

	FGameplayEventData EventData;
	EventData.EventTag = LabGameplayTags::Event_Abilities_Changed;
	EventData.Instigator = GetAvatarActor();
	EventData.Target = GetAvatarActor();
	HandleGameplayEvent(LabGameplayTags::Event_Abilities_Changed, &EventData);
}

// 사망 능력은 유지하고 다른 시전을 취소한다. 종료 중 쿨다운 재생성을 막고, 이미 적용된 쿨다운도 제거한다.
void UPdAbilitySystemComponent::ResetAbilityRuntimeStateForDeath()
{
	CollectionRuntime->ClearPressedAbilityInputs();
	PendingSourceActivations.Reset();
	TArray<FGameplayAbilitySpecHandle> ActiveAbilityHandles;
	{
		FScopedAbilityListLock AbilityListLock(*this);
		for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
		{
			if (!AbilitySpec.Ability)
			{
				continue;
			}

			AbilitySpec.InputPressed = false;
			if (!AbilitySpec.IsActive()
				|| AbilitySpec.Ability->GetAssetTags().HasTagExact(LabGameplayTags::GameplayAbility_Death))
			{
				continue;
			}

			for (UGameplayAbility* AbilityInstance : AbilitySpec.GetAbilityInstances())
			{
				if (!IsValid(AbilityInstance))
				{
					continue;
				}

				if (UPdGameplayAbility* PdAbilityInstance = Cast<UPdGameplayAbility>(AbilityInstance))
				{
					PdAbilityInstance->SuppressPendingCooldownForRuntimeReset();
					PdAbilityInstance->CleanupConfiguredPresentation();
				}

				// 복제 목록에 초기화되지 않은 인스턴스가 남을 수 있어, 이 ASC에 연결된 활성 인스턴스만 취소 가능하게 만든다.
				const FGameplayAbilityActorInfo* InstanceActorInfo = AbilityInstance->GetCurrentActorInfo();
				if (AbilityInstance->IsActive() && InstanceActorInfo && InstanceActorInfo->AbilitySystemComponent.Get() == this)
				{
					AbilityInstance->SetCanBeCanceled(true);
				}
			}

			ActiveAbilityHandles.Add(AbilitySpec.Handle);
		}
	}

	{
		// 취소로 호출되는 능력의 종료 처리가 일반 종료와 리셋을 구분할 수 있도록, 취소가 진행되는 동안만 표시한다.
		TGuardValue<bool> ResetGuard(bResettingAbilityRuntimeState, true);
		for (const FGameplayAbilitySpecHandle& AbilityHandle : ActiveAbilityHandles)
		{
			CancelAbilityHandle(AbilityHandle);
		}
	}

	FGameplayTagContainer EffectTags(LabGameplayTags::Effect_Policy_RemoveOnDeath);
	EffectTags.AddTag(LabGameplayTags::Cooldown);
	const UGameSettingDefinition* Settings = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	RemoveRuntimeEffects(EffectTags, FGameplayTagContainer(LabGameplayTags::Cooldown), FGameplayTagContainer(),
		Settings ? Settings->DeathGameplayCuesToRemove : FGameplayTagContainer());
	NotifyAbilitiesChanged();
}


// 부활 전 이전 목숨의 디버프와 상태 이상을 정리한다. 체력 회복과 능력 재시작은 호출자가 담당한다.
int32 UPdAbilitySystemComponent::ClearStatusEffectsForRespawn()
{
	FGameplayTagContainer StatusTags(LabGameplayTags::Debuff);
	StatusTags.AddTag(LabGameplayTags::Status_Burning);
	StatusTags.AddTag(LabGameplayTags::Status_Frostbite);
	StatusTags.AddTag(LabGameplayTags::Status_ElectricShock);
	FGameplayTagContainer EffectTags = StatusTags;
	EffectTags.AddTag(LabGameplayTags::Effect_Policy_RemoveOnRespawn);
	const UGameSettingDefinition* Settings = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	return RemoveRuntimeEffects(EffectTags, StatusTags, StatusTags,
		Settings ? Settings->RespawnGameplayCuesToRemove : FGameplayTagContainer());
}

// 서버에서 지정된 효과·직접 부여한 상태 태그·지속 연출을 제거한다. 반환값은 제거된 GameplayEffect 수다.
int32 UPdAbilitySystemComponent::RemoveRuntimeEffects(
	const FGameplayTagContainer& EffectTags, const FGameplayTagContainer& OwnedTags,
	const FGameplayTagContainer& LooseTags, const FGameplayTagContainer& GameplayCues)
{
	if (!IsOwnerActorAuthoritative())
	{
		return 0;
	}

	const int32 InitialCount = GetNumActiveGameplayEffects();
	if (!EffectTags.IsEmpty())
	{
		RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(EffectTags));
	}
	if (!OwnedTags.IsEmpty())
	{
		RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(OwnedTags));
	}
	FGameplayTagContainer CurrentTags;
	GetOwnedGameplayTags(CurrentTags);
	for (const FGameplayTag& Tag : CurrentTags)
	{
		if (Tag.MatchesAny(LooseTags))
		{
			SetLooseGameplayTagCount(Tag, 0, EGameplayTagReplicationState::CountToOwner);
		}
	}
	for (const FGameplayTag& Cue : GameplayCues)
	{
		RemoveGameplayCue(Cue);
	}
	ForceReplication();
	return FMath::Max(InitialCount - GetNumActiveGameplayEffects(), 0);
}

// 복제 시작 전에 부여된 판도라 출처도 ASC의 복제 하위 객체로 등록한다.
void UPdAbilitySystemComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		CollectionRuntime->CachePandoraSkillRuntimeContext(*this, Spec.SourceObject.Get());
	}
}

// 출처가 능력 목록보다 늦게 도착해도 이전 판도라의 데이터로 실행하지 않고, 정확한 원본이 준비되면 이어서 실행한다.
void UPdAbilitySystemComponent::NotifyPandoraSourceReplicated(UPandoraSkillRuntimeContext* Source)
{
	CollectionRuntime->CachePandoraSkillRuntimeContext(*this, Source);
	ReplaySourceReadyActivations();
	NotifyAbilitiesChanged();
}

// 서버의 실행 통지가 출처 복제보다 먼저 도착하면 실행 정보만 잠시 보관한다.
void UPdAbilitySystemComponent::ClientActivateAbilitySucceedWithEventData_Implementation(
	FGameplayAbilitySpecHandle Handle, FPredictionKey PredictionKey, FGameplayEventData TriggerEventData)
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora))
	{
		const UPandoraSkillRuntimeContext* Source = Cast<UPandoraSkillRuntimeContext>(Spec->SourceObject.Get());
		if (!Source || !Source->IsSourceReady())
		{
			FPendingAbilityInfo Pending;
			Pending.Handle = Handle;
			Pending.PredictionKey = PredictionKey;
			Pending.TriggerEventData = TriggerEventData;
			Pending.bPartiallyActivated = true;
			PendingSourceActivations.AddUnique(Pending);
			return;
		}
	}
	Super::ClientActivateAbilitySucceedWithEventData_Implementation(Handle, PredictionKey, TriggerEventData);
}

// 원본이 준비된 실행 통지만 재개한다. 아직 준비되지 않은 다른 스킬은 대기 상태를 유지한다.
void UPdAbilitySystemComponent::ReplaySourceReadyActivations()
{
	TArray<FPendingAbilityInfo> ReadyActivations;
	for (int32 Index = PendingSourceActivations.Num() - 1; Index >= 0; --Index)
	{
		const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(PendingSourceActivations[Index].Handle);
		const UPandoraSkillRuntimeContext* Source = Spec ? Cast<UPandoraSkillRuntimeContext>(Spec->SourceObject.Get()) : nullptr;
		if (Spec && Spec->Ability && Source && Source->IsSourceReady())
		{
			ReadyActivations.Add(PendingSourceActivations[Index]);
			PendingSourceActivations.RemoveAt(Index);
		}
	}
	for (const FPendingAbilityInfo& Pending : ReadyActivations)
	{
		Super::ClientActivateAbilitySucceedWithEventData_Implementation(Pending.Handle, Pending.PredictionKey, Pending.TriggerEventData);
	}
}

// 출처를 기다리는 동안 서버에서 이미 끝난 시전이 뒤늦게 시작되지 않도록 대기 정보도 지운다.
void UPdAbilitySystemComponent::ClientEndAbility_Implementation(
	FGameplayAbilitySpecHandle Handle, FGameplayAbilityActivationInfo ActivationInfo)
{
	PendingSourceActivations.RemoveAll([&](const FPendingAbilityInfo& Pending)
	{
		return Pending.Handle == Handle && Pending.PredictionKey == ActivationInfo.GetActivationPredictionKey();
	});
	Super::ClientEndAbility_Implementation(Handle, ActivationInfo);
}

// 사망·피격 등 서버 취소가 도착하면 아직 실행하지 못한 시전도 취소한다.
void UPdAbilitySystemComponent::ClientCancelAbility_Implementation(
	FGameplayAbilitySpecHandle Handle, FGameplayAbilityActivationInfo ActivationInfo)
{
	PendingSourceActivations.RemoveAll([&](const FPendingAbilityInfo& Pending)
	{
		return Pending.Handle == Handle && Pending.PredictionKey == ActivationInfo.GetActivationPredictionKey();
	});
	Super::ClientCancelAbility_Implementation(Handle, ActivationInfo);
}
