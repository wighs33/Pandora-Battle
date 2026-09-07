#include "AbilitySystem/Ability/AttackAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Character/EnemyBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(AttackAbility)

namespace
{
constexpr double LateComboInputGraceSeconds = 0.15;

float CalculateWeaponAttackSpeedPlayRate(const FGameplayAbilityActorInfo* ActorInfo)
{
	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;

	const float AttackSpeedPercent = AttributeSet ? FMath::Max(AttributeSet->GetAttackSpeed(), 0.0f) : 0.0f;
	return FMath::Max(0.01f, 1.0f + AttackSpeedPercent * 0.01f);
}
}

UAttackAbility::UAttackAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::Action_Attack);
	SetAssetTags(AbilityAssetTags);
	ActivationBlockedTags.AddTag(LabGameplayTags::State_Movement_Airborne);

	AttackInputWindowStartEventTag = LabGameplayTags::Notifier_Attack_ComboInputOpen;
	AttackInputWindowEndEventTag = LabGameplayTags::Notifier_Attack_ComboInputClose;
	AttackDamageWindowStartEventTag = LabGameplayTags::Notifier_Attack_DamageWindowOpen;
	AttackDamageWindowEndEventTag = LabGameplayTags::Notifier_Attack_DamageWindowClose;
	JumpSectionEventTag = LabGameplayTags::Notifier_Attack_NextCombo;
}

bool UAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const ACharacterBase* Character = ActorInfo
		? Cast<ACharacterBase>(ActorInfo->AvatarActor.Get())
		: nullptr;
	const UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr;
	if (MovementComponent && MovementComponent->IsFalling())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(LabGameplayTags::State_Movement_Airborne);
		}
		return false;
	}

	return Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags);
}

// State helpers
void UAttackAbility::CleanupAttackState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackDamageWindowCloseTimerHandle);
	}
	AttackDamageWindowCloseTimerHandle.Invalidate();

	if (WaitInputPressTask)
	{
		WaitInputPressTask->EndTask();
		WaitInputPressTask = nullptr;
	}

	SetCurrentWeaponBeginOverlapEnabled(false);
	SetCurrentComboDamageMultiplier(1.0f);
	bAttackDamageWindowActive = false;
	ActiveAttackDamageWindowSectionName = NAME_None;
	ResetAttackInputState();
	bComboInputConsumedForCurrentWindow = false;
	ComboInputWindowSectionName = NAME_None;
	bRestartAttackAfterMontage = false;

	if (AttackingEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(AttackingEffectClass);
	}
}

