#include "Component/AbilitySystem/Ability/AbilityMovementManager.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityMovementManager)

namespace
{
constexpr float MovementContactDamageTickInterval = 1.0f / 30.0f;
constexpr float MovementContactDamageCapsuleInflation = 15.0f;
} // namespace

// 시전 확정 시 이동 요청·입력·속도를 비워 이전 이동이 스킬 실행에 남지 않게 한다.
void UAbilityMovementManager::StopAvatarMovementForSkillActivation(UPdGameplayAbility& Ability)
{
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !MovementComponent)
	{
		return;
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->StopMovement();
	}

	Character->ConsumeMovementInputVector();
	MovementComponent->StopMovementImmediately();
}

// 현재 이동·회전 설정을 저장하고 입력을 잠근다. 빙결 중에는 회전도 정지한다.
void UAbilityMovementManager::LockAvatarMovementForAbility(UPdGameplayAbility& Ability)
{
	if (bAbilityMovementLocked)
	{
		return;
	}

	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	if (!MovementComponent)
	{
		return;
	}

	CachedAbilityMovementMode = static_cast<uint8>(MovementComponent->MovementMode);
	CachedAbilityCustomMovementMode = MovementComponent->CustomMovementMode;
	bCachedAbilityOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
	bCachedAbilityUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
	bCachedAbilityUseControllerRotationYaw = Character->bUseControllerRotationYaw;
	CachedAbilityRotationRate = MovementComponent->RotationRate;
	bAbilityMovementLocked = true;

	if (AController* Controller = Character->GetController())
	{
		Controller->StopMovement();
		Controller->SetIgnoreMoveInput(true);
	}

	MovementComponent->StopMovementImmediately();
	if (Character->IsStatusFrozen())
	{
		MovementComponent->MaxWalkSpeed = 0.0f;
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->bUseControllerDesiredRotation = false;
		MovementComponent->RotationRate = FRotator::ZeroRotator;
		Character->bUseControllerRotationYaw = false;
		return;
	}

	MovementComponent->bOrientRotationToMovement = false;
	MovementComponent->bUseControllerDesiredRotation = true;
	MovementComponent->RotationRate = FRotator(0.0f, 3000.0f, 0.0f);
	Character->bUseControllerRotationYaw = false;
}

// 이 능력의 입력 잠금을 해제한다. 사망·빙결을 우선하고 살아 있는 캐릭터의 현재 조준 정책을 복구한다.
void UAbilityMovementManager::RestoreAvatarMovementForAbility(UPdGameplayAbility& Ability)
{
	if (!bAbilityMovementLocked)
	{
		return;
	}

	bAbilityMovementLocked = false;

	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	if (!MovementComponent)
	{
		return;
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(false);
	}

	if (Character->IsDead())
	{
		return;
	}

	if (Character->IsStatusFrozen())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->MaxWalkSpeed = 0.0f;
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->bUseControllerDesiredRotation = false;
		MovementComponent->RotationRate = FRotator::ZeroRotator;
		Character->bUseControllerRotationYaw = false;
		return;
	}

	const EMovementMode RestoredMovementMode =
		CachedAbilityMovementMode != static_cast<uint8>(MOVE_None) ? static_cast<EMovementMode>(CachedAbilityMovementMode) : MOVE_Walking;

	MovementComponent->Activate(true);
	MovementComponent->SetComponentTickEnabled(true);
	MovementComponent->StopMovementImmediately();
	if (MovementComponent->MovementMode == MOVE_None)
	{
		MovementComponent->SetMovementMode(RestoredMovementMode, CachedAbilityCustomMovementMode);
	}
	MovementComponent->bOrientRotationToMovement = bCachedAbilityOrientRotationToMovement;
	MovementComponent->bUseControllerDesiredRotation = bCachedAbilityUseControllerDesiredRotation;
	MovementComponent->RotationRate = CachedAbilityRotationRate;
	Character->bUseControllerRotationYaw = bCachedAbilityUseControllerRotationYaw;

	// Aim can start or stop while an ability owns movement. Cached flags then
	// describe an obsolete state, so let the character's current policy win.
	Character->ReapplyCurrentRotationPolicy();
}

