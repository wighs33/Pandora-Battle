#include "AbilitySystem/Ability/PdGameplayAbility.h"

#include "Abilities/GameplayAbilityTargetActor.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystem/Interfaces/TargetingInterface.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/Ability/AbilityMovementManager.h"
#include "Component/AbilitySystem/Ability/AbilityPresentationManager.h"
#include "Component/AbilitySystem/Ability/AbilityCostAndCooldownManager.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Pandora/PandoraSkillSource.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Weapon/WeaponBase.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Mode/PdPlayerState.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdGameplayAbility)

// 생성 및 GAS 시작·종료

// 캐릭터별 능력 인스턴스와 서버 시작 실행을 기본 정책으로 설정하고, 사망 중 시전을 차단한다.
// 각 능력이 사용할 비용·쿨다운, 이동, 연출 관리 객체와 사망 시 쿨다운 제거 정책을 준비한다.
UPdGameplayAbility::UPdGameplayAbility(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	ActivationOwnedTags.AddTag(LabGameplayTags::GameplayAbility_Active);
	ActivationBlockedTags.AddTag(LabGameplayTags::State_Dead);
	CooldownRemovalPolicyTags.AddTag(LabGameplayTags::Effect_Policy_RemoveOnDeath);

	CostAndCooldownManager = ObjectInitializer.CreateDefaultSubobject<UAbilityCostAndCooldownManager>(this, TEXT("CostAndCooldownManager"));
	MovementManager = ObjectInitializer.CreateDefaultSubobject<UAbilityMovementManager>(this, TEXT("MovementManager"));
	PresentationManager = ObjectInitializer.CreateDefaultSubobject<UAbilityPresentationManager>(this, TEXT("PresentationManager"));
	// 이 세 객체는 모든 능력이 소유하는 필수 구성이다.
	check(CostAndCooldownManager && MovementManager && PresentationManager);
}

// 새 시전 전에 판도라 출처가 준비되었고 해당 판도라가 선택 상태인지 검사한다.
// 이 조건을 통과하면 비용·쿨다운 등 GAS의 기본 시전 조건도 확인한다.
bool UPdGameplayAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ?
		ActorInfo->AbilitySystemComponent.Get() : nullptr;

	const FGameplayAbilitySpec* Spec = ASC ?
		ASC->FindAbilitySpecFromHandle(Handle) : nullptr;

	if (Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora))
	{
		const UPandoraSkillSource* Source = Cast<UPandoraSkillSource>(Spec->SourceObject.Get());
		if (!Source || !Source->IsSourceReady() ||
			!Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Pandora_Selected))
		{
			return false;
		}
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

// 시전 시작 직전 서버에서 출처의 판도라 슬롯 방향을 갱신하고, 이전 시전의 연출 액터를 정리한다.
// 입력형 스킬이 시작될 때 진행 중인 무기 장착·해제 능력을 취소해 스킬 동작과 겹치지 않게 한다.
void UPdGameplayAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	// 새 시전 직전에만 방향을 갱신한다. 교체 도중 실행 중인 시전의 출처는 변경하지 않는다.
	if (ActorInfo && ActorInfo->IsNetAuthority())
	{
		const APdPlayerState* PlayerState = Cast<APdPlayerState>(ActorInfo->OwnerActor.Get());
		const UPandoraComponent* Pandora = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
		const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
		UPandoraSkillSource* Source = Spec ? Cast<UPandoraSkillSource>(Spec->SourceObject.Get()) : nullptr;

		if (Source && Pandora && Source->GetPandoraDefinition() == Pandora->GetCurrentPandoraDefinition())
		{
			Source->Initialize(
				Source->GetPandoraDefinition(), Source->GetSkillDataAsset(), Source->GetSkillIndex(),
				Source->GetPandoraLevel(), Pandora->GetCurrentPandoraLoadoutDirection());
		}
	}
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
	DestroyActiveSkillPresentationActor();

	UPdAbilitySystemComponent* AbilitySystemComponent = ActorInfo ?
		Cast<UPdAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent ?
		AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) : nullptr;

	const USkillDefinition* SkillDefinition =
		ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);

	const bool bInputDrivenSkill = SkillDefinition
		&& (SkillDefinition->SkillType == ESkillType::Instant || SkillDefinition->SkillType == ESkillType::Press
			|| SkillDefinition->SkillType == ESkillType::Duration);

	if (!AbilitySystemComponent || !bInputDrivenSkill)
	{
		return;
	}

	FGameplayTagContainer EquipmentTransitionTags;
	EquipmentTransitionTags.AddTag(LabGameplayTags::Action_Equip);
	EquipmentTransitionTags.AddTag(LabGameplayTags::Action_Unequip);
	if (AbilitySystemComponent->HasActiveAbilityWithTags(EquipmentTransitionTags))
	{
		AbilitySystemComponent->CancelAbilities(&EquipmentTransitionTags, nullptr, this);
	}
}

// GAS의 시전 확정과 비용 처리가 성공하면 설정에 따라 이동을 멈추고 자기 버프를 적용한다.
// 피격 취소를 허용하지 않는 스킬은 이때 일반 취소를 막고, 사망·리셋 취소는 ASC가 별도로 처리한다.
bool UPdGameplayAbility::CommitAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, FGameplayTagContainer* OptionalRelevantTags)
{
	if (!Super::CommitAbility(Handle, ActorInfo, ActivationInfo, OptionalRelevantTags))
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ?
		ActorInfo->AbilitySystemComponent.Get() : nullptr;

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent && Handle.IsValid() ?
		AbilitySystemComponent->FindAbilitySpecFromHandle(Handle) : GetCurrentAbilitySpec();

	const USkillDefinition* SkillDefinition = ResolveSourceSkillDataAsset(AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr);
	if (SkillDefinition)
	{
		MovementManager->StopAvatarMovementForSkillActivation(*this);

		// 비용을 지불한 보호 스킬은 일반 취소로 끊지 않는다. 사망·리셋은 별도로 취소 가능 상태를 복구한다.
		if (!SkillDefinition->bCancelOnHit)
		{
			SetCanBeCanceled(false);
		}
	}

	StartConfiguredSelfBuff(Handle, ActorInfo, ActivationInfo);
	return true;
}

