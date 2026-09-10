#include "Component/Character/AbilityStateComponent.h"

#include "AIController.h"
#include "AbilitySystem/Ability/Reactive/ReactiveRecoveryAbility.h"
#include "Mode/PdPlayerState.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/CharacterDeathComponent.h"
#include "Component/Character/CharacterHealthBarComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityStateComponent)

namespace
{
constexpr int32 ActorInfoMaxRetryAttempts = 10;
constexpr float ActorInfoRetryInterval = 0.05f;
constexpr float MinimumMaxWalkSpeed = 150.0f;
constexpr int32 MovementSpeedAttributeMaxRetryAttempts = 50;
constexpr float StaminaRegenDelay = 1.0f;
constexpr float StaminaRegenEffectLevel = 1.0f;
} // namespace

// 캐릭터의 ASC 연결과 능력 상태를 관리한다. 독립 Tick은 끄고, 빙결 중에는 캐릭터 Tick에서 잠금을 유지한다.
UAbilityStateComponent::UAbilityStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// 캐릭터 설정이 적용된 기본 걷기 속도를 저장해, 능력치·장비·스태미나 보정의 기준으로 사용한다.
void UAbilityStateComponent::CaptureBaseMovementSpeed()
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (const UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr)
	{
		BaseMaxWalkSpeed = MovementComponent->MaxWalkSpeed;
	}
}

// 빙의·PlayerState 복제·캐릭터 초기화 시 이전 재시도를 초기화하고 현재 Owner와 Avatar의 ASC 연결을 시도한다.
void UAbilityStateComponent::InitializeAbilitySystemActorInfo()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActorInfoInitializationRetryTimerHandle);
	}
	bActorInfoInitializationQueued = false;
	ActorInfoInitializationRetryCount = 0;
	TryInitializeAbilitySystemActorInfo();
}

// 이전 Pawn의 ASC 연결을 정리하고 현재 캐릭터에 상태 구독과 기본 회복 능력을 연결한다. ASC가 미준비이면 재시도한다.
void UAbilityStateComponent::TryInitializeAbilitySystemActorInfo()
{
	ACharacterBase* Character = GetCharacterOwner();
	UPdAbilitySystemComponent* ASC = Character ? Character->GetPdAbilitySystemComponent() : nullptr;
	AActor* OwnerActor = Character ? Character->GetAbilitySystemOwnerActor() : nullptr;
	AActor* AvatarActor = Character ? Character->GetAbilitySystemAvatarActor() : nullptr;
	if (BoundAbilitySystemComponent.IsValid() && BoundAbilitySystemComponent.Get() != ASC)
	{
		ClearAbilitySystemActorInfo();
	}
	// 이전 Pawn의 늦은 초기화 요청으로 PlayerState의 새 Avatar를 빼앗지 않는다.
	if (const APdPlayerState* PlayerState = Cast<APdPlayerState>(OwnerActor); PlayerState && PlayerState->GetPawn() != AvatarActor)
	{
		ClearAbilitySystemActorInfo();
		return;
	}
	if (!Character || !ASC || !OwnerActor || !AvatarActor || !ASC->IsRegistered() || !ASC->HasAbilityActorInfoAllocated())
	{
		QueueAbilitySystemActorInfoInitializationRetry();
		if (Character && Character->GetCharacterHealthBarComponent())
		{
			Character->GetCharacterHealthBarComponent()->TryRefreshViewModel();
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActorInfoInitializationRetryTimerHandle);
	}
	bActorInfoInitializationQueued = false;
	ActorInfoInitializationRetryCount = 0;

	// 같은 ASC가 새 캐릭터로 넘어갈 때 이전 캐릭터의 구독부터 해제한다.
	if (ACharacterBase* PreviousAvatar = Cast<ACharacterBase>(ASC->GetAvatarActor()); PreviousAvatar && PreviousAvatar != Character)
	{
		PreviousAvatar->ClearAbilitySystemActorInfo();
	}
	BoundAbilitySystemComponent = ASC;
	ASC->InitAbilityActorInfo(OwnerActor, AvatarActor);
	if (Character->HasAuthority() && OwnerActor->IsA<APdPlayerState>())
	{
		// 플레이어의 기본 패시브이며, 중복 부여 방지는 기존 능력 목록 API가 담당한다.
		ASC->GrantAbilities({UReactiveRecoveryAbility::StaticClass()}, 1, OwnerActor);
		if (const FGameplayAbilitySpec* RecoverySpec = ASC->FindAbilitySpecFromClass(UReactiveRecoveryAbility::StaticClass());
			RecoverySpec && !RecoverySpec->IsActive())
		{
			ASC->TryActivateAbility(RecoverySpec->Handle);
		}
	}
	BindStaminaRegenToASC(ASC);
	BindMovementSpeedAttributeToASC(ASC);
	BindDeadTagEvent(ASC);
	BindFrozenTagEvent(ASC);
	RefreshAirborneGameplayTag();

	if (UEquipmentComponent* EquipmentComponent = Character->GetEquipmentComponent())
	{
		EquipmentComponent->RefreshCachedReferences();
	}
	if (UCombatComponent* CombatComponent = Character->GetCombatComponent())
	{
		CombatComponent->RefreshCachedReferences();
	}
	if (Character->GetCharacterHealthBarComponent())
	{
		Character->GetCharacterHealthBarComponent()->RefreshViewModel();
	}
}