// 지속시간 동안 잠금이 필요한 스킬만 잠근다. 이미 다른 단계에서 잡은 잠금은 소유하지 않는다.
void UAbilityMovementManager::StartDurationMovementLock(UPdGameplayAbility& Ability)
{
	if (bDurationMovementLockActive)
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = Ability.GetSourceSkillDataAsset();
	if (!SkillDataAsset || !SkillDataAsset->Movement.bLockMovementDuringDuration || SkillDataAsset->SkillType != ESkillType::Duration
		|| SkillDataAsset->Time.Duration <= 0.0)
	{
		return;
	}

	const bool bWasAlreadyLocked = bAbilityMovementLocked;
	LockAvatarMovementForAbility(Ability);
	bDurationMovementLockActive = !bWasAlreadyLocked && bAbilityMovementLocked;
}

// 지속시간 단계에서 직접 잡았던 잠금만 해제한다.
void UAbilityMovementManager::StopDurationMovementLock(UPdGameplayAbility& Ability)
{
	if (!bDurationMovementLockActive)
	{
		return;
	}

	bDurationMovementLockActive = false;
	RestoreAvatarMovementForAbility(Ability);
}

// 서버에서 접촉 피해 조건을 확인하고 최초 검사 후 반복 타이머를 등록한다.
void UAbilityMovementManager::StartMovementContactDamage(UPdGameplayAbility& Ability)
{
	StopMovementContactDamage(Ability);

	const USkillDefinition* SkillDataAsset = Ability.GetSourceSkillDataAsset();
	const FSkillMovementSettings* MovementSettings = SkillDataAsset ? &SkillDataAsset->Movement : nullptr;
	AActor* AvatarActor = Ability.GetAvatarActorFromActorInfo();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !MovementSettings || !MovementSettings->bDamageEnemiesOnContact || !AvatarActor || !AvatarActor->HasAuthority()
		|| !Character || !Character->GetCapsuleComponent())
	{
		return;
	}

	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	if (!DamageConfig.GameplayEffectClass || Ability.CalculateSkillDamageMagnitude(DamageConfig) <= 0.0f)
	{
		return;
	}

	bMovementContactDamageActive = true;
	MovementContactOverlappingActors.Reset();
	MovementContactDamagePreviousLocation = Character->GetCapsuleComponent()->GetComponentLocation();

	HandleMovementContactDamageTick();
	// 첫 피해 적용 중 능력이 종료되었다면 타이머를 다시 등록하지 않는다.
	if (!bMovementContactDamageActive)
	{
		return;
	}

	if (UWorld* World = Ability.GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MovementContactDamageTimerHandle, this, &ThisClass::HandleMovementContactDamageTick, MovementContactDamageTickInterval, true);
	}
}

// 접촉 피해 타이머와 이전 위치·대상 기록을 함께 비워 다음 시전에 남지 않게 한다.
void UAbilityMovementManager::StopMovementContactDamage(UPdGameplayAbility& Ability)
{
	if (UWorld* World = Ability.GetWorld())
	{
		World->GetTimerManager().ClearTimer(MovementContactDamageTimerHandle);
	}

	MovementContactDamageTimerHandle.Invalidate();
	bMovementContactDamageActive = false;
	MovementContactDamagePreviousLocation = FVector::ZeroVector;
	MovementContactOverlappingActors.Reset();
	MovementContactCurrentActors.Reset();
	MovementContactSweepHits.Reset();
	MovementContactOverlapResults.Reset();
}