// 정상 종료·취소 시 파생 스킬 정리, 연출·자기 버프·접촉 피해·이동 잠금 해제를 순서대로 수행한다.
// 정상 종료에 예약된 쿨다운을 적용한 뒤 GAS 종료를 알리고, 필요한 장비 전환과 회전 정책을 복구한다.
// 중복 종료와 잠금 중 종료를 제어해 같은 시전의 정리가 겹쳐 실행되지 않게 한다.
void UPdGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const bool bReplicateEndAbility, const bool bWasCancelled)
{
	if (bIsCleaningUpAbility || !IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(
			this, &ThisClass::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}

	// 파생 능력의 정리 중 들어오는 종료 알림도 같은 종료를 다시 실행하지 못하게 한다.
	bIsCleaningUpAbility = true;
	OnAbilityEnding();

	const bool bEquipmentTransitionAbility =
		GetAssetTags().HasTagExact(LabGameplayTags::Action_Equip) || GetAssetTags().HasTagExact(LabGameplayTags::Action_Unequip);

	DestroyActiveSkillPresentationActor();
	StopConfiguredSelfBuff();
	MovementManager->StopMovementContactDamage(*this);
	MovementManager->StopDurationMovementLock(*this);
	RestoreAvatarMovementForAbility();

	const UPdAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? Cast<UPdAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	const bool bCooldownWasPending = CostAndCooldownManager->ConsumePendingCooldown(bWasCancelled);
	const bool bShouldApplySkillCooldown =
		bCooldownWasPending && !(AbilitySystemComponent && AbilitySystemComponent->IsResettingAbilityRuntimeState());
	if (bShouldApplySkillCooldown)
	{
		ApplyCooldownImmediately(Handle, ActorInfo, ActivationInfo);
	}

	// 이제부터는 GAS의 bIsAbilityEnding이 재진입을 막는다.
	bIsCleaningUpAbility = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 종료 알림에서 같은 인스턴스가 다시 활성화되었다면 이전 시전의 후처리를 실행하지 않는다.
	if (IsActive())
	{
		return;
	}
	OnAbilityEnded(bWasCancelled);

	if (bEquipmentTransitionAbility)
	{
		return;
	}

	ACharacterBase* Character = ActorInfo ? Cast<ACharacterBase>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character)
	{
		return;
	}

	if (UEquipmentComponent* Equipment = Character->GetEquipmentComponent())
	{
		if (!Equipment->TryResumePendingWeaponSelection())
		{
			// 장착 몽타주가 중단되어도 현재 무기의 애니메이션 레이어로 복구한다.
			Equipment->RefreshCurrentWeaponAnimationLayer();
		}
	}
	Character->ReapplyCurrentRotationPolicy();
}

// GAS가 능력을 비활성화하기 전에 파생 스킬이 전용 타이머·타기팅·공격 상태를 정리하는 확장 지점이다.
// 공통 정리는 EndAbility가 담당하므로 기본 구현은 비워 둔다.
void UPdGameplayAbility::OnAbilityEnding() {}

// GAS 종료 후 파생 스킬이 대기 입력이나 다음 행동을 이어 가는 확장 지점이다.
// 이미 같은 인스턴스가 다시 활성화된 경우에는 EndAbility에서 이 호출을 생략한다.
void UPdGameplayAbility::OnAbilityEnded(bool bWasCancelled) {}

// 오라처럼 지속시간이 끝난 스킬을 취소가 아닌 정상 종료로 마무리해 예약된 종료 쿨다운이 적용되게 한다.
// 서버에서 끝낼 때는 종료 사실을 클라이언트에도 전달한다.
void UPdGameplayAbility::FinishAbilityFromDuration()
{
	if (!IsEndAbilityValid(CurrentSpecHandle, CurrentActorInfo))
	{
		return;
	}

	const bool bReplicateEndAbility = CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, false);
}

// 지연된 타이머·이벤트가 실제 스킬 효과를 실행하기 전에 시전자 생존 여부와 ASC 리셋 상태를 확인한다.
// 사망 태그 반영 전이라도 체력이 0 이하이면 실행을 막는다.
bool UPdGameplayAbility::CanExecuteSkillPayload() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return false;
	}
	if (const ACharacterBase* Character = Cast<ACharacterBase>(AvatarActor); Character && Character->IsDead())
	{
		return false;
	}

	const UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	const UBasicAttributeSet* BasicAttributeSet = AbilitySystemComponent ? AbilitySystemComponent->GetSet<UBasicAttributeSet>() : nullptr;
	if (BasicAttributeSet && BasicAttributeSet->GetHealth() <= 0.0f)
	{
		return false;
	}

	return !AbilitySystemComponent || !AbilitySystemComponent->IsResettingAbilityRuntimeState();
}

