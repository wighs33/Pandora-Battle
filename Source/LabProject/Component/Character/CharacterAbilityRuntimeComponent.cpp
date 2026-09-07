#include "Component/Character/CharacterAbilityRuntimeComponent.h"

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

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterAbilityRuntimeComponent)

namespace
{
	constexpr int32 ActorInfoMaxRetryAttempts = 10;
	constexpr float ActorInfoRetryInterval = 0.05f;
	constexpr float MinimumMaxWalkSpeed = 150.0f;
	constexpr int32 MovementSpeedAttributeMaxRetryAttempts = 50;
	constexpr float StaminaRegenDelay = 1.0f;
	constexpr float StaminaRegenEffectLevel = 1.0f;
}

UCharacterAbilityRuntimeComponent::UCharacterAbilityRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterAbilityRuntimeComponent::CaptureBaseMovementSpeed()
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (const UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr)
	{
		BaseMaxWalkSpeed = MovementComponent->MaxWalkSpeed;
	}
}

void UCharacterAbilityRuntimeComponent::InitializeAbilitySystemActorInfo()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			ActorInfoInitializationRetryTimerHandle);
	}
	bActorInfoInitializationQueued = false;
	ActorInfoInitializationRetryCount = 0;
	TryInitializeAbilitySystemActorInfo();
}

void UCharacterAbilityRuntimeComponent::TryInitializeAbilitySystemActorInfo()
{
	ACharacterBase* Character = GetCharacterOwner();
	UPdAbilitySystemComponent* PdASC =
		Character ? Character->GetPdAbilitySystemComponent() : nullptr;
	UAbilitySystemComponent* ASC = PdASC;
	AActor* OwnerActor =
		Character ? Character->GetAbilitySystemOwnerActor() : nullptr;
	AActor* AvatarActor =
		Character ? Character->GetAbilitySystemAvatarActor() : nullptr;
	if (BoundAbilitySystemComponent.IsValid() && BoundAbilitySystemComponent.Get() != ASC)
	{
		ClearAbilitySystemActorInfo();
	}
	// 이전 Pawn의 늦은 초기화 요청으로 PlayerState의 새 Avatar를 빼앗지 않는다.
	if (const APdPlayerState* PlayerState = Cast<APdPlayerState>(OwnerActor);
		PlayerState && PlayerState->GetPawn() != AvatarActor)
	{
		ClearAbilitySystemActorInfo();
		return;
	}
	if (!Character || !ASC || !OwnerActor || !AvatarActor)
	{
		QueueAbilitySystemActorInfoInitializationRetry();
		if (Character && Character->GetCharacterHealthBarComponent())
		{
			Character->GetCharacterHealthBarComponent()->TryRefreshViewModel();
		}
		return;
	}

	if (!PdASC->IsRegistered() || !PdASC->HasAbilityActorInfoAllocated())
	{
		QueueAbilitySystemActorInfoInitializationRetry();
		if (Character->GetCharacterHealthBarComponent())
		{
			Character->GetCharacterHealthBarComponent()->TryRefreshViewModel();
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			ActorInfoInitializationRetryTimerHandle);
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
		PdASC->GrantAbilities({ UReactiveRecoveryAbility::StaticClass() }, 1, OwnerActor);
		if (const FGameplayAbilitySpec* RecoverySpec = PdASC->FindAbilitySpecFromClass(UReactiveRecoveryAbility::StaticClass());
			RecoverySpec && !RecoverySpec->IsActive())
		{
			PdASC->TryActivateAbility(RecoverySpec->Handle);
		}
	}
	BindStaminaRegenToASC(ASC);
	BindMovementSpeedAttributeToASC(ASC);
	BindDeadTagEvent(ASC);
	BindFrozenTagEvent(ASC);
	RefreshAirborneGameplayTag();

	if (UEquipmentComponent* EquipmentComponent =
		Character->GetEquipmentComponent())
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

void UCharacterAbilityRuntimeComponent::
QueueAbilitySystemActorInfoInitializationRetry()
{
	UWorld* World = GetWorld();
	if (bActorInfoInitializationQueued || !World)
	{
		return;
	}

	if (ActorInfoInitializationRetryCount
		>= ActorInfoMaxRetryAttempts)
	{
		return;
	}

	bActorInfoInitializationQueued = true;
	++ActorInfoInitializationRetryCount;
	World->GetTimerManager().SetTimer(
		ActorInfoInitializationRetryTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bActorInfoInitializationQueued = false;
			ActorInfoInitializationRetryTimerHandle.Invalidate();
			TryInitializeAbilitySystemActorInfo();
		}),
		ActorInfoRetryInterval,
		false);
}