// ASC 등록과 PlayerState 준비가 늦게 끝나는 경우 0.05초 간격으로 최대 10회 연결을 재시도한다.
void UAbilityStateComponent::QueueAbilitySystemActorInfoInitializationRetry()
{
	UWorld* World = GetWorld();
	if (bActorInfoInitializationQueued || !World)
	{
		return;
	}

	if (ActorInfoInitializationRetryCount >= ActorInfoMaxRetryAttempts)
	{
		return;
	}

	bActorInfoInitializationQueued = true;
	++ActorInfoInitializationRetryCount;
	World->GetTimerManager().SetTimer(ActorInfoInitializationRetryTimerHandle,
		FTimerDelegate::CreateWeakLambda(this,
			[this]() {
				bActorInfoInitializationQueued = false;
				ActorInfoInitializationRetryTimerHandle.Invalidate();
				TryInitializeAbilitySystemActorInfo();
			}),
		ActorInfoRetryInterval, false);
}

// 빙의 해제·Pawn 교체·종료 시 자신이 구독한 상태와 타이머를 정리한다. ASC의 Avatar가 자신일 때만 ActorInfo를 비운다.
void UAbilityStateComponent::ClearAbilitySystemActorInfo()
{
	ACharacterBase* Character = GetCharacterOwner();
	UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get();
	if (!ASC && Character)
	{
		ASC = Character->GetAbilitySystemComponent();
	}

	UnbindFrozenTagEvent();
	UnbindStaminaRegenFromASC();
	UnbindMovementSpeedAttribute();
	UnbindDeadTagEvent();
	BoundAbilitySystemComponent.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActorInfoInitializationRetryTimerHandle);
	}
	bActorInfoInitializationQueued = false;
	ActorInfoInitializationRetryCount = 0;

	if (ASC && Character && ASC->GetAvatarActor() == Character)
	{
		ASC->SetLooseGameplayTagCount(LabGameplayTags::State_Movement_Airborne, 0,
			Character->HasAuthority() ? EGameplayTagReplicationState::CountToOwner : EGameplayTagReplicationState::None);
		ASC->CancelAbility(UReactiveRecoveryAbility::StaticClass()->GetDefaultObject<UGameplayAbility>());
		if (ASC->GetAvatarActor() == Character)
		{
			ASC->ClearActorInfo();
		}
	}
}