// Timing callbacks
void UAttackAbility::OnAttackMontageCompleted()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const bool bRecentInputWindow = bCanReceiveAttackInput
		|| (LastUnconsumedComboWindowCloseTime >= 0.0 && Now - LastUnconsumedComboWindowCloseTime <= LateComboInputGraceSeconds);
	LateComboInputExpiresAt = !bComboInputConsumedForCurrentWindow && bRecentInputWindow ? Now + LateComboInputGraceSeconds : -1.0;
	const bool bShouldRestartAttack = bRestartAttackAfterMontage;
	const bool bCanRestartOnThisMachine = HasAuthority(&CurrentActivationInfo);

	if (bShouldRestartAttack && bCanRestartOnThisMachine)
	{
		RestartAttackAfterMontage();
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAttackAbility::OnAttackMontageInterrupted()
{
	LateComboInputExpiresAt = -1.0;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAttackAbility::OnAttackMontageCancelled()
{
	LateComboInputExpiresAt = -1.0;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAttackAbility::OnJumpSectionTiming(FGameplayEventData Payload)
{
	if (Payload.EventMagnitude > 0.f)
	{
		RequestNextComboInput();
		return;
	}

	bReachedJumpSectionTiming = true;
	if (!BufferedJumpSectionName.IsNone())
	{
		TryJumpToSection(BufferedJumpSectionName);
	}
}

void UAttackAbility::OnComboInputWindowOpened(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	FaceCurrentAttackTarget(TEXT("InputWindowOpened"));
	const FName CurrentSectionName = GetCurrentAttackSectionName();
	if (CurrentActorInfo
		&& CurrentActorInfo->IsLocallyControlled()
		&& HasPlayerController()
		&& !CurrentSectionName.IsNone()
		&& LastComboWindowEffectSectionName != CurrentSectionName)
	{
		LastComboWindowEffectSectionName = CurrentSectionName;
		PlayConfiguredComboWindowStartEffect();
	}

	const bool bEnteredNewSection = ComboInputWindowSectionName != CurrentSectionName;
	if (bEnteredNewSection)
	{
		ComboInputWindowSectionName = CurrentSectionName;
		bComboInputConsumedForCurrentWindow = false;
		bReachedJumpSectionTiming = false;
		BufferedJumpSectionName = NAME_None;
		QueuedFromSectionName = NAME_None;
		bBufferedComboCostCommitted = false;
	}

	bCanReceiveAttackInput = !bComboInputConsumedForCurrentWindow;
	if (bCanReceiveAttackInput)
	{
		WaitForContinueInput();
	}

	if (bCanReceiveAttackInput && bAIAlwaysContinueCombo && !HasPlayerController())
	{
		bComboInputConsumedForCurrentWindow = true;
		RequestJumpToSection(GetNextAttackSectionName());
		bCanReceiveAttackInput = false;
	}
}

void UAttackAbility::OnComboInputWindowClosed(FGameplayEventData Payload)
{
	static_cast<void>(Payload);
	LastUnconsumedComboWindowCloseTime = bCanReceiveAttackInput && !bComboInputConsumedForCurrentWindow && GetWorld()
		? GetWorld()->GetTimeSeconds() : -1.0;

	if (!BufferedJumpSectionName.IsNone() && !bReachedJumpSectionTiming)
	{
		// The transition is normally registered as soon as the input succeeds.
		// Keep this as a recovery path for any externally supplied section request.
		if (!bBufferedComboCostCommitted)
		{
			QueueBufferedComboTransition();
		}
		return;
	}

	ResetAttackInputState();
}

void UAttackAbility::OnAttackDamageWindowOpened(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	const FName CurrentSectionName = GetCurrentAttackSectionName();
	if (CurrentSectionName.IsNone())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackDamageWindowCloseTimerHandle);
	}
	AttackDamageWindowCloseTimerHandle.Invalidate();

	if (bAttackDamageWindowActive && ActiveAttackDamageWindowSectionName == CurrentSectionName)
	{
		return;
	}

	bAttackDamageWindowActive = true;
	ActiveAttackDamageWindowSectionName = CurrentSectionName;
	const float ComboDamageMultiplier = CalculateCurrentComboDamageMultiplier();
	SetCurrentComboDamageMultiplier(ComboDamageMultiplier);
	SetCurrentWeaponBeginOverlapEnabled(true, CurrentSectionName);
}

void UAttackAbility::OnAttackDamageWindowClosed(FGameplayEventData Payload)
{
	static_cast<void>(Payload);

	if (!bAttackDamageWindowActive)
	{
		return;
	}

	const float CloseGraceSeconds = FMath::Clamp(AttackDamageWindowCloseGraceSeconds, 0.0f, 0.15f);
	UWorld* World = GetWorld();
	if (World && CloseGraceSeconds > UE_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(
			AttackDamageWindowCloseTimerHandle,
			this,
			&ThisClass::FinalizeAttackDamageWindowClose,
			CloseGraceSeconds,
			false);
		return;
	}

	FinalizeAttackDamageWindowClose();
}

void UAttackAbility::FinalizeAttackDamageWindowClose()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackDamageWindowCloseTimerHandle);
	}
	AttackDamageWindowCloseTimerHandle.Invalidate();

	if (!bAttackDamageWindowActive)
	{
		return;
	}

	bAttackDamageWindowActive = false;
	ActiveAttackDamageWindowSectionName = NAME_None;
	SetCurrentWeaponBeginOverlapEnabled(false);
	SetCurrentComboDamageMultiplier(1.0f);
}

void UAttackAbility::OnContinueInputPressed(float TimeWaited)
{
	static_cast<void>(TimeWaited);
	WaitInputPressTask = nullptr;
	RequestNextComboInput();
	if (!bComboInputConsumedForCurrentWindow)
	{
		WaitForContinueInput();
	}
}

// Ability flow
void UAttackAbility::OnAbilityEnding()
{
	Super::OnAbilityEnding();
	CleanupAttackState();
}

void UAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	static_cast<void>(TriggerEventData);
	LateComboInputExpiresAt = -1.0;
	LastUnconsumedComboWindowCloseTime = -1.0;

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!Character)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	FAttackData AttackData;
	UEquipmentComponent* EquipmentComponent = Character->GetEquipmentComponent();
	const bool bHasEquippedWeapon = EquipmentComponent && EquipmentComponent->GetCurrentWeaponDefinition() != nullptr;
	bool bHasAttackData = bHasEquippedWeapon && EquipmentComponent->GetAttackData(AttackData);
	if (!bHasAttackData && !bHasEquippedWeapon)
	{
		if (UCombatComponent* CombatComponent = Character->GetCombatComponent())
		{
			bHasAttackData = CombatComponent->GetUnarmedAttackData(AttackData);
		}
	}
	if (!bHasAttackData)
	{
		if (const AEnemyBase* Enemy = Cast<AEnemyBase>(Character))
		{
			bHasAttackData = Enemy->GetFallbackAttackData(AttackData);
		}
	}

	if (!bHasAttackData || !AttackData.AttackMontage)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ResetAttackDamageHitTracking();
	ResetAttackInputState();
	bComboInputConsumedForCurrentWindow = false;
	ComboInputWindowSectionName = NAME_None;
	bAttackDamageWindowActive = false;
	ActiveAttackDamageWindowSectionName = NAME_None;
	bRestartAttackAfterMontage = false;
	LastComboWindowEffectSectionName = NAME_None;
	WaitForContinueInput();

	if (AttackInputWindowStartEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackInputWindowStartedEventTask =
			CreateWaitGameplayEventTask(AttackInputWindowStartEventTag);
		if (ensure(AttackInputWindowStartedEventTask))
		{
			AttackInputWindowStartedEventTask->EventReceived.AddDynamic(this, &UAttackAbility::OnComboInputWindowOpened);
			AttackInputWindowStartedEventTask->ReadyForActivation();
		}
	}

	if (AttackInputWindowEndEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackInputWindowEndedEventTask =
			CreateWaitGameplayEventTask(AttackInputWindowEndEventTag);
		if (ensure(AttackInputWindowEndedEventTask))
		{
			AttackInputWindowEndedEventTask->EventReceived.AddDynamic(this, &UAttackAbility::OnComboInputWindowClosed);
			AttackInputWindowEndedEventTask->ReadyForActivation();
		}
	}

	if (AttackDamageWindowStartEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackDamageWindowStartedEventTask =
			CreateWaitGameplayEventTask(AttackDamageWindowStartEventTag);
		if (ensure(AttackDamageWindowStartedEventTask))
		{
			AttackDamageWindowStartedEventTask->EventReceived.AddDynamic(
				this,
				&UAttackAbility::OnAttackDamageWindowOpened);
			AttackDamageWindowStartedEventTask->ReadyForActivation();
		}
	}

	if (AttackDamageWindowEndEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* AttackDamageWindowEndedEventTask =
			CreateWaitGameplayEventTask(AttackDamageWindowEndEventTag);
		if (ensure(AttackDamageWindowEndedEventTask))
		{
			AttackDamageWindowEndedEventTask->EventReceived.AddDynamic(
				this,
				&UAttackAbility::OnAttackDamageWindowClosed);
			AttackDamageWindowEndedEventTask->ReadyForActivation();
		}
	}

	if (JumpSectionEventTag.IsValid())
	{
		UAbilityTask_WaitGameplayEvent* JumpSectionEventTask =
			CreateWaitGameplayEventTask(JumpSectionEventTag);
		if (ensure(JumpSectionEventTask))
		{
			JumpSectionEventTask->EventReceived.AddDynamic(this, &UAttackAbility::OnJumpSectionTiming);
			JumpSectionEventTask->ReadyForActivation();
		}
	}

	const float AttackSpeedPlayRate = CalculateWeaponAttackSpeedPlayRate(ActorInfo);
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		AttackData.AttackMontage,
		AttackSpeedPlayRate,
		NAME_None,
		false,
		1.f,
		0.f,
		false);
	if (!MontageTask)
	{

		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UAttackAbility::OnAttackMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UAttackAbility::OnAttackMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UAttackAbility::OnAttackMontageCancelled);

	if (AttackingEffectClass && !HasActiveGameplayEffect(AttackingEffectClass))
	{
		ApplyGameplayEffect(AttackingEffectClass, 1.f, 1);
	}

	FaceCurrentAttackTarget(TEXT("ActivateAbility"));
	MontageTask->ReadyForActivation();

}