// 자신의 Avatar 연결과 이동 상태만 정리한다. 새 Pawn의 ASC 상태나 판도라 능력·쿨다운은 건드리지 않는다.
void UCharacterAbilityRuntimeComponent::ClearAbilitySystemActorInfo()
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
		World->GetTimerManager().ClearTimer(MovementSpeedAttributeRetryTimerHandle);
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

void UCharacterAbilityRuntimeComponent::ShutdownRuntime()
{
	ClearAbilitySystemActorInfo();
}

void UCharacterAbilityRuntimeComponent::HandleMovementModeChanged()
{
	RefreshAirborneGameplayTag();
}

void UCharacterAbilityRuntimeComponent::TickRuntime()
{
	if (bFrozenMovementActive)
	{
		MaintainFrozenRotationLock();
	}
}

void UCharacterAbilityRuntimeComponent::BindStaminaRegenToASC(
	UAbilitySystemComponent* AbilitySystemComponent)
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character || !Character->HasAuthority())
	{
		return;
	}

	if (StaminaRegenASC.Get() == AbilitySystemComponent
		&& StaminaChangedDelegateHandle.IsValid())
	{
		return;
	}

	UnbindStaminaRegenFromASC();
	if (!AbilitySystemComponent)
	{
		return;
	}

	StaminaRegenASC = AbilitySystemComponent;
	StaminaChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(
			UBasicAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleStaminaChanged);
}

void UCharacterAbilityRuntimeComponent::UnbindStaminaRegenFromASC()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
	}

	if (UAbilitySystemComponent* ASC = StaminaRegenASC.Get())
	{
		if (StaminaChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UBasicAttributeSet::GetStaminaAttribute())
				.Remove(StaminaChangedDelegateHandle);
		}
	}
	StaminaChangedDelegateHandle.Reset();
	StaminaRegenASC.Reset();
}

void UCharacterAbilityRuntimeComponent::BindMovementSpeedAttributeToASC(
	UAbilitySystemComponent* AbilitySystemComponent)
{
	if (MovementSpeedAttributeASC.Get() == AbilitySystemComponent
		&& MovementSpeedAttributeChangedDelegateHandle.IsValid()
		&& MovementStaminaAttributeChangedDelegateHandle.IsValid()
		&& MovementMaxStaminaAttributeChangedDelegateHandle.IsValid())
	{
		ApplyMovementSpeedFromAttribute();
		return;
	}

	UnbindMovementSpeedAttribute();
	if (!AbilitySystemComponent)
	{
		return;
	}

	MovementSpeedAttributeASC = AbilitySystemComponent;
	MovementSpeedAttributeRetryAttempts = 0;
	MovementSpeedAttributeChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(
			UBasicAttributeSet::GetMovementSpeedAttribute())
		.AddUObject(
			this,
			&ThisClass::HandleMovementSpeedAttributeChanged);
	MovementStaminaAttributeChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(
			UBasicAttributeSet::GetStaminaAttribute())
		.AddUObject(
			this,
			&ThisClass::HandleMovementStaminaAttributeChanged);
	MovementMaxStaminaAttributeChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(
			UBasicAttributeSet::GetMaxStaminaAttribute())
		.AddUObject(
			this,
			&ThisClass::HandleMovementStaminaAttributeChanged);
	ApplyMovementSpeedFromAttribute();
}