// 이동속도·현재 스태미나·최대 스태미나 변경을 같은 콜백에 연결하고 현재 장비와 상태에 맞춰 속도를 갱신한다.
void UAbilityStateComponent::BindMovementSpeedAttributeToASC(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (MovementAttributesAbilitySystemComponent.Get() == AbilitySystemComponent && MovementSpeedAttributeChangedDelegateHandle.IsValid()
		&& MovementStaminaAttributeChangedDelegateHandle.IsValid() && MovementMaxStaminaAttributeChangedDelegateHandle.IsValid())
	{
		ApplyMovementSpeedFromAttribute();
		return;
	}

	UnbindMovementSpeedAttribute();
	if (!AbilitySystemComponent)
	{
		return;
	}

	MovementAttributesAbilitySystemComponent = AbilitySystemComponent;
	MovementSpeedAttributeRetryAttempts = 0;
	MovementSpeedAttributeChangedDelegateHandle =
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMovementSpeedAttribute())
			.AddUObject(this, &ThisClass::HandleMovementAttributesChanged);
	MovementStaminaAttributeChangedDelegateHandle =
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute())
			.AddUObject(this, &ThisClass::HandleMovementAttributesChanged);
	MovementMaxStaminaAttributeChangedDelegateHandle =
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaAttribute())
			.AddUObject(this, &ThisClass::HandleMovementAttributesChanged);
	ApplyMovementSpeedFromAttribute();
}

// 이동속도나 스태미나 값이 실제로 달라졌을 때 캐릭터의 걷기 속도와 저스태미나 연출을 다시 계산한다.
void UAbilityStateComponent::HandleMovementAttributesChanged(const FOnAttributeChangeData& Data)
{
	if (!FMath::IsNearlyEqual(Data.NewValue, Data.OldValue))
	{
		ApplyMovementSpeedFromAttribute();
	}
}

// 기본 속도에 능력치·무기·스태미나 보정을 적용한다. 빙결 중에는 속도를 0으로 유지하고 속성셋 미도착 시 재시도한다.
void UAbilityStateComponent::ApplyMovementSpeedFromAttribute()
{
	ACharacterBase* Character = GetCharacterOwner();
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	UAbilitySystemComponent* ASC = MovementAttributesAbilitySystemComponent.Get();
	if (!MovementComponent || !ASC)
	{
		return;
	}

	if (!ASC->GetAttributeSet(UBasicAttributeSet::StaticClass()))
	{
		QueueMovementSpeedAttributeApplyRetry();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovementSpeedAttributeRetryTimerHandle);
	}
	MovementSpeedAttributeRetryAttempts = 0;

	const float CurrentStamina = ASC->GetNumericAttribute(UBasicAttributeSet::GetStaminaAttribute());
	const float CurrentMaxStamina = ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxStaminaAttribute());
	const float StaminaRatio = CurrentMaxStamina > UE_SMALL_NUMBER ? FMath::Clamp(CurrentStamina / CurrentMaxStamina, 0.0f, 1.0f) : 1.0f;
	const float StaminaPercent = StaminaRatio * 100.0f;

	const UGameSettingDefinition* SettingDefinition = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	if (!SettingDefinition)
	{
		SettingDefinition = GetDefault<UGameSettingDefinition>();
	}
	RefreshLowStaminaEffectComponent(StaminaPercent, SettingDefinition);

	if (bFrozenMovementActive)
	{
		if (!FMath::IsNearlyZero(MovementComponent->MaxWalkSpeed))
		{
			MovementComponent->StopMovementImmediately();
			MovementComponent->MaxWalkSpeed = 0.0f;
		}
		return;
	}

	const float MovementSpeedAttribute = ASC->GetNumericAttribute(UBasicAttributeSet::GetMovementSpeedAttribute());
	const float NormalMaxWalkSpeed =
		FMath::Max(MinimumMaxWalkSpeed, BaseMaxWalkSpeed * (1.0f + FMath::Max(MovementSpeedAttribute, 0.0f) * 0.01f));
	const UEquipmentComponent* EquipmentComponent = Character->GetEquipmentComponent();
	const UItemDefinition* EquippedWeaponDefinition = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponDefinition() : nullptr;
	const float EquippedWeaponMovementSpeedMultiplier =
		EquippedWeaponDefinition ? EquippedWeaponDefinition->GetEquippedMovementSpeedMultiplier() : 1.0f;

	float StaminaSpeedMultiplier = 1.0f;
	if (StaminaPercent <= FMath::Clamp(SettingDefinition->CriticalStaminaThresholdPercent, 0.0f, 100.0f))
	{
		StaminaSpeedMultiplier = 1.0f - FMath::Clamp(SettingDefinition->CriticalStaminaMovementSpeedReductionPercent, 0.0f, 100.0f) * 0.01f;
	}
	else if (StaminaPercent <= FMath::Clamp(SettingDefinition->LowStaminaThresholdPercent, 0.0f, 100.0f))
	{
		StaminaSpeedMultiplier = 1.0f - FMath::Clamp(SettingDefinition->LowStaminaMovementSpeedReductionPercent, 0.0f, 100.0f) * 0.01f;
	}

	const float NewMaxWalkSpeed = NormalMaxWalkSpeed * EquippedWeaponMovementSpeedMultiplier * StaminaSpeedMultiplier;
	if (!FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, NewMaxWalkSpeed))
	{
		MovementComponent->MaxWalkSpeed = NewMaxWalkSpeed;
	}
}

