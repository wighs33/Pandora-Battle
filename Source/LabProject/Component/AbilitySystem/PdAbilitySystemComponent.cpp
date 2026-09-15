#include "Component/AbilitySystem/PdAbilitySystemComponent.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/AbilityGrantAndInputManager.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Definition/Player/StatUpgradeDefinition.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Pandora/PandoraSkillSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAbilitySystemComponent)

// 캐릭터의 스탯·능력·입력과 GAS를 연결하는 공통 창구다.
// 스탯 적용은 직접 처리하고, 태그 정의는 AttributeSet과 ProjectTagConfig에서 조회한다.
// 능력 목록과 입력 처리는 AbilityGrantAndInputManager에 맡긴다.

// 캐릭터의 능력 상태를 네트워크로 공유하도록 설정하고, 능력 목록과 입력을 관리할 내부 객체를 준비한다.
UPdAbilitySystemComponent::UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AbilityGrantAndInputManager = CreateDefaultSubobject<UAbilityGrantAndInputManager>(TEXT("AbilityGrantAndInputManager"));
}

// 능력이 부여되면 판도라 출처 정보가 사라지지 않도록 보관하고, 스킬바 등 구독자에게 변경을 알린다.
void UPdAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	RegisterPandoraSkillSource(AbilitySpec.SourceObject.Get());

	Super::OnGiveAbility(AbilitySpec);
	OnAbilitiesChangedNative.Broadcast();
}

// 능력이 회수될 때 남은 연출을 정리하고, 다른 능력도 사용하지 않는 판도라 출처 정보의 보관을 해제한다.
// 제거 이후에는 스킬바 등 구독자에게 목록 변경을 알린다.
void UPdAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	// GAS는 회수 중인 활성 능력을 정상 종료로 끝내므로, 종료 콜백이 회수를 식별할 수 있게 한다.
	AbilitySpec.PendingRemove = true;
	UPandoraSkillSource* RemovedSkillSource = Cast<UPandoraSkillSource>(AbilitySpec.SourceObject.Get());
	const FGameplayAbilitySpecHandle RemovedHandle = AbilitySpec.Handle;
	AbilityGrantAndInputManager->ClearAbilityInput(RemovedHandle);
	ActivationsWaitingForSource.RemoveAll([RemovedHandle](const FPendingAbilityInfo& Pending) { return Pending.Handle == RemovedHandle; });

	for (UGameplayAbility* AbilityInstance : AbilitySpec.GetAbilityInstances())
	{
		if (!IsValid(AbilityInstance))
		{
			continue;
		}

		if (UPdGameplayAbility* PdAbilityInstance = Cast<UPdGameplayAbility>(AbilityInstance))
		{
			PdAbilityInstance->DestroyActiveSkillPresentationActor();
		}
	}

	Super::OnRemoveAbility(AbilitySpec);

	ReleasePandoraSkillSourceIfUnused(RemovedSkillSource, RemovedHandle);

	OnAbilitiesChangedNative.Broadcast();
}

// 활성화가 확인되면 서버 응답 대기 기록을 제거한다.
void UPdAbilitySystemComponent::NotifyAbilityActivated(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability)
{
	AbilityGrantAndInputManager->NotifyAbilityActivated(Handle);
	Super::NotifyAbilityActivated(Handle, Ability);
}

// 종료·실패한 시전의 입력을 버려, 이후 시전이 이전 키 해제를 물려받지 않게 한다.
void UPdAbilitySystemComponent::NotifyAbilityEnded(
	FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	AbilityGrantAndInputManager->ClearAbilityInput(Handle);
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);
}

void UPdAbilitySystemComponent::NotifyAbilityFailed(
	const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason)
{
	AbilityGrantAndInputManager->ClearAbilityInput(Handle);
	Super::NotifyAbilityFailed(Handle, Ability, FailureReason);
}

void UPdAbilitySystemComponent::ClientActivateAbilityFailed_Implementation(
	FGameplayAbilitySpecHandle Handle, int16 PredictionKey)
{
	AbilityGrantAndInputManager->ClearAbilityInput(Handle);
	Super::ClientActivateAbilityFailed_Implementation(Handle, PredictionKey);
}