void UCharacterAbilityRuntimeComponent::UnbindMovementSpeedAttribute()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			MovementSpeedAttributeRetryTimerHandle);
	}

	if (UAbilitySystemComponent* ASC = MovementSpeedAttributeASC.Get())
	{
		if (MovementSpeedAttributeChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UBasicAttributeSet::GetMovementSpeedAttribute())
				.Remove(MovementSpeedAttributeChangedDelegateHandle);
		}
		if (MovementStaminaAttributeChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UBasicAttributeSet::GetStaminaAttribute())
				.Remove(MovementStaminaAttributeChangedDelegateHandle);
		}
		if (MovementMaxStaminaAttributeChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UBasicAttributeSet::GetMaxStaminaAttribute())
				.Remove(MovementMaxStaminaAttributeChangedDelegateHandle);
		}
	}

	MovementSpeedAttributeChangedDelegateHandle.Reset();
	MovementStaminaAttributeChangedDelegateHandle.Reset();
	MovementMaxStaminaAttributeChangedDelegateHandle.Reset();
	MovementSpeedAttributeASC.Reset();
	MovementSpeedAttributeRetryAttempts = 0;
}

void UCharacterAbilityRuntimeComponent::HandleMovementSpeedAttributeChanged(
	const FOnAttributeChangeData& Data)
{
	if (!FMath::IsNearlyEqual(Data.NewValue, Data.OldValue))
	{
		ApplyMovementSpeedFromAttribute();
	}
}

void UCharacterAbilityRuntimeComponent::HandleMovementStaminaAttributeChanged(
	const FOnAttributeChangeData& Data)
{
	if (!FMath::IsNearlyEqual(Data.NewValue, Data.OldValue))
	{
		ApplyMovementSpeedFromAttribute();
	}
}

void UCharacterAbilityRuntimeComponent::RefreshLowStaminaEffectComponent(
	const float StaminaPercent,
	const UGameSettingDefinition* SettingDefinition)
{
	if (!SettingDefinition)
	{
		return;
	}

	UActorComponent* EffectComponent =
		ResolveLowStaminaEffectComponent(
			SettingDefinition->LowStaminaEffectComponentName);
	if (!EffectComponent)
	{
		return;
	}

	const bool bShouldBeActive = StaminaPercent <= FMath::Clamp(
		SettingDefinition->LowStaminaThresholdPercent,
		0.0f,
		100.0f);
	if (EffectComponent->IsActive() != bShouldBeActive)
	{
		EffectComponent->SetActive(bShouldBeActive, true);
	}
}

UActorComponent*
UCharacterAbilityRuntimeComponent::ResolveLowStaminaEffectComponent(
	const FName ComponentName)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (ComponentName.IsNone() || !Character)
	{
		LowStaminaEffectComponent.Reset();
		CachedLowStaminaEffectComponentName = NAME_None;
		return nullptr;
	}

	if (CachedLowStaminaEffectComponentName == ComponentName
		&& LowStaminaEffectComponent.IsValid())
	{
		return LowStaminaEffectComponent.Get();
	}

	LowStaminaEffectComponent.Reset();
	CachedLowStaminaEffectComponentName = ComponentName;
	TArray<UActorComponent*> ActorComponents;
	Character->GetComponents<UActorComponent>(ActorComponents);
	for (UActorComponent* ActorComponent : ActorComponents)
	{
		if (ActorComponent
			&& (ActorComponent->GetFName() == ComponentName
				|| ActorComponent->ComponentHasTag(ComponentName)))
		{
			LowStaminaEffectComponent = ActorComponent;
			return ActorComponent;
		}
	}
	return nullptr;
}