// Query helpers
FName UAttackAbility::GetCurrentAttackSectionName() const
{
	UAnimMontage* CurrentAttackMontage = GetCurrentMontage();
	if (!CurrentAttackMontage)
	{
		return NAME_None;
	}

	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return NAME_None;
	}

	return AnimInstance->Montage_GetCurrentSection(CurrentAttackMontage);
}

FName UAttackAbility::GetNextAttackSectionName() const
{
	UAnimMontage* CurrentAttackMontage = GetCurrentMontage();
	if (!CurrentAttackMontage)
	{
		return NAME_None;
	}

	const FName CurrentSectionName = GetCurrentAttackSectionName();
	if (CurrentSectionName.IsNone())
	{
		return NAME_None;
	}

	const int32 CurrentSectionIndex = CurrentAttackMontage->GetSectionIndex(CurrentSectionName);
	const int32 NumSections = CurrentAttackMontage->GetNumSections();
	const int32 NextSectionIndex = CurrentSectionIndex + 1;
	if (CurrentSectionIndex == INDEX_NONE || NumSections <= 0)
	{
		return NAME_None;
	}

	if (NextSectionIndex >= NumSections)
	{
		return NAME_None;
	}

	return CurrentAttackMontage->GetSectionName(NextSectionIndex);
}

// Public requests
bool UAttackAbility::RequestNextComboInput()
{
	if (!IsActive())
	{
		return false;
	}

	const FName CurrentSectionName = GetCurrentAttackSectionName();
	if (bComboInputConsumedForCurrentWindow)
	{
		return false;
	}

	if (!bCanReceiveAttackInput)
	{
		return false;
	}

	bComboInputConsumedForCurrentWindow = true;
	if (WaitInputPressTask)
	{
		WaitInputPressTask->EndTask();
		WaitInputPressTask = nullptr;
	}

	if (!GetCurrentMontage())
	{
		// PlayMontageAndWait can report completion one frame after the montage
		// instance has disappeared. Only a window-authorized input may restart it.
		bCanReceiveAttackInput = false;
		bRestartAttackAfterMontage = true;
		return true;
	}

	const FName NextSectionName = GetNextAttackSectionName();

	if (!CurrentSectionName.IsNone() && NextSectionName.IsNone())
	{
		bCanReceiveAttackInput = false;
		bRestartAttackAfterMontage = true;
		return true;
	}

	return RequestJumpToSection(NextSectionName);
}

// 네트워크 지연 보정은 자연 종료된 같은 실행에 대해 짧은 시간 동안 한 번만 허용한다.
bool UAttackAbility::TryConsumeLateComboInput()
{
	const UWorld* World = GetWorld();
	if (IsActive() || !HasAuthority(&CurrentActivationInfo) || !World
		|| LateComboInputExpiresAt < 0.0 || World->GetTimeSeconds() > LateComboInputExpiresAt)
	{
		return false;
	}
	LateComboInputExpiresAt = -1.0;
	return true;
}

bool UAttackAbility::RequestJumpToSection(FName RequestedSectionName)
{
	if (!bCanReceiveAttackInput || RequestedSectionName.IsNone() || !IsAttackSectionNameValid(RequestedSectionName))
	{
		return false;
	}

	if (!IsAITargetInComboRange(TEXT("RequestJumpToSection")))
	{
		RequestAIChaseTarget(TEXT("RequestJumpToSection"));
		return false;
	}

	if (bReachedJumpSectionTiming)
	{
		return TryJumpToSection(RequestedSectionName);
	}

	BufferedJumpSectionName = RequestedSectionName;

	// Set the active montage instance's next-section link while the source
	// section is unquestionably still active. NotifyEnd can be delivered after
	// an unlinked section has already stopped, which is too late to set it.
	return QueueBufferedComboTransition();
}

// State helpers
void UAttackAbility::ResetAttackInputState()
{
	bCanReceiveAttackInput = false;
	bReachedJumpSectionTiming = false;
	BufferedJumpSectionName = NAME_None;
	QueuedFromSectionName = NAME_None;
	bBufferedComboCostCommitted = false;
}