// 이동 경로와 현재 캡슐을 검사해 새로 접촉한 대상을 모은 뒤 피해를 적용한다. 계속 접촉 중인 대상은 재타격하지 않는다.
void UAbilityMovementManager::HandleMovementContactDamageTick()
{
	if (!bMovementContactDamageActive)
	{
		return;
	}

	UPdGameplayAbility* Ability = GetTypedOuter<UPdGameplayAbility>();
	if (!Ability)
	{
		bMovementContactDamageActive = false;
		return;
	}

	ACharacterBase* Character = Ability->GetPdCharacterFromActorInfo();
	UCapsuleComponent* CapsuleComponent = Character ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !CapsuleComponent || !World)
	{
		StopMovementContactDamage(*Ability);
		return;
	}

	const FVector CurrentLocation = CapsuleComponent->GetComponentLocation();
	const FVector PreviousLocation =
		MovementContactDamagePreviousLocation.IsNearlyZero() ? CurrentLocation : MovementContactDamagePreviousLocation;
	const FQuat CapsuleRotation = CapsuleComponent->GetComponentQuat();
	const FCollisionShape ContactShape =
		FCollisionShape::MakeCapsule(CapsuleComponent->GetScaledCapsuleRadius() + MovementContactDamageCapsuleInflation,
			CapsuleComponent->GetScaledCapsuleHalfHeight() + MovementContactDamageCapsuleInflation);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MovementContactDamage), false, Character);
	QueryParams.AddIgnoredActor(Character);

	MovementContactCurrentActors.Reset();
	MovementContactSweepHits.Reset();
	MovementContactOverlapResults.Reset();

	TArray<TWeakObjectPtr<AActor>> ContactDamageTargets;
	const auto ProcessContactActor = [this, &ContactDamageTargets](AActor* ContactActor) {
		if (!IsValid(ContactActor))
		{
			return;
		}

		const FObjectKey ContactActorKey(ContactActor);
		const bool bAlreadySeenThisTick = MovementContactCurrentActors.Contains(ContactActorKey);
		MovementContactCurrentActors.Add(ContactActorKey);
		if (bAlreadySeenThisTick || MovementContactOverlappingActors.Contains(ContactActorKey))
		{
			return;
		}

		ContactDamageTargets.Add(ContactActor);
	};

	if (!PreviousLocation.Equals(CurrentLocation, UE_KINDA_SMALL_NUMBER))
	{
		World->SweepMultiByObjectType(
			MovementContactSweepHits, PreviousLocation, CurrentLocation, CapsuleRotation, ObjectQueryParams, ContactShape, QueryParams);

		for (const FHitResult& SweepHit : MovementContactSweepHits)
		{
			ProcessContactActor(SweepHit.GetActor());
		}
	}

	World->OverlapMultiByObjectType(
		MovementContactOverlapResults, CurrentLocation, CapsuleRotation, ObjectQueryParams, ContactShape, QueryParams);

	for (const FOverlapResult& OverlapResult : MovementContactOverlapResults)
	{
		ProcessContactActor(OverlapResult.GetActor());
	}

	MovementContactOverlappingActors = MoveTemp(MovementContactCurrentActors);
	MovementContactDamagePreviousLocation = CurrentLocation;

	for (const TWeakObjectPtr<AActor>& ContactActorPtr : ContactDamageTargets)
	{
		if (!bMovementContactDamageActive)
		{
			break;
		}

		if (AActor* ContactActor = ContactActorPtr.Get())
		{
			ApplyMovementContactDamageToActor(*Ability, ContactActor);
		}
	}
}

// 살아 있는 적에게 스킬 피해를 적용하고, 성공한 경우 설정된 상태 효과를 이어서 적용한다.
void UAbilityMovementManager::ApplyMovementContactDamageToActor(UPdGameplayAbility& Ability, AActor* HitActor)
{
	AActor* SourceActor = Ability.GetAvatarActorFromActorInfo();
	if (!bMovementContactDamageActive || !IsValid(HitActor) || HitActor == SourceActor)
	{
		return;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(SourceActor);
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (!SourceCharacter || !TargetCharacter)
	{
		return;
	}

	UAbilitySystemComponent* SourceAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
	UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!SourceAbilitySystemComponent || !TargetAbilitySystemComponent)
	{
		return;
	}

	if (TargetCharacter->IsDead() || !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = Ability.GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		return;
	}
	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	const float DamageMagnitude = Ability.CalculateSkillDamageMagnitude(DamageConfig);
	if (!DamageConfig.GameplayEffectClass || DamageMagnitude <= 0.0f)
	{
		return;
	}
	FGameplayEffectSpecHandle DamageSpecHandle = Ability.MakeConfiguredDamageEffectSpec(DamageConfig, DamageMagnitude);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetAbilitySystemComponent);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		Ability.ApplyConfiguredStatusEffectToTarget(Ability.GetSourceSkillDataAsset(), TargetAbilitySystemComponent);
	}
}