// 부여 목록뿐 아니라 입력 연결이나 출처가 뒤늦게 도착한 경우에도 스킬바를 갱신한다.
void UPdAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		RegisterPandoraSkillSource(Spec.SourceObject.Get());
	}
	ActivateAbilitiesWithReadySources();
	OnAbilitiesChangedNative.Broadcast();
}

// 서버에서 기본값과 시작 투자분을 검증해 최대 자원, 현재 자원 순서로 초기화한다.
// 초기화 시점과 중복 호출 방지는 호출하는 플레이어·적 컴포넌트가 담당한다.
bool UPdAbilitySystemComponent::ApplyConfiguredAttributeDefaults(const UStatUpgradeDefinition& Definition)
{
	if (!IsOwnerActorAuthoritative() || !GetSet<UBasicAttributeSet>())
	{
		return false;
	}

	TArray<TPair<FGameplayTag, float>> InitialValues;
	TArray<FPairedResourceStatTag> ResourcesToFill;
	if (!Definition.CalculateInitialAttributeValues(InitialValues, ResourcesToFill))
	{
		return false;
	}

	// 값을 변경하기 전에 모든 태그가 이 ASC의 어트리뷰트로 연결되는지 확인한다.
	TArray<TPair<FGameplayAttribute, FGameplayAttribute>> PairedResources;
	for (const FPairedResourceStatTag& Pair : ResourcesToFill)
	{
		FGameplayAttribute MaxAttribute;
		FGameplayAttribute CurrentAttribute;
		if (!UBasicAttributeSet::ResolveAttributeFromStatTag(Pair.MaxStatTag, MaxAttribute) || !UBasicAttributeSet::ResolveAttributeFromStatTag(Pair.CurrentStatTag, CurrentAttribute)
			|| !HasAttributeSetForAttribute(MaxAttribute) || !HasAttributeSetForAttribute(CurrentAttribute))
		{
			return false;
		}
		PairedResources.Emplace(MaxAttribute, CurrentAttribute);
	}

	TArray<TPair<FGameplayAttribute, float>> ResolvedDefaults;
	for (const TPair<FGameplayTag, float>& Entry : InitialValues)
	{
		FGameplayAttribute Attribute;
		if (!UBasicAttributeSet::ResolveAttributeFromStatTag(Entry.Key, Attribute) || !HasAttributeSetForAttribute(Attribute))
		{
			return false;
		}
		ResolvedDefaults.Emplace(Attribute, Entry.Value);
	}

	// 정의가 계산한 순서대로 최대값을 먼저 적용하고 현재 자원을 나중에 설정한다.
	for (const TPair<FGameplayAttribute, float>& Entry : ResolvedDefaults)
	{
		SetNumericAttributeBase(Entry.Key, Entry.Value);
	}
	bool bAllResourcesFilled = true;
	for (const TPair<FGameplayAttribute, FGameplayAttribute>& Pair : PairedResources)
	{
		// 장비·버프와 클램프까지 반영된 실제 최대값으로 채운다.
		const float MaxValue = GetNumericAttribute(Pair.Key);
		if (!FMath::IsFinite(MaxValue))
		{
			bAllResourcesFilled = false;
			break;
		}
		SetNumericAttributeBase(Pair.Value, MaxValue);
	}
	// 초기화 중 변경된 값은 한 번만 복제 갱신을 요청한다.
	if (AActor* OwningActor = GetOwner())
	{
		OwningActor->ForceNetUpdate();
	}
	return bAllResourcesFilled;
}

// 서버에서 값을 검증해 GAS 기본값을 변경한다. Dirty 표시는 엔진에 맡기고 복제 갱신을 요청한다.
bool UPdAbilitySystemComponent::ApplyAttributeDefaultValue(const FGameplayAttribute& Attribute, const float DefaultValue)
{
	if (!IsOwnerActorAuthoritative() || !Attribute.IsValid() || !FMath::IsFinite(DefaultValue)
		|| !HasAttributeSetForAttribute(Attribute))
	{
		return false;
	}

	SetNumericAttributeBase(Attribute, DefaultValue);

	if (AActor* OwningActor = GetOwner())
	{
		OwningActor->ForceNetUpdate();
	}

	return true;
}

