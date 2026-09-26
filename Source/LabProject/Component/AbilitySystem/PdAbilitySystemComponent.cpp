#include "Component/AbilitySystem/PdAbilitySystemComponent.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/AbilityGrantAndInputManager.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Definition/Player/StatUpgradeDefinition.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAbilitySystemComponent)

// 생성자
UPdAbilitySystemComponent::UPdAbilitySystemComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 서버/클라이언트 동기화가 자동
	SetIsReplicatedByDefault(true);
	// 하위에 붙은 서브 오브젝트들도 함께 복제 (AddReplicatedSubObject로 서브 오브젝트 등록)
	bReplicateUsingRegisteredSubObjectList = true;
	// GameplayEffect 복제 모드를 "Mixed"로 설정 : 일부 효과는 전체 복제, 일부는 조건부 복제를 하는 방식으로 성능과 정확성을 절충
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AbilityGrantAndInputManager = CreateDefaultSubobject<UAbilityGrantAndInputManager>(TEXT("AbilityGrantAndInputManager"));
}

// 능력이 회수될 때 입력과 연출을 정리하고, 자원 소유자와 UI에 회수를 알린다.
void UPdAbilitySystemComponent::OnRemoveAbility(
	FGameplayAbilitySpec& AbilitySpec)
{
    AbilityGrantAndInputManager->ClearAbilityInput(*this, AbilitySpec.Handle);

    Super::OnRemoveAbility(AbilitySpec);

    OnAbilityRemovedNative.Broadcast(AbilitySpec);
    OnAbilitiesChangedNative.Broadcast();
}

void UPdAbilitySystemComponent::NotifyAbilityEnded(
	FGameplayAbilitySpecHandle Handle,
	UGameplayAbility* Ability,
	bool bWasCancelled)
{
	AbilityGrantAndInputManager->ClearAbilityInput(*this, Handle);

	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);
}

void UPdAbilitySystemComponent::NotifyAbilityFailed(
	const FGameplayAbilitySpecHandle Handle,
	UGameplayAbility* Ability,
	const FGameplayTagContainer& FailureReason)
{
	AbilityGrantAndInputManager->ClearAbilityInput(*this, Handle);

	Super::NotifyAbilityFailed(Handle, Ability, FailureReason);
}

void UPdAbilitySystemComponent::ClientActivateAbilityFailed_Implementation(
	FGameplayAbilitySpecHandle Handle,
	int16 PredictionKey)
{
	// 늦게 도착한 이전 시전의 실패 응답이 현재 시전의 입력을 지우지 않도록 PredictionKey를 확인한다.
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	const UGameplayAbility* Instance = Spec ? Spec->GetPrimaryInstance() : nullptr;

	if (Instance &&
		Instance->GetCurrentActivationInfo()
			.GetActivationPredictionKey().Current == PredictionKey)
	{
		AbilityGrantAndInputManager->ClearAbilityInput(*this, Handle);
	}

	Super::ClientActivateAbilityFailed_Implementation(Handle, PredictionKey);
}

// 부여 목록뿐 아니라 입력 연결이나 출처가 뒤늦게 도착한 경우에도 스킬바를 갱신한다.
void UPdAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	OnAbilitiesChangedNative.Broadcast();
}

// 서버에서 기본값과 시작 투자분을 검증해 최대 자원, 현재 자원 순서로 초기화한다.
// 초기화 시점과 중복 호출 방지는 호출하는 플레이어·적 컴포넌트가 담당한다.
bool UPdAbilitySystemComponent::ApplyConfiguredAttributeDefaults(
	const UStatUpgradeDefinition& Definition)
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

	// 기본 능력치 적용
	for (const TPair<FGameplayTag, float>& Entry : InitialValues)
	{
		FGameplayAttribute Attribute;
		if (!UBasicAttributeSet::ResolveAttributeFromStatTag(Entry.Key, Attribute))
		{
			return false;
		}

		SetNumericAttributeBase(Attribute, Entry.Value);
	}

	// HP/MP 같은 현재 자원을 최종 최대값까지 채운다.
	for (const FPairedResourceStatTag& Pair : ResourcesToFill)
	{
		FGameplayAttribute MaxAttribute;
		FGameplayAttribute CurrentAttribute;

		if (!UBasicAttributeSet::ResolveAttributeFromStatTag(Pair.MaxStatTag, MaxAttribute)
			|| !UBasicAttributeSet::ResolveAttributeFromStatTag(Pair.CurrentStatTag, CurrentAttribute))
		{
			return false;
		}

		SetNumericAttributeBase(
			CurrentAttribute,
			GetNumericAttribute(MaxAttribute));
	}

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}

	return true;
}

// 서버에서 값을 검증해 GAS 기본값을 변경한다. Dirty 표시는 엔진에 맡기고 복제 갱신을 요청한다.
bool UPdAbilitySystemComponent::ApplyAttributeDefaultValue(
	const FGameplayAttribute& Attribute,
	const float DefaultValue)
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
void UPdAbilitySystemComponent::HandleAbilityInputPressed(
	const FGameplayTag& InputTag)
{
	if (!GetWorld() || GetWorld()->IsPaused() || !AbilityActorInfo.IsValid()
		|| !AbilityActorInfo->OwnerActor.IsValid() || !AbilityActorInfo->AvatarActor.IsValid()
		|| !AbilityActorInfo->IsLocallyControlled())
	{
		return;
	}
	AbilityGrantAndInputManager->HandleAbilityInputPressed(*this, InputTag);
}