void UCharacterAbilityRuntimeComponent::ApplyMovementSpeedFromAttribute()
{
	ACharacterBase* Character = GetCharacterOwner();
	UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr;
	UAbilitySystemComponent* ASC = MovementSpeedAttributeASC.Get();
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
		World->GetTimerManager().ClearTimer(
			MovementSpeedAttributeRetryTimerHandle);
	}
	MovementSpeedAttributeRetryAttempts = 0;

	const float CurrentStamina = ASC->GetNumericAttribute(
		UBasicAttributeSet::GetStaminaAttribute());
	const float CurrentMaxStamina = ASC->GetNumericAttribute(
		UBasicAttributeSet::GetMaxStaminaAttribute());
	const float StaminaRatio = CurrentMaxStamina > UE_SMALL_NUMBER
		? FMath::Clamp(CurrentStamina / CurrentMaxStamina, 0.0f, 1.0f)
		: 1.0f;
	const float StaminaPercent = StaminaRatio * 100.0f;

	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
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

	const float MovementSpeedAttribute = ASC->GetNumericAttribute(
		UBasicAttributeSet::GetMovementSpeedAttribute());
	const float NormalMaxWalkSpeed = FMath::Max(
		MinimumMaxWalkSpeed,
		BaseMaxWalkSpeed
			* (1.0f + FMath::Max(MovementSpeedAttribute, 0.0f) * 0.01f));
	const UEquipmentComponent* EquipmentComponent =
		Character->GetEquipmentComponent();
	const UItemDefinition* EquippedWeaponDefinition = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponDefinition()
		: nullptr;
	const float EquippedWeaponMovementSpeedMultiplier =
		EquippedWeaponDefinition
			? EquippedWeaponDefinition->GetEquippedMovementSpeedMultiplier()
			: 1.0f;

	float StaminaSpeedMultiplier = 1.0f;
	if (StaminaPercent <= FMath::Clamp(
		SettingDefinition->CriticalStaminaThresholdPercent,
		0.0f,
		100.0f))
	{
		StaminaSpeedMultiplier = 1.0f
			- FMath::Clamp(
				SettingDefinition->CriticalStaminaMovementSpeedReductionPercent,
				0.0f,
				100.0f) * 0.01f;
	}
	else if (StaminaPercent <= FMath::Clamp(
		SettingDefinition->LowStaminaThresholdPercent,
		0.0f,
		100.0f))
	{
		StaminaSpeedMultiplier = 1.0f
			- FMath::Clamp(
				SettingDefinition->LowStaminaMovementSpeedReductionPercent,
				0.0f,
				100.0f) * 0.01f;
	}

	const float NewMaxWalkSpeed =
		NormalMaxWalkSpeed
			* EquippedWeaponMovementSpeedMultiplier
			* StaminaSpeedMultiplier;
	if (!FMath::IsNearlyEqual(
		MovementComponent->MaxWalkSpeed,
		NewMaxWalkSpeed))
	{
		MovementComponent->MaxWalkSpeed = NewMaxWalkSpeed;
	}
}

void UCharacterAbilityRuntimeComponent::
QueueMovementSpeedAttributeApplyRetry()
{
	UWorld* World = GetWorld();
	if (!World
		|| World->GetTimerManager().IsTimerActive(
			MovementSpeedAttributeRetryTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		MovementSpeedAttributeRetryTimerHandle,
		this,
		&ThisClass::RetryApplyMovementSpeedFromAttribute,
		0.1f,
		true);
}

void UCharacterAbilityRuntimeComponent::RetryApplyMovementSpeedFromAttribute()
{
	++MovementSpeedAttributeRetryAttempts;
	if (MovementSpeedAttributeRetryAttempts
		> MovementSpeedAttributeMaxRetryAttempts)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(
				MovementSpeedAttributeRetryTimerHandle);
		}
		return;
	}
	ApplyMovementSpeedFromAttribute();
}

void UCharacterAbilityRuntimeComponent::BindDeadTagEvent(
	UAbilitySystemComponent* AbilitySystemComponent)
{
	if (DeadTagBoundAbilitySystemComponent.Get() == AbilitySystemComponent
		&& DeadTagChangedDelegateHandle.IsValid())
	{
		return;
	}

	UnbindDeadTagEvent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	DeadTagBoundAbilitySystemComponent = AbilitySystemComponent;
	DeadTagChangedDelegateHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(
			LabGameplayTags::State_Dead,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::OnDeadTagChanged);

	const int32 CurrentDeadTagCount =
		AbilitySystemComponent->GetTagCount(LabGameplayTags::State_Dead);
	if (CurrentDeadTagCount > 0)
	{
		OnDeadTagChanged(
			LabGameplayTags::State_Dead,
			CurrentDeadTagCount);
	}
}

