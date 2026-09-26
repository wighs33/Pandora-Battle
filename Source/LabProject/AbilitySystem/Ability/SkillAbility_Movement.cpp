#include "AbilitySystem/Ability/SkillAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"

namespace
{
constexpr float MovementContactDamageTickInterval = 1.0f / 30.0f;
constexpr float MovementContactDamageCapsuleInflation = 15.0f;
}

void USkillAbility::StopAvatarMovementForSkillActivation()
{
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
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

void USkillAbility::LockAvatarMovementForAbility()
{
	if (bAbilityMovementLocked)
	{
		return;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
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

void USkillAbility::RestoreAvatarMovementForAbility()
{
	if (!bAbilityMovementLocked)
	{
		return;
	}

	bAbilityMovementLocked = false;

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
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

void USkillAbility::StartDurationMovementLock()
{
	if (bDurationMovementLockActive)
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset || !SkillDataAsset->Movement.bLockMovementDuringDuration || SkillDataAsset->SkillType != ESkillType::Duration
		|| SkillDataAsset->Time.Duration <= 0.0)
	{
		return;
	}

	const bool bWasAlreadyLocked = bAbilityMovementLocked;
	LockAvatarMovementForAbility();
	bDurationMovementLockActive = !bWasAlreadyLocked && bAbilityMovementLocked;
}

void USkillAbility::StopDurationMovementLock()
{
	if (!bDurationMovementLockActive)
	{
		return;
	}

	bDurationMovementLockActive = false;
	RestoreAvatarMovementForAbility();
}

void USkillAbility::StartMovementContactDamage()
{
	StopMovementContactDamage();

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	const FSkillMovementSettings* MovementSettings = SkillDataAsset ? &SkillDataAsset->Movement : nullptr;
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !MovementSettings || !MovementSettings->bDamageEnemiesOnContact || !AvatarActor || !AvatarActor->HasAuthority()
		|| !Character || !Character->GetCapsuleComponent())
	{
		return;
	}

	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	if (!DamageConfig.GameplayEffectClass
		|| CalculateDamageMagnitude(DamageConfig) <= 0.0f)
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

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MovementContactDamageTimerHandle, this, &ThisClass::HandleMovementContactDamageTick, MovementContactDamageTickInterval, true);
	}
}

void USkillAbility::StopMovementContactDamage()
{
	if (UWorld* World = GetWorld())
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

void USkillAbility::HandleMovementContactDamageTick()
{
	if (!bMovementContactDamageActive)
	{
		return;
	}


	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UCapsuleComponent* CapsuleComponent = Character ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !CapsuleComponent || !World)
	{
		StopMovementContactDamage();
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
			ApplyMovementContactDamageToActor(ContactActor);
		}
	}
}

void USkillAbility::ApplyMovementContactDamageToActor(AActor* HitActor)
{
	AActor* SourceActor = GetAvatarActorFromActorInfo();
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

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		return;
	}
	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	const float DamageMagnitude = CalculateDamageMagnitude(DamageConfig);
	if (!DamageConfig.GameplayEffectClass || DamageMagnitude <= 0.0f)
	{
		return;
	}
	FGameplayEffectSpecHandle DamageSpecHandle = MakeConfiguredDamageEffectSpec(DamageConfig, DamageMagnitude);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetAbilitySystemComponent);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		ApplyConfiguredStatusEffectToTarget(GetSourceSkillDataAsset(), TargetAbilitySystemComponent);
	}
}