void UAttackAbility::WaitForContinueInput()
{
	if (WaitInputPressTask || !IsActive())
	{
		return;
	}

	WaitInputPressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	if (!WaitInputPressTask)
	{
		return;
	}

	WaitInputPressTask->OnPress.AddDynamic(this, &ThisClass::OnContinueInputPressed);
	WaitInputPressTask->ReadyForActivation();
}

void UAttackAbility::RestartAttackAfterMontage()
{
	UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpecHandle AbilityHandle = CurrentSpecHandle;
	if (!AbilitySystemComponent || !AbilityHandle.IsValid())
	{
		return;
	}

	UWorld* World = AbilitySystemComponent->GetWorld();
	if (!World)
	{
		return;
	}

	const TWeakObjectPtr<UAbilitySystemComponent> WeakAbilitySystemComponent = AbilitySystemComponent;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WeakAbilitySystemComponent, AbilityHandle]()
		{
			if (UAbilitySystemComponent* StrongAbilitySystemComponent = WeakAbilitySystemComponent.Get())
			{
				StrongAbilitySystemComponent->TryActivateAbility(AbilityHandle, true);
			}
		}));
}

AWeaponBase* UAttackAbility::GetCurrentWeaponActor() const
{
	const ACharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

// Action helpers
bool UAttackAbility::FaceCurrentAttackTarget(const TCHAR* Context) const
{
	if (!bAIFaceTargetWhileAttacking || HasPlayerController())
	{
		return false;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	AActor* AttackTarget = GetAttackTargetFromAvatar();
	if (!IsValid(Character) || !IsValid(AttackTarget) || Character == AttackTarget)
	{
		return false;
	}
	if (Character->IsStatusFrozen())
	{

		return false;
	}

	FVector ToTarget = AttackTarget->GetActorLocation() - Character->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return false;
	}

	FRotator LookAtRotation = ToTarget.Rotation();
	LookAtRotation.Pitch = 0.0f;
	LookAtRotation.Roll = 0.0f;

	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(LookAtRotation);
	}
	Character->SetActorRotation(LookAtRotation);

	return true;
}

void UAttackAbility::RequestAIChaseTarget(const TCHAR* Context) const
{
	if (HasPlayerController())
	{
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetPdCharacterFromActorInfo());
	AActor* AttackTarget = GetAttackTargetFromAvatar();
	if (!IsValid(Enemy) || !IsValid(AttackTarget))
	{
		return;
	}

	const bool bMoveRequested = Enemy->RequestMoveToAttackTarget(AttackTarget);

}

void UAttackAbility::SetCurrentWeaponBeginOverlapEnabled(
	const bool bEnabled,
	const FName AttackSectionName) const
{
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActor();
	if (CurrentWeapon)
	{
		if (bEnabled)
		{
			CurrentWeapon->StartAttackTraceForSection(AttackSectionName);
		}
		else
		{
			CurrentWeapon->StopAttackTrace();
		}
		return;
	}

	if (ACharacterBase* Character = GetPdCharacterFromActorInfo())
	{
		if (UCombatComponent* CombatComponent = Character->GetCombatComponent())
		{
			CombatComponent->SetUnarmedAttackTraceEnabledForSection(bEnabled, AttackSectionName);
		}
	}
}

void UAttackAbility::ResetAttackDamageHitTracking() const
{
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!Character)
	{
		return;
	}

	if (AWeaponBase* CurrentWeapon = GetCurrentWeaponActor())
	{
		CurrentWeapon->ResetAttackHitTracking();
	}

	if (UCombatComponent* CombatComponent = Character->GetCombatComponent())
	{
		CombatComponent->ResetUnarmedAttackHitTracking();
	}
}

void UAttackAbility::SetCurrentComboDamageMultiplier(const float DamageMultiplier) const
{
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UCombatComponent* CombatComponent = Character ? Character->GetCombatComponent() : nullptr;
	if (CombatComponent)
	{
		CombatComponent->SetActiveComboDamageMultiplier(DamageMultiplier);
	}
}

float UAttackAbility::CalculateCurrentComboDamageMultiplier() const
{
	UAnimMontage* CurrentAttackMontage = GetCurrentMontage();
	const FName CurrentSectionName = GetCurrentAttackSectionName();
	if (!CurrentAttackMontage || CurrentSectionName.IsNone())
	{
		return 1.0f;
	}

	const int32 CurrentSectionIndex = CurrentAttackMontage->GetSectionIndex(CurrentSectionName);
	if (CurrentSectionIndex <= 0)
	{
		return 1.0f;
	}

	const float StepMultiplier = FMath::Max(ComboDamageMultiplierPerStep, 1.0f);
	return FMath::Pow(StepMultiplier, static_cast<float>(CurrentSectionIndex));
}