void UCharacterAbilityRuntimeComponent::UnbindDeadTagEvent()
{
	if (UAbilitySystemComponent* ASC =
		DeadTagBoundAbilitySystemComponent.Get())
	{
		if (DeadTagChangedDelegateHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(
				LabGameplayTags::State_Dead,
				EGameplayTagEventType::NewOrRemoved)
				.Remove(DeadTagChangedDelegateHandle);
		}
	}
	DeadTagChangedDelegateHandle.Reset();
	DeadTagBoundAbilitySystemComponent.Reset();
}

void UCharacterAbilityRuntimeComponent::OnDeadTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);
	ACharacterBase* Character = GetCharacterOwner();
	UCharacterDeathComponent* DeathComponent =
		Character ? Character->GetCharacterDeathComponent() : nullptr;
	if (DeathComponent)
	{
		DeathComponent->HandleDeadTagChanged(
			NewCount,
			DeadTagBoundAbilitySystemComponent.Get());
	}
}

void UCharacterAbilityRuntimeComponent::BindFrozenTagEvent(
	UAbilitySystemComponent* AbilitySystemComponent)
{
	if (FrozenTagBoundAbilitySystemComponent.Get() == AbilitySystemComponent
		&& FrozenTagChangedDelegateHandle.IsValid())
	{
		OnFrozenTagChanged(
			LabGameplayTags::Status_Frostbite,
			AbilitySystemComponent
				? AbilitySystemComponent->GetTagCount(
					LabGameplayTags::Status_Frostbite)
				: 0);
		return;
	}

	UnbindFrozenTagEvent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	FrozenTagBoundAbilitySystemComponent = AbilitySystemComponent;
	FrozenTagChangedDelegateHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(
			LabGameplayTags::Status_Frostbite,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::OnFrozenTagChanged);
	OnFrozenTagChanged(
		LabGameplayTags::Status_Frostbite,
		AbilitySystemComponent->GetTagCount(
			LabGameplayTags::Status_Frostbite));
}

void UCharacterAbilityRuntimeComponent::UnbindFrozenTagEvent()
{
	if (bFrozenMovementActive)
	{
		OnFrozenTagChanged(LabGameplayTags::Status_Frostbite, 0);
	}

	if (UAbilitySystemComponent* ASC =
		FrozenTagBoundAbilitySystemComponent.Get())
	{
		if (FrozenTagChangedDelegateHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(
				LabGameplayTags::Status_Frostbite,
				EGameplayTagEventType::NewOrRemoved)
				.Remove(FrozenTagChangedDelegateHandle);
		}
	}
	FrozenTagChangedDelegateHandle.Reset();
	FrozenTagBoundAbilitySystemComponent.Reset();
}

void UCharacterAbilityRuntimeComponent::RefreshAirborneGameplayTag()
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	const UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr;
	SetAirborneGameplayTag(
		MovementComponent && MovementComponent->IsFalling());
}

void UCharacterAbilityRuntimeComponent::SetAirborneGameplayTag(
	const bool bAirborne)
{
	ACharacterBase* Character = GetCharacterOwner();
	UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get();
	if (!Character
		|| !ASC
		|| ASC->GetAvatarActor() != Character
		|| (!Character->HasAuthority() && !Character->IsLocallyControlled()))
	{
		return;
	}

	const EGameplayTagReplicationState ReplicationState =
		Character->HasAuthority()
			? EGameplayTagReplicationState::CountToOwner
			: EGameplayTagReplicationState::None;
	ASC->SetLooseGameplayTagCount(
		LabGameplayTags::State_Movement_Airborne,
		bAirborne ? 1 : 0,
		ReplicationState);

	if (!bAirborne || !Character->HasAuthority())
	{
		return;
	}

	FGameplayTagContainer AttackAbilityTags;
	AttackAbilityTags.AddTag(LabGameplayTags::Action_Attack);
	AttackAbilityTags.AddTag(LabGameplayTags::Action_Punch);
	AttackAbilityTags.AddTag(LabGameplayTags::Action_RangedAttack);
	ASC->CancelAbilities(&AttackAbilityTags);
}