// ASC는 연결됐지만 기본 속성셋이 아직 도착하지 않은 경우 0.1초 주기의 이동속도 적용 재시도를 예약한다.
void UAbilityStateComponent::QueueMovementSpeedAttributeApplyRetry()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(MovementSpeedAttributeRetryTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		MovementSpeedAttributeRetryTimerHandle, this, &ThisClass::RetryApplyMovementSpeedFromAttribute, 0.1f, true);
}

// 복제된 속성셋 준비를 최대 50회 기다리며 이동속도를 적용한다. 제한을 넘으면 확인 타이머를 종료한다.
void UAbilityStateComponent::RetryApplyMovementSpeedFromAttribute()
{
	++MovementSpeedAttributeRetryAttempts;
	if (MovementSpeedAttributeRetryAttempts > MovementSpeedAttributeMaxRetryAttempts)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(MovementSpeedAttributeRetryTimerHandle);
		}
		return;
	}
	ApplyMovementSpeedFromAttribute();
}

// 스태미나 비율이 설정된 임계값 이하일 때 캐릭터의 저스태미나 이펙트를 켜고, 회복하면 끈다.
void UAbilityStateComponent::RefreshLowStaminaEffectComponent(const float StaminaPercent, const UGameSettingDefinition* SettingDefinition)
{
	if (!SettingDefinition)
	{
		return;
	}

	UActorComponent* EffectComponent = ResolveLowStaminaEffectComponent(SettingDefinition->LowStaminaEffectComponentName);
	if (!EffectComponent)
	{
		return;
	}

	const bool bShouldBeActive = StaminaPercent <= FMath::Clamp(SettingDefinition->LowStaminaThresholdPercent, 0.0f, 100.0f);
	if (EffectComponent->IsActive() != bShouldBeActive)
	{
		EffectComponent->SetActive(bShouldBeActive, true);
	}
}

// 설정에 지정된 이름 또는 컴포넌트 태그로 저스태미나 이펙트를 찾아 재사용하며, 제거된 컴포넌트는 다시 찾는다.
UActorComponent* UAbilityStateComponent::ResolveLowStaminaEffectComponent(const FName ComponentName)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (ComponentName.IsNone() || !Character)
	{
		LowStaminaEffectComponent.Reset();
		CachedLowStaminaEffectComponentName = NAME_None;
		return nullptr;
	}

	if (CachedLowStaminaEffectComponentName == ComponentName && LowStaminaEffectComponent.IsValid())
	{
		return LowStaminaEffectComponent.Get();
	}

	LowStaminaEffectComponent.Reset();
	CachedLowStaminaEffectComponentName = ComponentName;
	TArray<UActorComponent*> ActorComponents;
	Character->GetComponents<UActorComponent>(ActorComponents);
	for (UActorComponent* ActorComponent : ActorComponents)
	{
		if (ActorComponent && (ActorComponent->GetFName() == ComponentName || ActorComponent->ComponentHasTag(ComponentName)))
		{
			LowStaminaEffectComponent = ActorComponent;
			return ActorComponent;
		}
	}
	return nullptr;
}