// 타깃 확보나 실제 스킬 실행에 실패하면 취소 금지 상태를 해제하고 능력을 취소한다.
// 정상 완료가 아니므로 예약된 종료 쿨다운이 부과되지 않게 한다.
void UPdGameplayAbility::CancelAbilityForSkillExecutionFailure()
{
	// 실제 스킬 실행 실패는 보호 상태라도 취소로 종료해, 예약된 종료 쿨다운을 부과하지 않는다.
	if (!CanBeCanceled())
	{
		SetCanBeCanceled(true);
	}

	K2_CancelAbility();
}

// 비용과 쿨다운

// GAS가 쿨다운 상태를 식별할 태그를 제공한다. 스킬 정의가 있으면 그 설정을, 없으면 부모 설정을 사용한다.
const FGameplayTagContainer* UPdGameplayAbility::GetCooldownTags() const
{
	return CostAndCooldownManager->BuildCooldownTags(*this, Super::GetCooldownTags());
}

// 시전 전에 기본 GAS 비용과 프로젝트의 마나·스태미나 요구량을 검사하고, 부족하면 실패 태그를 남긴다.
// 자원을 차감하는 단계는 ApplyCost다.
bool UPdGameplayAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)
		&& CostAndCooldownManager->CheckCost(*this, Handle, ActorInfo, OptionalRelevantTags);
}

// 시전 확정 시 기본 GAS 비용과 스킬·행동에 설정된 마나·스태미나 비용을 GameplayEffect로 적용한다.
// 프로젝트 비용의 권한·예측 처리와 현재 자원 범위 내 차감은 비용 관리 객체가 담당한다.
void UPdGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	CostAndCooldownManager->ApplyCost(*this, Handle, ActorInfo, ActivationInfo);
}

// 다시 시전할 수 있는지 확인하며, 판도라 스킬은 부여된 출처별 쿨다운을 조회해 다른 판도라와 구분한다.
// 스킬 정의가 없는 능력은 GAS의 기본 쿨다운 검사를 사용한다.
bool UPdGameplayAbility::CheckCooldown(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	bool bHandled = false;
	const bool bAvailable =
		CostAndCooldownManager->CheckConfiguredCooldown(*this, Handle, ActorInfo, Super::GetCooldownTags(), OptionalRelevantTags, bHandled);
	return bHandled ? bAvailable : Super::CheckCooldown(Handle, ActorInfo, OptionalRelevantTags);
}

// 시전 확정 때 호출되며, 양수 쿨다운이 설정된 스킬은 정상 종료 시 적용하도록 예약한다.
// 종료까지 미룰 대상이 아니면 즉시 쿨다운 적용 경로로 넘긴다.
void UPdGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (CostAndCooldownManager->ShouldDeferCooldown(*this, Handle, ActorInfo))
	{
		CostAndCooldownManager->MarkCooldownForAbilityEnd();
		return;
	}

	ApplyCooldownImmediately(Handle, ActorInfo, ActivationInfo);
}

// 스킬 정의의 쿨다운에 신비(Arcane) 감소율을 반영해 적용하고, 스킬 정의가 없으면 부모 GAS 설정을 적용한다.
void UPdGameplayAbility::ApplyCooldownImmediately(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	const bool bHandled = CostAndCooldownManager->ApplyConfiguredCooldownImmediately(*this, Handle, ActorInfo, ActivationInfo);
	if (!bHandled)
	{
		Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}
}

// GAS나 호출자가 현재 능력의 재사용 대기 시간을 한 값으로 요구할 때 남은 초를 반환한다.
float UPdGameplayAbility::GetCooldownTimeRemaining(const FGameplayAbilityActorInfo* ActorInfo) const
{
	float Remaining = 0.0f;
	float Duration = 0.0f;
	GetCooldownTimeRemainingAndDuration(GetCurrentAbilitySpecHandle(), ActorInfo, Remaining, Duration);
	return Remaining;
}

// 스킬바의 쿨다운 숫자와 진행률에 필요한 남은 시간·전체 시간을 실제 GameplayEffect에서 조회한다.
// 판도라 출처가 아직 복제되지 않았으면 다른 출처의 쿨다운을 대신 표시하지 않고 두 값을 0으로 반환한다.
void UPdGameplayAbility::GetCooldownTimeRemainingAndDuration(
	FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& TimeRemaining, float& CooldownDuration) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
	const UPandoraSkillSource* Source = Spec ? Cast<UPandoraSkillSource>(Spec->SourceObject.Get()) : nullptr;
	if (Source)
	{
		UAbilityCostAndCooldownManager::GetPandoraCooldown(*ASC, *Source, TimeRemaining, CooldownDuration);
		return;
	}
	if (Spec && Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora))
	{
		TimeRemaining = 0.0f;
		CooldownDuration = 0.0f;
		return;
	}
	Super::GetCooldownTimeRemainingAndDuration(Handle, ActorInfo, TimeRemaining, CooldownDuration);
}

// 생성한 쿨다운 효과에 사망 시 제거 같은 정책 태그를 붙여, ASC의 상태 정리가 해당 효과를 찾을 수 있게 한다.
void UPdGameplayAbility::AppendCooldownRemovalPolicyTags(FGameplayEffectSpecHandle& CooldownSpecHandle) const
{
	if (CooldownSpecHandle.IsValid() && CooldownSpecHandle.Data.IsValid())
	{
		CooldownSpecHandle.Data->AppendDynamicAssetTags(CooldownRemovalPolicyTags);
	}
}

// 사망·능력 회수로 강제 정리할 때 종료 쿨다운 예약을 취소해, 정리 도중 쿨다운이 새로 생기지 않게 한다.
// 이미 적용된 쿨다운 효과를 제거하는 함수는 아니다.
void UPdGameplayAbility::DisableCooldownOnAbilityEnd() const
{
	CostAndCooldownManager->SuppressPendingCooldown();
}