void UCharacterAbilityRuntimeComponent::OnFrozenTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);
	ACharacterBase* Character = GetCharacterOwner();
	UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !MovementComponent)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	const bool bIsDead =
		ASC && ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead);
	if (NewCount > 0)
	{
		if (bIsDead)
		{
			return;
		}

		if (UCombatComponent* CombatComponent =
			Character->GetCombatComponent())
		{
			CombatComponent->StopPrimaryAttack();
			CombatComponent->StopAim();
		}

		if (ASC)
		{
			FGameplayTagContainer GrappleAbilityTags;
			GrappleAbilityTags.AddTag(
				LabGameplayTags::GameplayAbility_Movement_Grapple);
			ASC->CancelAbilities(&GrappleAbilityTags);
		}

		const bool bWasAlreadyFrozen = bFrozenMovementActive;
		if (AAIController* AIController =
			Cast<AAIController>(Character->GetController()))
		{
			AIController->StopMovement();
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}

		if (!bWasAlreadyFrozen)
		{
			bFrozenRotationStateCached = true;
			FrozenLockedActorRotation = Character->GetActorRotation();
			FrozenLockedControlRotation = Character->GetController()
				? Character->GetController()->GetControlRotation()
				: FrozenLockedActorRotation;
			FrozenLockedMeshRelativeRotation = Character->GetMesh()
				? Character->GetMesh()->GetRelativeRotation()
				: FRotator::ZeroRotator;
			FrozenLockedAimYaw =
				Character->GetAimYawForAnimation();
			FrozenLockedAimPitch =
				Character->GetAimPitchForAnimation();
			bFrozenCachedOrientRotationToMovement =
				MovementComponent->bOrientRotationToMovement;
			bFrozenCachedUseControllerDesiredRotation =
				MovementComponent->bUseControllerDesiredRotation;
			bFrozenCachedUseControllerRotationYaw =
				Character->bUseControllerRotationYaw;
			FrozenCachedRotationRate = MovementComponent->RotationRate;
			bFrozenMovementActive = true;
			Character->RefreshCharacterTickEnabled();
		}

		MovementComponent->StopMovementImmediately();
		MovementComponent->MaxWalkSpeed = 0.0f;
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->bUseControllerDesiredRotation = false;
		MovementComponent->RotationRate = FRotator::ZeroRotator;
		Character->bUseControllerRotationYaw = false;

		if (AController* Controller = Character->GetController();
			Controller && !FrozenInputController.IsValid())
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

void UCharacterAbilityRuntimeComponent::MaintainFrozenRotationLock()
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character
		|| !bFrozenMovementActive
		|| !bFrozenRotationStateCached)
	{
		return;
	}

	const UAbilitySystemComponent* ASC =
		Character->GetAbilitySystemComponent();
	if (ASC && ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
	{
		return;
	}

	const FRotator CurrentActorRotation = Character->GetActorRotation();
	const FRotator CurrentControlRotation = Character->GetController()
		? Character->GetController()->GetControlRotation()
		: FRotator::ZeroRotator;
	USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	const bool bMeshSimulatingPhysics =
		CharacterMesh && CharacterMesh->IsSimulatingPhysics();
	const FRotator CurrentMeshRelativeRotation = CharacterMesh
		? CharacterMesh->GetRelativeRotation()
		: FRotator::ZeroRotator;

	if (UCharacterMovementComponent* MovementComponent =
		Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->MaxWalkSpeed = 0.0f;
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->bUseControllerDesiredRotation = false;
		MovementComponent->RotationRate = FRotator::ZeroRotator;
	}
	Character->bUseControllerRotationYaw = false;
	Character->SetAimOffsetForAnimation(
		FrozenLockedAimYaw,
		FrozenLockedAimPitch);

	if (!CurrentActorRotation.Equals(FrozenLockedActorRotation, 0.01f))
	{
		Character->SetActorRotation(
			FrozenLockedActorRotation,
			ETeleportType::TeleportPhysics);
	}

	if (AController* Controller = Character->GetController();
		Controller
			&& !CurrentControlRotation.Equals(
				FrozenLockedControlRotation,
				0.01f))
	{
		Controller->SetControlRotation(FrozenLockedControlRotation);
	}

	if (CharacterMesh
		&& !bMeshSimulatingPhysics
		&& !CurrentMeshRelativeRotation.Equals(
			FrozenLockedMeshRelativeRotation,
			0.01f))
	{
		CharacterMesh->SetRelativeRotation(
			FrozenLockedMeshRelativeRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
}

void UCharacterAbilityRuntimeComponent::RestoreCachedRotationSettings(
	UCharacterMovementComponent* MovementComponent) const
{
	ACharacterBase* Character =
		const_cast<UCharacterAbilityRuntimeComponent*>(this)
			->GetCharacterOwner();
	if (!Character || !MovementComponent)
	{
		return;
	}

	MovementComponent->bOrientRotationToMovement =
		bFrozenCachedOrientRotationToMovement;
	MovementComponent->bUseControllerDesiredRotation =
		bFrozenCachedUseControllerDesiredRotation;
	MovementComponent->RotationRate = FrozenCachedRotationRate;
	Character->bUseControllerRotationYaw =
		bFrozenCachedUseControllerRotationYaw;
}

void UCharacterAbilityRuntimeComponent::ClearFrozenStateForRespawn()
{
	OnFrozenTagChanged(LabGameplayTags::Status_Frostbite, 0);
}

void UCharacterAbilityRuntimeComponent::HandleStaminaChanged(
	const FOnAttributeChangeData& Data)
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character
		|| !Character->HasAuthority()
		|| FMath::IsNearlyEqual(Data.NewValue, Data.OldValue))
	{
		return;
	}

	if (Data.NewValue < Data.OldValue)
	{
		RemoveStaminaRegenEffects();
		const UGameSettingDefinition* SettingDefinition =
			UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
		if (GetWorld()
			&& SettingDefinition
			&& SettingDefinition->StaminaRegenGameplayEffectClass)
		{
			GetWorld()->GetTimerManager().SetTimer(
				StaminaRegenDelayTimerHandle,
				this,
				&ThisClass::ApplyStaminaRegenEffect,
				StaminaRegenDelay,
				false);
		}
		return;
	}

	const float MaxStamina = GetCurrentMaxStamina();
	if (MaxStamina > 0.0f
		&& Data.NewValue >= MaxStamina - KINDA_SMALL_NUMBER)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(
				StaminaRegenDelayTimerHandle);
		}
		RemoveStaminaRegenEffects();
	}
}