// 이전 ASC의 이동 관련 속성 구독과 속성 준비 재시도를 해제해, 교체 전 Pawn이 새 속도 변경을 받지 않게 한다.
void UAbilityStateComponent::UnbindMovementSpeedAttribute()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovementSpeedAttributeRetryTimerHandle);
	}

	if (UAbilitySystemComponent* ASC = MovementAttributesAbilitySystemComponent.Get())
	{
		if (MovementSpeedAttributeChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMovementSpeedAttribute())
				.Remove(MovementSpeedAttributeChangedDelegateHandle);
		}
		if (MovementStaminaAttributeChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute())
				.Remove(MovementStaminaAttributeChangedDelegateHandle);
		}
		if (MovementMaxStaminaAttributeChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaAttribute())
				.Remove(MovementMaxStaminaAttributeChangedDelegateHandle);
		}
	}

	MovementSpeedAttributeChangedDelegateHandle.Reset();
	MovementStaminaAttributeChangedDelegateHandle.Reset();
	MovementMaxStaminaAttributeChangedDelegateHandle.Reset();
	MovementAttributesAbilitySystemComponent.Reset();
	MovementSpeedAttributeRetryAttempts = 0;
}

// 서버와 소유 클라이언트가 낙하 상태를 GAS 태그에 반영한다. 서버는 공중 상태에서 일반 공격·주먹·원거리 공격 능력을 취소한다.
void UAbilityStateComponent::RefreshAirborneGameplayTag()
{
	ACharacterBase* Character = GetCharacterOwner();
	UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get();
	if (!Character || !ASC || ASC->GetAvatarActor() != Character || (!Character->HasAuthority() && !Character->IsLocallyControlled()))
	{
		return;
	}

	const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();
	const bool bAirborne = MovementComponent && MovementComponent->IsFalling();
	const EGameplayTagReplicationState ReplicationState =
		Character->HasAuthority() ? EGameplayTagReplicationState::CountToOwner : EGameplayTagReplicationState::None;
	ASC->SetLooseGameplayTagCount(LabGameplayTags::State_Movement_Airborne, bAirborne ? 1 : 0, ReplicationState);

	if (bAirborne && Character->HasAuthority())
	{
		FGameplayTagContainer AttackAbilityTags;
		AttackAbilityTags.AddTag(LabGameplayTags::Action_Attack);
		AttackAbilityTags.AddTag(LabGameplayTags::Action_Punch);
		AttackAbilityTags.AddTag(LabGameplayTags::Action_RangedAttack);
		ASC->CancelAbilities(&AttackAbilityTags);
	}
}

// ASC의 Dead 태그 변경을 구독하고 이미 사망한 상태로 연결됐다면 사망 컴포넌트에 즉시 전달한다.
void UAbilityStateComponent::BindDeadTagEvent(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (DeadTagBoundAbilitySystemComponent.Get() == AbilitySystemComponent && DeadTagChangedDelegateHandle.IsValid())
	{
		return;
	}

	UnbindDeadTagEvent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	DeadTagBoundAbilitySystemComponent = AbilitySystemComponent;
	DeadTagChangedDelegateHandle =
		AbilitySystemComponent->RegisterGameplayTagEvent(LabGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnDeadTagChanged);

	const int32 CurrentDeadTagCount = AbilitySystemComponent->GetTagCount(LabGameplayTags::State_Dead);
	if (CurrentDeadTagCount > 0)
	{
		OnDeadTagChanged(LabGameplayTags::State_Dead, CurrentDeadTagCount);
	}
}

// Dead 태그 개수와 해당 ASC를 사망 컴포넌트에 전달해 서버 사망 처리와 클라이언트 연출을 연결한다.
void UAbilityStateComponent::OnDeadTagChanged(FGameplayTag, const int32 NewCount)
{
	ACharacterBase* Character = GetCharacterOwner();
	UCharacterDeathComponent* DeathComponent = Character ? Character->GetCharacterDeathComponent() : nullptr;
	if (DeathComponent)
	{
		DeathComponent->HandleDeadTagChanged(NewCount, DeadTagBoundAbilitySystemComponent.Get());
	}
}