// 판도라 스킬·무기 장착·그래플 등이 지정한 시간과 태그로 프로젝트 공통 쿨다운 효과를 자신에게 적용한다.
// 효과에는 시전 출처와 제거 정책도 함께 담긴다.
bool UPdGameplayAbility::ApplySharedCooldownEffect(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const float CooldownDuration, const FGameplayTagContainer& CooldownTags) const
{
	if (CooldownDuration <= 0.0f || CooldownTags.IsEmpty())
	{
		return true;
	}

	const UGameSettingDefinition* SettingDefinition = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const TSubclassOf<UGameplayEffect> CooldownEffectClass =
		SettingDefinition ? SettingDefinition->AbilityCooldownGameplayEffectClass : nullptr;
	if (!CooldownEffectClass)
	{
		return false;
	}

	FGameplayEffectSpecHandle CooldownSpec =
		MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, CooldownEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!CooldownSpec.IsValid() || !CooldownSpec.Data.IsValid())
	{
		return false;
	}

	CooldownSpec.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_Cooldown, CooldownDuration);
	CooldownSpec.Data->DynamicGrantedTags.AppendTags(CooldownTags);
	CooldownSpec.Data->AppendDynamicAssetTags(CooldownTags);

	AppendCooldownRemovalPolicyTags(CooldownSpec);
	return ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CooldownSpec).WasSuccessfullyApplied();
}

// 기본 공격의 다음 콤보 동작을 이어 가기 전에 추가 스태미나 비용을 확인하고, 서버에서 실제로 차감한다.
// 플레이어가 조종하지 않는 캐릭터는 이 추가 비용 검사를 통과한다.
bool UPdGameplayAbility::TryCommitAdditionalActionStaminaCost() const
{
	return CostAndCooldownManager->TryCommitAdditionalActionStaminaCost(*this);
}

// 능력 출처와 실행 대상

// 능력을 실제로 수행하는 Avatar 캐릭터를 가져와 이동·무기·외형 같은 캐릭터 기능에 접근하게 한다.
ACharacterBase* UPdGameplayAbility::GetPdCharacterFromActorInfo() const
{
	return Cast<ACharacterBase>(GetAvatarActorFromActorInfo());
}

// 현재 시전이 연결된 프로젝트 ASC를 가져와 스탯·능력 조회와 리셋 상태 확인 등에 사용한다.
UPdAbilitySystemComponent* UPdGameplayAbility::GetPdAbilitySystemComponentFromActorInfo() const
{
	return Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

// 부여된 SourceObject가 스킬 정의 자체인지 판도라 스킬 출처인지 구분해 원래 스킬 설정을 찾는다.
// 현재 선택한 판도라로 추정하지 않아, 판도라 교체 뒤에도 기존 능력의 설정이 바뀌지 않게 한다.
const USkillDefinition* UPdGameplayAbility::ResolveSourceSkillDataAsset(UObject* SourceObject)
{
	if (USkillDefinition* SkillDefinition = Cast<USkillDefinition>(SourceObject))
	{
		return SkillDefinition;
	}
	const UPandoraSkillSource* Source = Cast<UPandoraSkillSource>(SourceObject);
	return Source ? Source->GetSkillDataAsset() : nullptr;
}

// 현재 시전의 출처에서 스킬 정의를 가져와 피해량·시간·비용·이동·연출 설정을 읽게 한다.
USkillDefinition* UPdGameplayAbility::GetSourceSkillDataAsset() const
{
	return const_cast<USkillDefinition*>(ResolveSourceSkillDataAsset(GetCurrentSourceObject()));
}

// 현재 능력에 부여된 판도라·스킬 번호·레벨·슬롯 방향 정보를 가져와 출처별 피해 보정과 쿨다운에 사용한다.
UPandoraSkillSource* UPdGameplayAbility::GetPandoraSkillSource() const
{
	return Cast<UPandoraSkillSource>(GetCurrentSourceObject());
}

// 투사체 생성 코드에 출처의 충돌 시 추가 영역 설정을 전달한다.
// 현재 UPandoraSkillSource 구현은 빈 목록을 반환하므로 이 경로에서는 추가 영역 설정이 전달되지 않는다.
TArray<FProjectileImpactEffectAreaSpawnConfig> UPdGameplayAbility::GetSourceProjectileImpactEffectAreas() const
{
	const UPandoraSkillSource* Source = GetPandoraSkillSource();
	return Source ? Source->GetProjectileImpactEffectAreas() : TArray<FProjectileImpactEffectAreaSpawnConfig>();
}

// 공통 입력 처리가 키 해제를 받았을 때 타기팅을 확정할지 알려 준다. 기본은 Press 스킬만 해당한다.
// 기본 공격 입력으로 확정하는 ProjectileAbility 등은 이 정책을 재정의한다.
bool UPdGameplayAbility::ShouldConfirmTargetingOnInputRelease() const
{
	const USkillDefinition* Skill = GetSourceSkillDataAsset();
	return Skill && Skill->SkillType == ESkillType::Press;
}

// 플레이어 조작과 AI 실행을 구분해, 공격 입력·콤보 진행·타기팅 확정 방식을 선택하게 한다.
bool UPdGameplayAbility::HasPlayerController() const
{
	const APawn* AvatarPawn = Cast<APawn>(GetAvatarActorFromActorInfo());
	const AController* Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	return Controller && Controller->IsPlayerController();
}

// AI 공격이나 자동 조준에서 Avatar의 타기팅 인터페이스가 정한 대상을 조회하고, 사망한 캐릭터는 제외한다.
AActor* UPdGameplayAbility::GetAttackTargetFromAvatar() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor) || !AvatarActor->GetClass()->ImplementsInterface(UTargetingInterface::StaticClass()))
	{
		return nullptr;
	}

	AActor* AttackTarget = ITargetingInterface::Execute_GetAttackTarget(AvatarActor);
	const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(AttackTarget);
	return TargetCharacter && TargetCharacter->IsDead() ? nullptr : AttackTarget;
}