// 해제는 일시 정지·입력 차단과 무관하게 누름 기록을 소비하고, 유효한 로컬 시전에 즉시 전달한다.
void UPdAbilitySystemComponent::HandleAbilityInputReleased(
	const FGameplayTag& InputTag)
{
	AbilityGrantAndInputManager->HandleAbilityInputReleased(*this, InputTag);
}

// 장비 교체·스킬 사용 등의 상태 판단에 필요한, 지정 태그 중 하나와 일치하는 첫 번째 실행 중 능력을 찾는다.
const FGameplayAbilitySpec* UPdAbilitySystemComponent::FindActiveAbilitySpecByTags(
	const FGameplayTagContainer& AbilityTags) const
{
	return AbilityGrantAndInputManager->FindActiveAbilitySpecByTags(*this, AbilityTags);
}

// 동작 간 충돌을 판단할 때, 지정한 종류의 능력이 실행 중인지만 확인한다. 부여되어 있지만 쉬고 있는 능력은 제외한다.
bool UPdAbilitySystemComponent::HasActiveAbilityWithTags(
	const FGameplayTagContainer& AbilityTags) const
{
	return FindActiveAbilitySpecByTags(AbilityTags) != nullptr;
}

// 특정 스킬 클래스의 동작이 진행 중인지 확인한다. 파생 스킬까지 같은 동작으로 볼지는 호출자가 선택한다.
bool UPdAbilitySystemComponent::HasActiveAbilityOfClass(
	const TSubclassOf<UGameplayAbility> AbilityClass,
	const bool bIncludeChildClasses) const
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
	if (!IsOwnerActorAuthoritative() || !GameplayEffectClass || StatMagnitudes.IsEmpty())
	{
		return false;
	}

	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingSpec(GameplayEffectClass, Level, MakeEffectContext());

	if (!SpecHandle.IsValid())
	{
		return false;
	}

	for (const TPair<FGameplayTag, float>& Pair : StatMagnitudes)
	{
		SpecHandle.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
	}

	SpecHandle.Data->SetSetByCallerMagnitude(
		UProjectTagDefinition::GetDefaultConfig()->GetSetByCallerStatUpOperationTag(),
		static_cast<float>(Operation));

	return ApplyGameplayEffectSpecToSelf(*SpecHandle.Data).WasSuccessfullyApplied();
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
void UPdAbilitySystemComponent::RemoveAbilities(
	const TArray<FGameplayAbilitySpecHandle>& AbilityHandles)
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
			AbilitySpec.InputPressed = false;

			if (!AbilitySpec.Ability
				|| !AbilitySpec.IsActive()
				|| AbilitySpec.Ability->GetAssetTags().HasTagExact(
					LabGameplayTags::GameplayAbility_Death))
			{
				continue;
			}

			for (UGameplayAbility* AbilityInstance : AbilitySpec.GetAbilityInstances())
			{
				if (IsValid(AbilityInstance) && AbilityInstance->IsActive())
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
	const FGameplayTagContainer& EffectTags,
	const FGameplayTagContainer& OwnedTags,
	const FGameplayTagContainer& LooseTags,
	const FGameplayTagContainer& GameplayCues)
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

void UPdAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(
	const FGameplayTagContainer& AbilityTags,
	UGameplayAbility* RequestingAbility,
	bool bEnableBlockTags,
	const FGameplayTagContainer& BlockTags,
	bool bExecuteCancelTags,
	const FGameplayTagContainer& CancelTags)
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

void UPdAbilitySystemComponent::ResetRuntimeStateForRespawn()
{
	if (!IsRegistered() || !GetAttributeSet(UBasicAttributeSet::StaticClass()))
	{
		return;
	}

	// 기존 리스폰과 동일하게 효과 제거 전 최대 자원값을 복구 기준으로 사용한다.
	const float MaxHealth = GetNumericAttribute(UBasicAttributeSet::GetMaxHealthAttribute());
	const float MaxStamina = GetNumericAttribute(UBasicAttributeSet::GetMaxStaminaAttribute());
	const float MaxMana = GetNumericAttribute(UBasicAttributeSet::GetMaxManaAttribute());

	ClearStatusEffectsForRespawn();

	FGameplayTagContainer DeadTags;
	DeadTags.AddTag(LabGameplayTags::State_Dead);
	RemoveActiveEffectsWithGrantedTags(DeadTags);
	RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(DeadTags));

	SetNumericAttributeBase(UBasicAttributeSet::GetHealthAttribute(), FMath::Max(MaxHealth, 1.0f));
	SetNumericAttributeBase(UBasicAttributeSet::GetShieldAttribute(), 0.0f);
	SetNumericAttributeBase(UBasicAttributeSet::GetStaminaAttribute(), FMath::Max(MaxStamina, 0.0f));
	SetNumericAttributeBase(UBasicAttributeSet::GetManaAttribute(), FMath::Max(MaxMana, 0.0f));
	ForceReplication();
}

void UPdAbilitySystemComponent::RestoreResourcesToMaximum()
{
	if (!GetAttributeSet(UBasicAttributeSet::StaticClass()))
	{
		return;
	}

	SetNumericAttributeBase(UBasicAttributeSet::GetHealthAttribute(), GetNumericAttribute(UBasicAttributeSet::GetMaxHealthAttribute()));
	SetNumericAttributeBase(UBasicAttributeSet::GetManaAttribute(), GetNumericAttribute(UBasicAttributeSet::GetMaxManaAttribute()));
	SetNumericAttributeBase(UBasicAttributeSet::GetStaminaAttribute(), GetNumericAttribute(UBasicAttributeSet::GetMaxStaminaAttribute()));
	ForceReplication();
}