// 자신이 연결했던 ASC에서 Dead 태그 구독을 해제한다.
void UAbilityStateComponent::UnbindDeadTagEvent()
{
	if (UAbilitySystemComponent* ASC = DeadTagBoundAbilitySystemComponent.Get())
	{
		if (DeadTagChangedDelegateHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(LabGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
				.Remove(DeadTagChangedDelegateHandle);
		}
	}
	DeadTagChangedDelegateHandle.Reset();
	DeadTagBoundAbilitySystemComponent.Reset();
}

// 빙결 태그를 구독하고 현재 태그 개수도 즉시 반영해, 이미 얼어 있는 캐릭터의 이동과 회전을 잠근다.
void UAbilityStateComponent::BindFrozenTagEvent(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (FrozenTagBoundAbilitySystemComponent.Get() == AbilitySystemComponent && FrozenTagChangedDelegateHandle.IsValid())
	{
		OnFrozenTagChanged(LabGameplayTags::Status_Frostbite,
			AbilitySystemComponent ? AbilitySystemComponent->GetTagCount(LabGameplayTags::Status_Frostbite) : 0);
		return;
	}

	UnbindFrozenTagEvent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	FrozenTagBoundAbilitySystemComponent = AbilitySystemComponent;
	FrozenTagChangedDelegateHandle =
		AbilitySystemComponent->RegisterGameplayTagEvent(LabGameplayTags::Status_Frostbite, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnFrozenTagChanged);
	OnFrozenTagChanged(LabGameplayTags::Status_Frostbite, AbilitySystemComponent->GetTagCount(LabGameplayTags::Status_Frostbite));
}

// 빙결 시 공격·조준·그래플·AI 이동을 중단하고 회전과 입력을 잠근다. 해제 시 살아 있는 캐릭터의 회전 정책과 속도를 복구한다.
void UAbilityStateComponent::OnFrozenTagChanged(FGameplayTag, const int32 NewCount)
{
	ACharacterBase* Character = GetCharacterOwner();
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !MovementComponent)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	const bool bIsDead = ASC && ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead);
	if (NewCount > 0)
	{
		if (bIsDead)
		{
			return;
		}

		if (UCombatComponent* CombatComponent = Character->GetCombatComponent())
		{
			CombatComponent->StopPrimaryAttack();
			CombatComponent->StopAim();
		}

		if (ASC)
		{
			FGameplayTagContainer GrappleAbilityTags;
			GrappleAbilityTags.AddTag(LabGameplayTags::GameplayAbility_Movement_Grapple);
			ASC->CancelAbilities(&GrappleAbilityTags);
		}

		if (AAIController* AIController = Cast<AAIController>(Character->GetController()))
		{
			AIController->StopMovement();
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}

		if (!bFrozenMovementActive)
		{
			bFrozenRotationStateCached = true;
			FrozenLockedActorRotation = Character->GetActorRotation();
			FrozenLockedControlRotation =
				Character->GetController() ? Character->GetController()->GetControlRotation() : FrozenLockedActorRotation;
			FrozenLockedMeshRelativeRotation = Character->GetMesh() ? Character->GetMesh()->GetRelativeRotation() : FRotator::ZeroRotator;
			FrozenLockedAimYaw = Character->GetAimYawForAnimation();
			FrozenLockedAimPitch = Character->GetAimPitchForAnimation();
			bFrozenCachedOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
			bFrozenCachedUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
			bFrozenCachedUseControllerRotationYaw = Character->bUseControllerRotationYaw;
			FrozenCachedRotationRate = MovementComponent->RotationRate;
			bFrozenMovementActive = true;
			Character->RefreshCharacterTickEnabled();
		}

		if (AController* Controller = Character->GetController(); Controller && !FrozenInputController.IsValid())
		{
			Controller->SetIgnoreLookInput(true);
			FrozenInputController = Controller;
		}
		MaintainFrozenRotationLock();
		return;
	}

	if (!bFrozenMovementActive)
	{
		return;
	}

	bFrozenMovementActive = false;
	if (AController* Controller = FrozenInputController.Get())
	{
		Controller->SetIgnoreLookInput(false);
	}
	FrozenInputController.Reset();

	if (bFrozenRotationStateCached)
	{
		if (!bIsDead)
		{
			Character->RestoreRotationSettingsAfterFrozen(MovementComponent);
		}
		bFrozenRotationStateCached = false;
	}
	if (!bIsDead)
	{
		ApplyMovementSpeedFromAttribute();
	}
	Character->RefreshCharacterTickEnabled();
}