// 파생 능력이 태그에 해당하는 다른 능력의 실행을 요청할 때 ASC로 전달한다.
// 원격 실행 허용 여부도 전달하며, 실제 활성화 가능 여부는 요청받은 능력과 GAS가 판단한다.
bool UPdGameplayAbility::TryActivateAbilitiesByTags(FGameplayTagContainer InAbilityTags, const bool bAllowRemoteActivation) const
{
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	return AbilitySystemComponent && !InAbilityTags.IsEmpty()
		&& AbilitySystemComponent->TryActivateAbilitiesByTag(InAbilityTags, bAllowRemoteActivation);
}

// 피해량과 상태 효과

// 스킬 설정의 기본 피해량을 0 이상으로 정리해 가져온다. 지능·판도라 슬롯 보정 전 수치다.
float UPdGameplayAbility::CalculateBaseSkillDamageMagnitude(const FSkillGameplayEffectConfig& DamageConfig) const
{
	return static_cast<float>(FMath::Max(DamageConfig.Magnitude, 0.0));
}

// 피해량에 지능과 이 능력의 출처 슬롯(왼쪽·위·오른쪽)에 해당하는 판도라 능력치의 퍼센트 보너스를 적용한다.
float UPdGameplayAbility::ApplyIntelligenceToSkillDamage(const float DamageMagnitude) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const UBasicAttributeSet* Attributes = ASC ? ASC->GetSet<UBasicAttributeSet>() : nullptr;
	float DamageBonusPercent = Attributes ? FMath::Max(Attributes->GetIntelligence(), 0.0f) : 0.0f;
	const UPandoraSkillSource* Source = GetPandoraSkillSource();
	if (Attributes && Source)
	{
		switch (Source->GetLoadoutDirection())
		{
		case EEnum_Direction::Left:
			DamageBonusPercent += FMath::Max(Attributes->GetFirstPandora(), 0.0f);
			break;
		case EEnum_Direction::Up:
			DamageBonusPercent += FMath::Max(Attributes->GetSecondPandora(), 0.0f);
			break;
		case EEnum_Direction::Right:
			DamageBonusPercent += FMath::Max(Attributes->GetThirdPandora(), 0.0f);
			break;
		default:
			break;
		}
	}
	return static_cast<float>(FMath::Max(DamageMagnitude, 0.0f) * (1.0 + static_cast<double>(DamageBonusPercent) * 0.01));
}

// 설정된 기본 피해량에 지능·판도라 슬롯 보너스를 합쳐, 피해 효과에 전달할 스킬 피해량을 계산한다.
float UPdGameplayAbility::CalculateSkillDamageMagnitude(const FSkillGameplayEffectConfig& DamageConfig) const
{
	return ApplyIntelligenceToSkillDamage(CalculateBaseSkillDamageMagnitude(DamageConfig));
}

// 공격자·출처·능력 레벨과 계산된 피해량을 담은 GameplayEffectSpec을 만들어 적중 처리에 넘긴다.
// 이 단계는 효과 생성만 하며, 대상에게 실제 피해를 적용하는 것은 호출부가 담당한다.
FGameplayEffectSpecHandle UPdGameplayAbility::MakeConfiguredDamageEffectSpec(
	const FSkillGameplayEffectConfig& DamageConfig, const float DamageMagnitude, UObject* SourceObject) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent || !DamageConfig.GameplayEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	if (SourceObject)
	{
		EffectContext.AddSourceObject(SourceObject);
	}
	else
	{
		EffectContext.AddSourceObject(GetCurrentSourceObject());
	}

	FGameplayEffectSpecHandle DamageSpecHandle =
		SourceAbilitySystemComponent->MakeOutgoingSpec(DamageConfig.GameplayEffectClass, FMath::Max(GetAbilityLevel(), 1), EffectContext);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayTag DamageDataTag = DamageConfig.MagnitudeDataTag;
	if (!DamageDataTag.IsValid())
	{
		SourceAbilitySystemComponent->ResolveDamageMagnitudeSetByCallerTag(DamageDataTag);
	}

	if (DamageDataTag.IsValid())
	{
		DamageSpecHandle.Data->SetSetByCallerMagnitude(DamageDataTag, DamageMagnitude);
	}

	return DamageSpecHandle;
}