// 로컬 입력이 유효하면 누름을 즉시 전달한다. 일시 정지와 월드 전환 중에는 새 시전을 시작하지 않는다.
void UPdAbilitySystemComponent::HandleAbilityInputPressed(const FGameplayTag& InputTag)
{
	if (!GetWorld() || GetWorld()->IsPaused() || !AbilityActorInfo.IsValid()
		|| !AbilityActorInfo->OwnerActor.IsValid() || !AbilityActorInfo->AvatarActor.IsValid()
		|| !AbilityActorInfo->IsLocallyControlled())
	{
		return;
	}
	AbilityGrantAndInputManager->HandleAbilityInputPressed(*this, InputTag);
}

// Press·그래플처럼 해제를 사용하는 능력만 누를 때의 대상에 해제를 예약한다.
void UPdAbilitySystemComponent::HandleAbilityInputReleased(const FGameplayTag& InputTag)
{
	AbilityGrantAndInputManager->HandleAbilityInputReleased(InputTag);
}

// 컨트롤러의 입력 처리 후, 아직 전달하지 못한 해제만 재확인한다.
void UPdAbilitySystemComponent::ProcessPendingInputReleases()
{
	// Seamless Travel 중에는 ActorInfo만 남고 이전 Owner/Avatar가 먼저 제거될 수 있다.
	if (!AbilityActorInfo.IsValid()
		|| !AbilityActorInfo->OwnerActor.IsValid()
		|| !AbilityActorInfo->AvatarActor.IsValid()
		|| !AbilityActorInfo->IsLocallyControlled())
	{
		return;
	}

	AbilityGrantAndInputManager->ProcessPendingInputReleases(*this);
}

// 장비 교체·스킬 사용 등의 상태 판단에 필요한, 지정 태그 중 하나와 일치하는 첫 번째 실행 중 능력을 찾는다.
const FGameplayAbilitySpec* UPdAbilitySystemComponent::FindActiveAbilitySpecByTags(const FGameplayTagContainer& AbilityTags) const
{
	return AbilityGrantAndInputManager->FindActiveAbilitySpecByTags(*this, AbilityTags);
}

// 동작 간 충돌을 판단할 때, 지정한 종류의 능력이 실행 중인지만 확인한다. 부여되어 있지만 쉬고 있는 능력은 제외한다.
bool UPdAbilitySystemComponent::HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const
{
	return FindActiveAbilitySpecByTags(AbilityTags) != nullptr;
}

// 특정 스킬 클래스의 동작이 진행 중인지 확인한다. 파생 스킬까지 같은 동작으로 볼지는 호출자가 선택한다.
bool UPdAbilitySystemComponent::HasActiveAbilityOfClass(const TSubclassOf<UGameplayAbility> AbilityClass, const bool bIncludeChildClasses) const
{
	return AbilityGrantAndInputManager->HasActiveAbilityOfClass(*this, AbilityClass, bIncludeChildClasses);
}

// 여러 스킬을 하나의 동작 범주로 묶어 실행 여부를 확인한다. 적 AI는 근접·원거리·주먹 공격 중 하나라도 진행 중인지 판단한다.
bool UPdAbilitySystemComponent::HasActiveAbilityOfAnyClass(
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
	const bool bIncludeChildClasses) const
{
	return AbilityGrantAndInputManager->HasActiveAbilityOfAnyClass(*this, AbilityClasses, bIncludeChildClasses);
}

// 여러 스탯의 변화량을 하나의 GameplayEffect에 담아 서버에서 자신에게 적용한다. 각 변화량과 연산 방식은 태그로 전달한다.
bool UPdAbilitySystemComponent::ApplyStatUpEffectByTags(
	TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const TMap<FGameplayTag, float>& StatMagnitudes,
	const EEnum_Operation Operation,
	const float Level)
{
	if (!IsOwnerActorAuthoritative())
	{
		return false;
	}

	const FGameplayTag OperationSetByCallerTag = UProjectTagConfig::GetDefaultConfig()->GetSetByCallerStatUpOperationTag();
	if (!GameplayEffectClass || StatMagnitudes.IsEmpty()
		|| !OperationSetByCallerTag.IsValid())
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = MakeEffectContext();
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(GameplayEffectClass, Level, EffectContext);
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

	const FActiveGameplayEffectHandle AppliedHandle = ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return AppliedHandle.WasSuccessfullyApplied();
}