// 빙결 동안 이동속도와 회전 정책을 잠그고, 저장한 몸·조종자·메시·조준 각도를 유지한다. 사망 후 래그돌 회전은 건드리지 않는다.
void UAbilityStateComponent::MaintainFrozenRotationLock()
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character || !bFrozenMovementActive || !bFrozenRotationStateCached)
	{
		return;
	}

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (ASC && ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return;
	}

	const FRotator CurrentActorRotation = Character->GetActorRotation();
	const FRotator CurrentControlRotation =
		Character->GetController() ? Character->GetController()->GetControlRotation() : FRotator::ZeroRotator;
	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	const bool bMeshSimulatingPhysics = CharacterMesh && CharacterMesh->IsSimulatingPhysics();
	const FRotator CurrentMeshRelativeRotation = CharacterMesh ? CharacterMesh->GetRelativeRotation() : FRotator::ZeroRotator;

	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->MaxWalkSpeed = 0.0f;
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->bUseControllerDesiredRotation = false;
		MovementComponent->RotationRate = FRotator::ZeroRotator;
	}
	Character->bUseControllerRotationYaw = false;
	Character->SetAimOffsetForAnimation(FrozenLockedAimYaw, FrozenLockedAimPitch);

	if (!CurrentActorRotation.Equals(FrozenLockedActorRotation, 0.01f))
	{
		Character->SetActorRotation(FrozenLockedActorRotation, ETeleportType::TeleportPhysics);
	}

	if (AController* Controller = Character->GetController();
		Controller && !CurrentControlRotation.Equals(FrozenLockedControlRotation, 0.01f))
	{
		Controller->SetControlRotation(FrozenLockedControlRotation);
	}

	if (CharacterMesh && !bMeshSimulatingPhysics && !CurrentMeshRelativeRotation.Equals(FrozenLockedMeshRelativeRotation, 0.01f))
	{
		CharacterMesh->SetRelativeRotation(FrozenLockedMeshRelativeRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

// 빙결 시작 전에 저장한 이동 방향 회전·컨트롤러 회전·회전 속도를 캐릭터에 복구한다.
void UAbilityStateComponent::RestoreCachedRotationSettings(UCharacterMovementComponent* MovementComponent) const
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character || !MovementComponent)
	{
		return;
	}

	MovementComponent->bOrientRotationToMovement = bFrozenCachedOrientRotationToMovement;
	MovementComponent->bUseControllerDesiredRotation = bFrozenCachedUseControllerDesiredRotation;
	MovementComponent->RotationRate = FrozenCachedRotationRate;
	Character->bUseControllerRotationYaw = bFrozenCachedUseControllerRotationYaw;
}

// 재사용할 캐릭터의 빙결 입력 잠금과 저장 상태를 해제해 리스폰 복구를 준비한다.
void UAbilityStateComponent::ClearFrozenStateForRespawn()
{
	OnFrozenTagChanged(LabGameplayTags::Status_Frostbite, 0);
}

// 빙결 상태와 자신이 걸었던 시선 입력 잠금을 먼저 해제한 뒤 이전 ASC의 빙결 태그 구독을 제거한다.
void UAbilityStateComponent::UnbindFrozenTagEvent()
{
	if (bFrozenMovementActive)
	{
		OnFrozenTagChanged(LabGameplayTags::Status_Frostbite, 0);
	}

	if (UAbilitySystemComponent* ASC = FrozenTagBoundAbilitySystemComponent.Get())
	{
		if (FrozenTagChangedDelegateHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(LabGameplayTags::Status_Frostbite, EGameplayTagEventType::NewOrRemoved)
				.Remove(FrozenTagChangedDelegateHandle);
		}
	}
	FrozenTagChangedDelegateHandle.Reset();
	FrozenTagBoundAbilitySystemComponent.Reset();
}

// 서버에서 스태미나 소비와 회복을 구독해 소비 후 지연 회복 정책을 시작할 수 있게 한다.
void UAbilityStateComponent::BindStaminaRegenToASC(UAbilitySystemComponent* AbilitySystemComponent)
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (!Character || !Character->HasAuthority())
	{
		return;
	}

	if (StaminaRegenAbilitySystemComponent.Get() == AbilitySystemComponent && StaminaChangedDelegateHandle.IsValid())
	{
		return;
	}

	UnbindStaminaRegenFromASC();
	if (!AbilitySystemComponent)
	{
		return;
	}

	StaminaRegenAbilitySystemComponent = AbilitySystemComponent;
	StaminaChangedDelegateHandle =
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute())
			.AddUObject(this, &ThisClass::HandleStaminaChanged);
}