// 상태 이상 정의 또는 호출자가 지정한 대체 효과로 레벨·중첩 수·지속시간을 담은 효과 Spec을 만든다.
// 상태 이상 정의가 있으면 중첩 한도와 공통 중첩 수명 규칙도 반영한다.
FGameplayEffectSpecHandle UPdGameplayAbility::MakeConfiguredStatusEffectSpec(const USkillDefinition* SkillDataAsset,
	const TSubclassOf<UGameplayEffect> FallbackStatusEffectClass, const float FallbackStatusEffectLevel) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	const UStatusEffectDefinition* StatusEffectDefinition = SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
	if (StatusEffectDefinition)
	{
		StatusEffectDefinition->SynchronizeDebuffGameplayEffectStackLimit();
	}
	const TSubclassOf<UGameplayEffect> DebuffGameplayEffectClass =
		StatusEffectDefinition ? StatusEffectDefinition->DebuffGameplayEffectClass : FallbackStatusEffectClass;
	if (!SourceAbilitySystemComponent || !DebuffGameplayEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystemComponent->MakeEffectContext();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	EffectContext.AddInstigator(AvatarActor, AvatarActor);
	EffectContext.AddSourceObject(GetCurrentSourceObject());

	const float StatusEffectLevel = StatusEffectDefinition && SkillDataAsset ? FMath::Max(SkillDataAsset->StatusEffectLevel, 1.0f)
																			 : FMath::Max(FallbackStatusEffectLevel, 1.0f);
	FGameplayEffectSpecHandle StatusEffectSpecHandle =
		SourceAbilitySystemComponent->MakeOutgoingSpec(DebuffGameplayEffectClass, StatusEffectLevel, EffectContext);
	if (!StatusEffectSpecHandle.IsValid() || !StatusEffectSpecHandle.Data.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}
	if (SkillDataAsset)
	{
		StatusEffectSpecHandle.Data->SetStackCount(FMath::Max(SkillDataAsset->StackCount, 1));
	}

	if (!StatusEffectDefinition)
	{
		return StatusEffectSpecHandle;
	}
	StatusEffectSpecHandle.Data->SetDuration(StatusEffectTiming::FullStackLifetimeSeconds, true);

	return StatusEffectSpecHandle;
}

// 대상에게 상태 이상을 누적할 수 있는지 확인한 뒤 효과를 적용하고, 성공한 효과를 상태 이상 복제 컴포넌트에 등록한다.
FActiveGameplayEffectHandle UPdGameplayAbility::ApplyConfiguredStatusEffectToTarget(const USkillDefinition* SkillDataAsset,
	UAbilitySystemComponent* TargetAbilitySystemComponent, const TSubclassOf<UGameplayEffect> FallbackStatusEffectClass,
	const float FallbackStatusEffectLevel) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent || !TargetAbilitySystemComponent)
	{
		return FActiveGameplayEffectHandle();
	}
	const UStatusEffectDefinition* StatusEffectDefinition = SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
	if (StatusEffectDefinition && !StatusEffectDefinition->CanAccumulateDebuffOn(TargetAbilitySystemComponent))
	{
		return FActiveGameplayEffectHandle();
	}

	const FGameplayEffectSpecHandle StatusEffectSpecHandle =
		MakeConfiguredStatusEffectSpec(SkillDataAsset, FallbackStatusEffectClass, FallbackStatusEffectLevel);
	if (!StatusEffectSpecHandle.IsValid() || !StatusEffectSpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*StatusEffectSpecHandle.Data.Get(), TargetAbilitySystemComponent);
	AActor* TargetActor = TargetAbilitySystemComponent->GetAvatarActor();
	if (AppliedHandle.WasSuccessfullyApplied() && StatusEffectDefinition && TargetActor)
	{
		if (UStatusEffectReplicationComponent* ReplicationComponent =
				TargetActor->FindComponentByClass<UStatusEffectReplicationComponent>())
		{
			ReplicationComponent->TrackAppliedStatusEffect(StatusEffectDefinition, AppliedHandle);
		}
	}

	return AppliedHandle;
}

// 공격 중 상태처럼 자기 자신에게 효과를 적용하고, 적용 성공 여부만 필요한 파생 능력에 결과를 반환한다.
bool UPdGameplayAbility::ApplyGameplayEffect(
	TSubclassOf<UGameplayEffect> GameplayEffectClass, const float EffectLevel, const int32 StackCount)
{
	return ApplyGameplayEffectHandle(GameplayEffectClass, EffectLevel, StackCount).WasSuccessfullyApplied();
}

// 자기 자신에게 효과를 적용하고, 종료 시 특정 효과를 제거하거나 추적할 수 있도록 핸들을 반환한다.
// 별도의 동적 부여 태그가 없는 호출은 빈 태그 목록으로 공통 적용 경로를 사용한다.
FActiveGameplayEffectHandle UPdGameplayAbility::ApplyGameplayEffectHandle(
	TSubclassOf<UGameplayEffect> GameplayEffectClass, const float EffectLevel, const int32 StackCount)
{
	const FGameplayTagContainer DynamicGrantedTags;
	return ApplyGameplayEffectHandle(GameplayEffectClass, DynamicGrantedTags, EffectLevel, StackCount);
}

// 권한 또는 예측 키를 확인한 뒤 자기 자신에게 적용할 효과 Spec에 레벨·중첩 수·추가 부여 태그를 담아 적용한다.
// 효과 핸들을 반환해 호출부가 적용된 효과를 추적하거나 종료 시 제거할 수 있게 한다.
FActiveGameplayEffectHandle UPdGameplayAbility::ApplyGameplayEffectHandle(TSubclassOf<UGameplayEffect> GameplayEffectClass,
	const FGameplayTagContainer& DynamicGrantedTags, const float EffectLevel, const int32 StackCount)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || !GameplayEffectClass || !ActorInfo || !HasAuthorityOrPredictionKey(ActorInfo, &CurrentActivationInfo))
	{
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(GameplayEffectClass, FMath::Max(EffectLevel, 1.0f));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	SpecHandle.Data->SetStackCount(FMath::Max(StackCount, 1));
	SpecHandle.Data->DynamicGrantedTags.AppendTags(DynamicGrantedTags);
	return ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, SpecHandle);
}