void UAttackAbility::PlayConfiguredComboWindowStartEffect()
{
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	const UItemDefinition* ItemDefinition = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponDefinition()
		: nullptr;
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActor();
	UNiagaraSystem* EffectSystem = ItemDefinition
		? ItemDefinition->WeaponData.Attack.ComboWindowStartEffect.Get()
		: nullptr;
	if (CurrentWeapon)
	{
		if (EffectSystem)
		{
			CurrentWeapon->PlayComboWindowStartEffect(EffectSystem);
		}
		return;
	}

	if (Character)
	{
		if (UCombatComponent* CombatComponent = Character->GetCombatComponent())
		{
			CombatComponent->PlayUnarmedComboWindowStartEffect();
		}
	}
}

// Query helpers
bool UAttackAbility::IsAttackSectionNameValid(FName SectionName) const
{
	UAnimMontage* CurrentAttackMontage = GetCurrentMontage();
	return CurrentAttackMontage
		&& !SectionName.IsNone()
		&& CurrentAttackMontage->GetSectionIndex(SectionName) != INDEX_NONE;
}

bool UAttackAbility::IsAITargetInComboRange(const TCHAR* Context) const
{
	if (HasPlayerController())
	{
		return true;
	}

	if (bAIIgnoreComboRange)
	{
		return true;
	}

	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetPdCharacterFromActorInfo());
	const AActor* AttackTarget = GetAttackTargetFromAvatar();
	if (!IsValid(Enemy) || !IsValid(AttackTarget))
	{
		return false;
	}

	const float Distance2D = Enemy->GetAttackDistanceToActor(AttackTarget);
	const float AttackStartDistance = FMath::Max(Enemy->GetAttackStartDistance(), 0.0f);
	return Distance2D <= AttackStartDistance;
}

// Action helpers
bool UAttackAbility::QueueBufferedComboTransition()
{
	if (BufferedJumpSectionName.IsNone() || !IsAttackSectionNameValid(BufferedJumpSectionName))
	{
		ResetAttackInputState();
		return false;
	}

	if (!IsAITargetInComboRange(TEXT("QueueBufferedComboTransition")))
	{
		RequestAIChaseTarget(TEXT("QueueBufferedComboTransition"));
		ResetAttackInputState();
		return false;
	}

	UAnimMontage* CurrentAttackMontage = GetCurrentMontage();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	const FName CurrentSectionName = GetCurrentAttackSectionName();
	if (!CurrentAttackMontage || !AnimInstance || CurrentSectionName.IsNone())
	{
		ResetAttackInputState();
		return false;
	}

	if (!bBufferedComboCostCommitted)
	{
		if (!TryCommitAdditionalActionStaminaCost())
		{
			ResetAttackInputState();
			return false;
		}

		bBufferedComboCostCommitted = true;
	}

	FaceCurrentAttackTarget(TEXT("QueueComboAtSectionEnd"));
	AnimInstance->Montage_SetNextSection(
		CurrentSectionName,
		BufferedJumpSectionName,
		CurrentAttackMontage);
	QueuedFromSectionName = CurrentSectionName;

	// Preserve the buffered section until either the optional NextCombo event
	// consumes it or the following section opens its own input window.
	bCanReceiveAttackInput = false;
	return true;
}

bool UAttackAbility::TryJumpToSection(FName SectionName)
{
	if (!IsAttackSectionNameValid(SectionName))
	{
		ResetAttackInputState();
		return false;
	}

	if (!IsAITargetInComboRange(TEXT("TryJumpToSection")))
	{
		RequestAIChaseTarget(TEXT("TryJumpToSection"));
		ResetAttackInputState();
		return false;
	}

	const bool bComboCostAlreadyCommitted =
		bBufferedComboCostCommitted && BufferedJumpSectionName == SectionName;
	if (!bComboCostAlreadyCommitted && !TryCommitAdditionalActionStaminaCost())
	{
		ResetAttackInputState();
		return false;
	}

	FaceCurrentAttackTarget(TEXT("JumpToSection"));
	MontageJumpToSection(SectionName);
	ResetAttackInputState();
	return true;
}