// 스태미나 소비 시 회복 효과를 중단하고 1초 뒤 회복을 예약한다. 최대치에 도달하면 회복 효과와 대기 타이머를 제거한다.
void UAbilityStateComponent::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (!Character || !Character->HasAuthority() || FMath::IsNearlyEqual(Data.NewValue, Data.OldValue))
	{
		return;
	}

	if (Data.NewValue < Data.OldValue)
	{
		RemoveStaminaRegenEffects();
		const UGameSettingDefinition* SettingDefinition = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
		if (GetWorld() && SettingDefinition && SettingDefinition->StaminaRegenGameplayEffectClass)
		{
			GetWorld()->GetTimerManager().SetTimer(
				StaminaRegenDelayTimerHandle, this, &ThisClass::ApplyStaminaRegenEffect, StaminaRegenDelay, false);
		}
		return;
	}

	const float MaxStamina = GetCurrentMaxStamina();
	if (MaxStamina > 0.0f && Data.NewValue >= MaxStamina - KINDA_SMALL_NUMBER)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
		}
		RemoveStaminaRegenEffects();
	}
}

// 서버에서 스태미나가 최대치 미만이면 설정된 회복 GameplayEffect를 캐릭터를 출처로 적용한다.
void UAbilityStateComponent::ApplyStaminaRegenEffect()
{
	ACharacterBase* Character = GetCharacterOwner();
	const UGameSettingDefinition* SettingDefinition = UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	if (!Character || !Character->HasAuthority() || !SettingDefinition || !SettingDefinition->StaminaRegenGameplayEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* ASC = StaminaRegenAbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	const float MaxStamina = GetCurrentMaxStamina();
	const float CurrentStamina = ASC->GetNumericAttribute(UBasicAttributeSet::GetStaminaAttribute());
	if (MaxStamina > 0.0f && CurrentStamina >= MaxStamina - KINDA_SMALL_NUMBER)
	{
		RemoveStaminaRegenEffects();
		return;
	}

	RemoveStaminaRegenEffects();
	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(Character);
	FGameplayEffectSpecHandle SpecHandle =
		ASC->MakeOutgoingSpec(SettingDefinition->StaminaRegenGameplayEffectClass, StaminaRegenEffectLevel, EffectContext);
	if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

// 스태미나 소비·최대치 도달·회복 재적용 시 StaminaRegen 태그를 가진 기존 회복 효과를 제거한다.
void UAbilityStateComponent::RemoveStaminaRegenEffects()
{
	UAbilitySystemComponent* ASC = StaminaRegenAbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}

	FGameplayTagContainer RegenTags;
	RegenTags.AddTag(LabGameplayTags::Status_StaminaRegen);
	ASC->RemoveActiveEffectsWithGrantedTags(RegenTags);
}

// 회복 정책이 연결된 ASC의 최대 스태미나를 읽어 회복 종료 여부를 판단한다.
float UAbilityStateComponent::GetCurrentMaxStamina() const
{
	const UAbilitySystemComponent* ASC = StaminaRegenAbilitySystemComponent.Get();
	return ASC ? ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxStaminaAttribute()) : 0.0f;
}

// 이전 ASC의 스태미나 구독과 아직 실행되지 않은 회복 예약을 해제한다.
void UAbilityStateComponent::UnbindStaminaRegenFromASC()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
	}

	if (UAbilitySystemComponent* ASC = StaminaRegenAbilitySystemComponent.Get())
	{
		if (StaminaChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedDelegateHandle);
		}
	}
	StaminaChangedDelegateHandle.Reset();
	StaminaRegenAbilitySystemComponent.Reset();
}

// 이 컴포넌트를 소유한 캐릭터를 가져와 이동·회전·ASC·사망 상태에 접근한다.
ACharacterBase* UAbilityStateComponent::GetCharacterOwner() const
{
	return Cast<ACharacterBase>(GetOwner());
}