// 현재 자신에게 지정한 효과 클래스가 적용 중인지 확인해 공격 상태 효과 등의 중복 적용을 방지한다.
bool UPdGameplayAbility::HasActiveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass) const
{
	const UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || !GameplayEffectClass)
	{
		return false;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return !AbilitySystemComponent->GetActiveEffects(Query).IsEmpty();
}

// 서버에서 자신에게 적용된 지정 클래스의 효과들을 제거하고, 실제로 제거한 효과가 있는지 반환한다.
bool UPdGameplayAbility::RemoveGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || !GameplayEffectClass || !ActorInfo || !HasAuthority(&CurrentActivationInfo))
	{
		return false;
	}

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GameplayEffectClass;
	return AbilitySystemComponent->RemoveActiveEffects(Query) > 0;
}

// 무기 해제처럼 특정 상태를 일괄 해제할 때, 서버에서 지정 태그를 부여하는 자기 효과들을 제거하고 개수를 반환한다.
int32 UPdGameplayAbility::RemoveGameplayEffectsWithGrantedTags(const FGameplayTagContainer& GrantedTags)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!AbilitySystemComponent || GrantedTags.IsEmpty() || !ActorInfo || !HasAuthority(&CurrentActivationInfo))
	{
		return 0;
	}

	return AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(GrantedTags);
}

// 이동·무기·시각효과

// 타기팅·스킬 실행 중 이동을 제한하고, 잠금 전 이동·회전 설정을 매니저에 저장해 나중에 복구할 수 있게 한다.
void UPdGameplayAbility::LockAvatarMovementForAbility()
{
	MovementManager->LockAvatarMovementForAbility(*this);
}

// 타기팅 종료·스킬 종료 시 이 능력의 이동 잠금을 해제하고, 사망·빙결과 현재 조준 정책을 고려해 이동·회전을 복구한다.
void UPdGameplayAbility::RestoreAvatarMovementForAbility()
{
	MovementManager->RestoreAvatarMovementForAbility(*this);
}

// 이동 잠금이 설정된 Duration 스킬이 지속되는 동안 이동을 제한한다. 공통 EndAbility가 종료 시 잠금을 해제한다.
void UPdGameplayAbility::StartDurationMovementLock()
{
	MovementManager->StartDurationMovementLock(*this);
}

// 접촉 피해가 설정된 돌진·오라 등의 스킬에서 서버의 충돌 검사를 시작해, 이동 중 닿은 적에게 피해를 적용한다.
// 검사 타이머와 접촉 기록은 매니저가 보관하고 공통 EndAbility에서 정리한다.
void UPdGameplayAbility::StartMovementContactDamage()
{
	MovementManager->StartMovementContactDamage(*this);
}