void UCharacterAbilityRuntimeComponent::ApplyStaminaRegenEffect()
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	if (!Character
		|| !Character->HasAuthority()
		|| !SettingDefinition
		|| !SettingDefinition->StaminaRegenGameplayEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* ASC = StaminaRegenASC.Get();
	if (!ASC)
	{
		return;
	}

	const float MaxStamina = GetCurrentMaxStamina();
	const float CurrentStamina = ASC->GetNumericAttribute(
		UBasicAttributeSet::GetStaminaAttribute());
	if (MaxStamina > 0.0f
		&& CurrentStamina >= MaxStamina - KINDA_SMALL_NUMBER)
	{
		RemoveStaminaRegenEffects();
		return;
	}

	RemoveStaminaRegenEffects();
	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(
		const_cast<ACharacterBase*>(Character));
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
		SettingDefinition->StaminaRegenGameplayEffectClass,
		StaminaRegenEffectLevel,
		EffectContext);
	if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void UCharacterAbilityRuntimeComponent::RemoveStaminaRegenEffects()
{
	UAbilitySystemComponent* ASC = StaminaRegenASC.Get();
	if (!ASC)
	{
		return;
	}

	FGameplayTagContainer RegenTags;
	RegenTags.AddTag(LabGameplayTags::Status_StaminaRegen);
	ASC->RemoveActiveEffectsWithGrantedTags(RegenTags);
}

float UCharacterAbilityRuntimeComponent::GetCurrentMaxStamina() const
{
	const UAbilitySystemComponent* ASC = StaminaRegenASC.Get();
	return ASC
		? ASC->GetNumericAttribute(
			UBasicAttributeSet::GetMaxStaminaAttribute())
		: 0.0f;
}

ACharacterBase* UCharacterAbilityRuntimeComponent::GetCharacterOwner() const
{
	return Cast<ACharacterBase>(GetOwner());
}

const ACharacterBase*
UCharacterAbilityRuntimeComponent::GetCharacterOwnerConst() const
{
	return Cast<ACharacterBase>(GetOwner());
}