// 서버에서 캐릭터가 사용할 능력을 중복 없이 부여하고, 나중에 회수할 핸들을 돌려준다.
// 자동 실행 대상으로 설정된 능력은 부여 후 실행도 시도한다.
TArray<FGameplayAbilitySpecHandle> UPdAbilitySystemComponent::GrantAbilities(
	const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses,
	const int32 AbilityLevel)
{
	return AbilityGrantAndInputManager->GrantAbilities(*this, AbilityClasses, AbilityLevel);
}

// 기능 해제나 구성 변경 시 서버에서 지정한 능력의 부여를 회수한다. 시전만 중단하는 리셋과 달리 능력 목록에서도 제거한다.
void UPdAbilitySystemComponent::RemoveAbilities(const TArray<FGameplayAbilitySpecHandle>& AbilityHandles)
{
	AbilityGrantAndInputManager->RemoveAbilities(*this, AbilityHandles);
}

// 부활 후에도 계속 작동해야 하는 자동 실행 능력을 다시 켜도록 시도한다. 이미 실행 중이거나 아직 사망 상태라면 건드리지 않는다.
void UPdAbilitySystemComponent::ReactivateAutoActivatedAbilities()
{
	AbilityGrantAndInputManager->ReactivateAutoActivatedAbilities(*this);
}

// 사망 능력은 유지하고 다른 시전을 취소한다. 종료 중 쿨다운 재생성을 막고, 이미 적용된 쿨다운도 제거한다.
void UPdAbilitySystemComponent::ResetAbilityRuntimeStateForDeath()
{
	AbilityGrantAndInputManager->ClearAllAbilityInputs();
	ActivationsWaitingForSource.Reset();
	CancelActiveAbilitiesForDeath();

	FGameplayTagContainer EffectTags(LabGameplayTags::Effect_Policy_RemoveOnDeath);
	EffectTags.AddTag(LabGameplayTags::Cooldown);
	const UGameSettingDefinition* Settings = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	RemoveRuntimeEffects(EffectTags, FGameplayTagContainer(LabGameplayTags::Cooldown), FGameplayTagContainer(),
		Settings ? Settings->DeathGameplayCuesToRemove : FGameplayTagContainer());
	OnAbilitiesChangedNative.Broadcast();
}


// 사망 능력은 유지하고, 나머지 활성 시전을 정리한 뒤 취소한다.
void UPdAbilitySystemComponent::CancelActiveAbilitiesForDeath()
{
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
					PdAbilityInstance->DestroyActiveSkillPresentationActor();
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

	for (const FGameplayAbilitySpecHandle& AbilityHandle : ActiveAbilityHandles)
	{
		CancelAbilityHandle(AbilityHandle);
	}
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
		RegisterPandoraSkillSource(Spec.SourceObject.Get());
	}
}

// 출처가 능력 목록보다 늦게 도착해도 이전 판도라의 데이터로 실행하지 않고, 정확한 원본이 준비되면 이어서 실행한다.
void UPdAbilitySystemComponent::NotifyPandoraSourceReplicated(UPandoraSkillSource* Source)
{
	RegisterPandoraSkillSource(Source);
	ActivateAbilitiesWithReadySources();
	OnAbilitiesChangedNative.Broadcast();
}

// 서버의 실행 통지가 출처 복제보다 먼저 도착하면 실행 정보만 잠시 보관한다.
void UPdAbilitySystemComponent::ClientActivateAbilitySucceedWithEventData_Implementation(
	FGameplayAbilitySpecHandle Handle, FPredictionKey PredictionKey, FGameplayEventData TriggerEventData)
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora))
	{
		const UPandoraSkillSource* Source = Cast<UPandoraSkillSource>(Spec->SourceObject.Get());
		if (!Source || !Source->IsSourceReady())
		{
			FPendingAbilityInfo Pending;
			Pending.Handle = Handle;
			Pending.PredictionKey = PredictionKey;
			Pending.TriggerEventData = TriggerEventData;
			Pending.bPartiallyActivated = true;
			ActivationsWaitingForSource.AddUnique(Pending);
			return;
		}
	}
	Super::ClientActivateAbilitySucceedWithEventData_Implementation(Handle, PredictionKey, TriggerEventData);
}