// 시전 캐릭터의 장비 컴포넌트에서 현재 무기 액터를 가져와 궤적 연출과 자기 버프의 무기 판정 범위 조정에 사용한다.
AWeaponBase* UPdGameplayAbility::GetCurrentWeaponActorFromAvatar() const
{
	const ACharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

// 무기 궤적 스킬을 실행하기 전에 현재 무기에 궤적용 Niagara 컴포넌트가 있는지 확인한다.
bool UPdGameplayAbility::HasCurrentWeaponSkillTrail() const
{
	const AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar();
	return CurrentWeapon && CurrentWeapon->HasSkillWeaponTrailComponent();
}

// Trail 스킬이 현재 무기의 Niagara 궤적 연출을 시작하도록 요청하고, 시작 성공 여부를 반환한다.
bool UPdGameplayAbility::StartCurrentWeaponSkillTrail(UNiagaraSystem* TrailSystem) const
{
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar();
	return CurrentWeapon ? CurrentWeapon->StartSkillWeaponTrail(TrailSystem) : false;
}

// Trail 스킬 종료 시 현재 무기의 궤적 연출을 중단해 공격 효과가 남지 않게 한다.
void UPdGameplayAbility::StopCurrentWeaponSkillTrail() const
{
	if (AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar())
	{
		CurrentWeapon->StopSkillWeaponTrail();
	}
}

// 파생 스킬이 자기 능력의 연출 관리 객체에 이펙트·데칼·오버레이·미사일 연출의 시작·갱신·중단을 요청하게 한다.
UAbilityPresentationManager& UPdGameplayAbility::GetPresentationManager()
{
	return *PresentationManager;
}

// 상태를 변경하지 않는 문맥에서 이 능력의 연출 관리 객체를 조회해 데칼 위치·지속시간 같은 연출 정보를 계산하게 한다.
const UAbilityPresentationManager& UPdGameplayAbility::GetPresentationManager() const
{
	return *PresentationManager;
}

// 재시전·능력 종료·ASC의 강제 정리 때 이 능력의 활성 연출 액터를 서버에서 제거하고 보관 참조를 비운다.
void UPdGameplayAbility::DestroyActiveSkillPresentationActor()
{
	PresentationManager->DestroyActiveSkillPresentationActor();
}

// 자기 버프 적용과 해제

// 시전 확정 후 자기 버프 설정에 따라 캐릭터 확대·무기 판정 범위 증가를 요청하고, 서버에서 무기 피해 보너스·효과를 적용한다.
// 되돌릴 대상과 종료 시 제거할 효과 핸들을 저장해 다른 능력의 버프와 구분한다.
void UPdGameplayAbility::StartConfiguredSelfBuff(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	StopConfiguredSelfBuff();
	const USkillDefinition* SkillDefinition = GetSourceSkillDataAsset();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!SkillDefinition || !SkillDefinition->SelfBuff.bEnabled || !ASC || !GetAvatarActorFromActorInfo())
	{
		return;
	}
	const FSkillSelfBuffSettings& Settings = SkillDefinition->SelfBuff;
	PresentationManager->ApplySelfBuffCharacterScale(*this, Settings);
	if (Settings.WeaponTraceEndZMultiplier > 1.0)
	{
		if (AWeaponBase* Weapon = GetCurrentWeaponActorFromAvatar())
		{
			Weapon->SetTemporaryAttackTraceEndZMultiplier(this, static_cast<float>(Settings.WeaponTraceEndZMultiplier));
			SelfBuffTraceEndZWeapon = Weapon;
		}
	}
	// 버프 수치는 서버가 확정한다. 외형과 무기 검사 범위의 로컬 처리는 기존 방식대로 유지한다.
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	if (UCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr; Combat && Settings.WeaponDamageBonus > 0.0)
	{
		Combat->SetTemporaryWeaponDamageBonus(this, static_cast<float>(Settings.WeaponDamageBonus));
		SelfBuffCombatComponent = Combat;
	}
	if (!Settings.GameplayEffectClass)
	{
		return;
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	Context.AddSourceObject(SkillDefinition);
	FGameplayEffectSpecHandle Spec =
		ASC->MakeOutgoingSpec(Settings.GameplayEffectClass, FMath::Max(GetAbilityLevel(Handle, ActorInfo), 1), Context);
	if (!Spec.IsValid())
	{
		return;
	}
	if (Settings.MagnitudeDataTag.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(Settings.MagnitudeDataTag, static_cast<float>(Settings.Magnitude));
	}
	const FActiveGameplayEffectHandle AppliedHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	if (AppliedHandle.IsValid() && Settings.bRemoveOnAbilityEnd)
	{
		ActiveSelfBuffEffectHandle = AppliedHandle;
		SelfBuffAbilitySystemComponent = ASC;
	}
}

// 자기 버프가 실제로 적용되었던 캐릭터·무기·전투 컴포넌트에서 이 능력의 변경분과 종료 시 제거 대상 효과를 해제한다.
// 시전 도중 장비나 Avatar가 바뀌어도 현재 대상이 아닌 저장된 적용 대상을 정리한다.
void UPdGameplayAbility::StopConfiguredSelfBuff()
{
	PresentationManager->RestoreSelfBuffCharacterScale(*this);
	if (AWeaponBase* Weapon = SelfBuffTraceEndZWeapon.Get())
	{
		Weapon->ClearTemporaryAttackTraceEndZMultiplier(this);
	}
	SelfBuffTraceEndZWeapon.Reset();
	if (UCombatComponent* Combat = SelfBuffCombatComponent.Get())
	{
		Combat->ClearTemporaryWeaponDamageBonus(this);
	}
	SelfBuffCombatComponent.Reset();
	if (UAbilitySystemComponent* ASC = SelfBuffAbilitySystemComponent.Get(); ASC && ASC->IsOwnerActorAuthoritative())
	{
		ASC->RemoveActiveGameplayEffect(ActiveSelfBuffEffectHandle);
	}
	ActiveSelfBuffEffectHandle.Invalidate();
	SelfBuffAbilitySystemComponent.Reset();
}

// 몽타주·이벤트·타기팅 태스크

// 공격·시전 몽타주를 재생하고 완료·중단을 기다릴 태스크를 만들며, 능력 종료 시 몽타주도 멈추도록 설정한다.
// 호출부가 완료·중단 콜백을 연결하고 ReadyForActivation을 호출해 재생을 시작한다.
UAbilityTask_PlayMontageAndWait* UPdGameplayAbility::CreateDefaultMontageAndWaitTask(UAnimMontage* MontageToPlay)
{
	if (!MontageToPlay)
	{
		return nullptr;
	}

	return UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, MontageToPlay, 1.0f, NAME_None, true, 1.0f, 0.0f, true);
}

// 몽타주 노티파이의 발사·타격·장착 확정 신호 등을 기다리는 GameplayEvent 태스크를 만든다.
// 호출부가 처리 함수를 연결하고 활성화하며, 한 번만 받을지와 정확한 태그만 받을지는 인자로 정한다.
UAbilityTask_WaitGameplayEvent* UPdGameplayAbility::CreateWaitGameplayEventTask(
	const FGameplayTag& EventTag, const bool bOnlyTriggerOnce, const bool bOnlyMatchExact)
{
	if (!EventTag.IsValid())
	{
		return nullptr;
	}

	return UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag, nullptr, bOnlyTriggerOnce, bOnlyMatchExact);
}

// 범위·발사 위치 등을 선택할 타기팅 액터의 지연 생성을 시작하고, 스킬이 설정을 채울 수 있도록 액터를 반환한다.
AGameplayAbilityTargetActor* UPdGameplayAbility::BeginSpawningTargetDataActor(
	UAbilityTask_WaitTargetData* TargetDataTask, const TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass)
{
	if (!TargetDataTask || !TargetActorClass)
	{
		return nullptr;
	}

	AGameplayAbilityTargetActor* SpawnedActor = nullptr;
	return TargetDataTask->BeginSpawningActor(this, TargetActorClass, SpawnedActor) ? SpawnedActor : nullptr;
}

// 파생 스킬이 범위·위치 등의 설정을 채운 타기팅 액터의 생성을 완료하고, WaitTargetData 태스크의 타기팅 시작 단계로 넘긴다.
void UPdGameplayAbility::FinishSpawningTargetDataActor(
	UAbilityTask_WaitTargetData* TargetDataTask, AGameplayAbilityTargetActor* SpawnedActor)
{
	if (TargetDataTask && SpawnedActor)
	{
		TargetDataTask->FinishSpawningActor(this, SpawnedActor);
	}
}