// 원본이 준비된 실행 통지만 재개한다. 아직 준비되지 않은 다른 스킬은 대기 상태를 유지한다.
void UPdAbilitySystemComponent::ActivateAbilitiesWithReadySources()
{
	TArray<FPendingAbilityInfo> ReadyActivations;
	for (int32 Index = 0; Index < ActivationsWaitingForSource.Num();)
	{
		const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(ActivationsWaitingForSource[Index].Handle);
		const UPandoraSkillSource* Source = Spec ? Cast<UPandoraSkillSource>(Spec->SourceObject.Get()) : nullptr;
		if (Spec && Spec->Ability && Source && Source->IsSourceReady())
		{
			ReadyActivations.Add(ActivationsWaitingForSource[Index]);
			ActivationsWaitingForSource.RemoveAt(Index);
		}
		else
		{
			++Index;
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
	ClearPendingActivation(Handle, ActivationInfo.GetActivationPredictionKey());
	Super::ClientEndAbility_Implementation(Handle, ActivationInfo);
}

// 사망·피격 등 서버 취소가 도착하면 아직 실행하지 못한 시전도 취소한다.
void UPdAbilitySystemComponent::ClientCancelAbility_Implementation(
	FGameplayAbilitySpecHandle Handle, FGameplayAbilityActivationInfo ActivationInfo)
{
	ClearPendingActivation(Handle, ActivationInfo.GetActivationPredictionKey());
	Super::ClientCancelAbility_Implementation(Handle, ActivationInfo);
}

void UPdAbilitySystemComponent::ClearPendingActivation(
	FGameplayAbilitySpecHandle Handle, const FPredictionKey& PredictionKey)
{
	const int32 RemovedCount = ActivationsWaitingForSource.RemoveAll([&](const FPendingAbilityInfo& Pending)
	{
		return Pending.Handle == Handle && Pending.PredictionKey == PredictionKey;
	});
	if (RemovedCount > 0)
	{
		AbilityGrantAndInputManager->ClearAbilityInput(Handle);
	}
}

void UPdAbilitySystemComponent::RegisterPandoraSkillSource(UObject* SourceObject)
{
	if (UPandoraSkillSource* SkillSource =
		Cast<UPandoraSkillSource>(SourceObject))
	{
		GrantedPandoraSkillSources.AddUnique(SkillSource);
		if (IsReadyForReplication() && IsOwnerActorAuthoritative())
		{
			AddReplicatedSubObject(SkillSource);
		}
	}
}

void UPdAbilitySystemComponent::ReleasePandoraSkillSourceIfUnused(
	UPandoraSkillSource* SkillSource,
	const FGameplayAbilitySpecHandle RemovedHandle)
{
	if (!SkillSource)
	{
		return;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.Handle != RemovedHandle
			&& AbilitySpec.SourceObject.Get() == SkillSource)
		{
			return;
		}
	}

	if (IsOwnerActorAuthoritative())
	{
		DestroyReplicatedSubObjectOnRemotePeers(SkillSource);
	}
	GrantedPandoraSkillSources.Remove(SkillSource);
}

void UPdAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags,
	UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags,
	bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags)
{
	Super::ApplyAbilityBlockAndCancelTags(AbilityTags, RequestingAbility, bEnableBlockTags, BlockTags, false, CancelTags);
	if (!bExecuteCancelTags || CancelTags.IsEmpty()) return;
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		// 공통 SkillAbility의 스킬별 식별 태그는 클래스 기본값 대신 Spec에 저장된다.
		if (Spec.IsActive() && Spec.Ability
			&& (Spec.Ability->GetAssetTags().HasAny(CancelTags) || Spec.GetDynamicSpecSourceTags().HasAny(CancelTags)))
		{
			CancelAbilitySpec(Spec, RequestingAbility);
		}
	}
}
