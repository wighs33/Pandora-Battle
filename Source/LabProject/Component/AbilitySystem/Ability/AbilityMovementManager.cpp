#include "Component/AbilitySystem/Ability/AbilityMovementManager.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
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
}

UPdGameplayAbility* UAbilityMovementManager::GetOwningAbility() const
{
	return GetTypedOuter<UPdGameplayAbility>();
}

void UAbilityMovementManager::StopAvatarMovementForSkillActivation(
	UPdGameplayAbility& Ability)
{
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr;
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

void UAbilityMovementManager::LockAvatarMovementForAbility(
	UPdGameplayAbility& Ability)
{
	if (bAbilityMovementLocked)
	{
		return;
	}

	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr;
	if (!MovementComponent)
	{
		return;
	}

	CachedAbilityMovementMode =
		static_cast<uint8>(MovementComponent->MovementMode);
	CachedAbilityCustomMovementMode =
		MovementComponent->CustomMovementMode;
	bCachedAbilityOrientRotationToMovement =
		MovementComponent->bOrientRotationToMovement;
	bCachedAbilityUseControllerDesiredRotation =
		MovementComponent->bUseControllerDesiredRotation;
	bCachedAbilityUseControllerRotationYaw =
		Character->bUseControllerRotationYaw;
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
	MovementComponent->RotationRate =
		FRotator(0.0f, 3000.0f, 0.0f);
	Character->bUseControllerRotationYaw = false;
}

void UAbilityMovementManager::RestoreAvatarMovementForAbility(
	UPdGameplayAbility& Ability)
{
	if (!bAbilityMovementLocked)
	{
		return;
	}

	bAbilityMovementLocked = false;

	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr;
	if (!MovementComponent)
	{
		return;
	}

	if (AController* Controller = Character->GetController())
	{
		Controller->SetIgnoreMoveInput(false);
	}

	const UAbilitySystemComponent* AbilitySystemComponent =
		Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (AbilitySystemComponent
		&& AbilitySystemComponent->HasMatchingGameplayTag(
			LabGameplayTags::State_Dead))
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
		CachedAbilityMovementMode != static_cast<uint8>(MOVE_None)
			? static_cast<EMovementMode>(CachedAbilityMovementMode)
			: MOVE_Walking;

	MovementComponent->Activate(true);
	MovementComponent->SetComponentTickEnabled(true);
	MovementComponent->StopMovementImmediately();
	if (MovementComponent->MovementMode == MOVE_None)
	{
		MovementComponent->SetMovementMode(
			RestoredMovementMode,
			CachedAbilityCustomMovementMode);
	}
	MovementComponent->bOrientRotationToMovement =
		bCachedAbilityOrientRotationToMovement;
	MovementComponent->bUseControllerDesiredRotation =
		bCachedAbilityUseControllerDesiredRotation;
	MovementComponent->RotationRate = CachedAbilityRotationRate;
	Character->bUseControllerRotationYaw =
		bCachedAbilityUseControllerRotationYaw;

	// Aim can start or stop while an ability owns movement. Cached flags then
	// describe an obsolete state, so let the character's current policy win.
	Character->ReapplyCurrentRotationPolicy();
}

void UAbilityMovementManager::StartDurationMovementLock(
	UPdGameplayAbility& Ability)
{
	if (bDurationMovementLockActive)
	{
		return;
	}

	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	if (!SkillDataAsset
		|| !SkillDataAsset->Movement.bLockMovementDuringDuration
		|| SkillDataAsset->SkillType != ESkillType::Duration
		|| SkillDataAsset->Time.Duration <= 0.0)
	{
		return;
	}

	const bool bWasAlreadyLocked = bAbilityMovementLocked;
	LockAvatarMovementForAbility(Ability);
	bDurationMovementLockActive =
		!bWasAlreadyLocked && bAbilityMovementLocked;
}

void UAbilityMovementManager::StopDurationMovementLock(
	UPdGameplayAbility& Ability)
{
	if (!bDurationMovementLockActive)
	{
		return;
	}

	bDurationMovementLockActive = false;
	RestoreAvatarMovementForAbility(Ability);
}

void UAbilityMovementManager::StartMovementContactDamage(
	UPdGameplayAbility& Ability)
{
	StopMovementContactDamage(Ability);

	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	const FSkillMovementSettings* MovementSettings =
		SkillDataAsset ? &SkillDataAsset->Movement : nullptr;
	AActor* AvatarActor = Ability.GetAvatarActorFromActorInfo();
	ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset
		|| !MovementSettings
		|| !MovementSettings->bDamageEnemiesOnContact
		|| !AvatarActor
		|| !AvatarActor->HasAuthority()
		|| !Character
		|| !Character->GetCapsuleComponent())
	{
		return;
	}

	const FSkillGameplayEffectConfig DamageConfig =
		SkillDataAsset->GetResolvedDamageConfig();
	if (!DamageConfig.GameplayEffectClass
		|| CalculateMovementContactDamageMagnitude(Ability) <= 0.0f)
	{
		return;
	}

	bMovementContactDamageActive = true;
	MovementContactOverlappingActors.Reset();
	MovementContactDamagePreviousLocation =
		Character->GetCapsuleComponent()->GetComponentLocation();

	HandleMovementContactDamageTick();

	if (UWorld* World = Ability.GetWorld())
	{
		World->GetTimerManager().SetTimer(
			MovementContactDamageTimerHandle,
			this,
			&ThisClass::HandleMovementContactDamageTick,
			MovementContactDamageTickInterval,
			true);
	}
}

void UAbilityMovementManager::StopMovementContactDamage(
	UPdGameplayAbility& Ability)
{
	if (UWorld* World = Ability.GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			MovementContactDamageTimerHandle);
	}

	MovementContactDamageTimerHandle.Invalidate();
	bMovementContactDamageActive = false;
	MovementContactDamagePreviousLocation = FVector::ZeroVector;
	MovementContactOverlappingActors.Reset();
	MovementContactCurrentActors.Reset();
	MovementContactProcessedActors.Reset();
	MovementContactSweepHits.Reset();
	MovementContactOverlapResults.Reset();
}

void UAbilityMovementManager::HandleMovementContactDamageTick()
{
	if (!bMovementContactDamageActive)
	{
		return;
	}

	UPdGameplayAbility* Ability = GetOwningAbility();
	if (!Ability)
	{
		bMovementContactDamageActive = false;
		return;
	}

	ACharacterBase* Character = Ability->GetPdCharacterFromActorInfo();
	UCapsuleComponent* CapsuleComponent =
		Character ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !CapsuleComponent || !World)
	{
		StopMovementContactDamage(*Ability);
		return;
	}

	const FVector CurrentLocation =
		CapsuleComponent->GetComponentLocation();
	const FVector PreviousLocation =
		MovementContactDamagePreviousLocation.IsNearlyZero()
			? CurrentLocation
			: MovementContactDamagePreviousLocation;
	const FQuat CapsuleRotation = CapsuleComponent->GetComponentQuat();
	const FCollisionShape ContactShape = FCollisionShape::MakeCapsule(
		CapsuleComponent->GetScaledCapsuleRadius()
			+ MovementContactDamageCapsuleInflation,
		CapsuleComponent->GetScaledCapsuleHalfHeight()
			+ MovementContactDamageCapsuleInflation);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(MovementContactDamage),
		false,
		Character);
	QueryParams.AddIgnoredActor(Character);

	MovementContactCurrentActors.Reset();
	MovementContactProcessedActors.Reset();
	MovementContactSweepHits.Reset();
	MovementContactOverlapResults.Reset();

	TArray<TWeakObjectPtr<AActor>> ContactDamageTargets;
	const auto ProcessContactActor =
		[this, &ContactDamageTargets](AActor* ContactActor)
		{
			if (!IsValid(ContactActor))
			{
				return;
			}

			const FObjectKey ContactActorKey(ContactActor);
			MovementContactCurrentActors.Add(ContactActorKey);
			if (MovementContactProcessedActors.Contains(ContactActorKey)
				|| MovementContactOverlappingActors.Contains(
					ContactActorKey))
			{
				return;
			}

			MovementContactProcessedActors.Add(ContactActorKey);
			ContactDamageTargets.Add(ContactActor);
		};

	if (!PreviousLocation.Equals(
		CurrentLocation,
		UE_KINDA_SMALL_NUMBER))
	{
		World->SweepMultiByObjectType(
			MovementContactSweepHits,
			PreviousLocation,
			CurrentLocation,
			CapsuleRotation,
			ObjectQueryParams,
			ContactShape,
			QueryParams);

		for (const FHitResult& SweepHit : MovementContactSweepHits)
		{
			ProcessContactActor(SweepHit.GetActor());
		}
	}

	World->OverlapMultiByObjectType(
		MovementContactOverlapResults,
		CurrentLocation,
		CapsuleRotation,
		ObjectQueryParams,
		ContactShape,
		QueryParams);

	for (const FOverlapResult& OverlapResult :
		MovementContactOverlapResults)
	{
		ProcessContactActor(OverlapResult.GetActor());
	}

	MovementContactOverlappingActors =
		MoveTemp(MovementContactCurrentActors);
	MovementContactDamagePreviousLocation = CurrentLocation;

	for (const TWeakObjectPtr<AActor>& ContactActorPtr :
		ContactDamageTargets)
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

void UAbilityMovementManager::ApplyMovementContactDamageToActor(
	UPdGameplayAbility& Ability,
	AActor* HitActor)
{
	AActor* SourceActor = Ability.GetAvatarActorFromActorInfo();
	if (!bMovementContactDamageActive
		|| !IsValid(HitActor)
		|| HitActor == SourceActor)
	{
		return;
	}

	const ACharacterBase* SourceCharacter =
		Cast<ACharacterBase>(SourceActor);
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (!SourceCharacter || !TargetCharacter)
	{
		return;
	}

	UAbilitySystemComponent* SourceAbilitySystemComponent =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
			SourceActor);
	UAbilitySystemComponent* TargetAbilitySystemComponent =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!SourceAbilitySystemComponent || !TargetAbilitySystemComponent)
	{
		return;
	}

	if (TargetAbilitySystemComponent->HasMatchingGameplayTag(
			LabGameplayTags::State_Dead)
		|| !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{
		return;
	}

	const float DamageMagnitude =
		CalculateMovementContactDamageMagnitude(Ability);
	FGameplayEffectSpecHandle DamageSpecHandle =
		MakeMovementContactDamageSpec(Ability, DamageMagnitude);
	if (!DamageSpecHandle.IsValid()
		|| !DamageSpecHandle.Data.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(
		*DamageSpecHandle.Data.Get(),
		TargetAbilitySystemComponent);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		Ability.ApplyConfiguredStatusEffectToTarget(
			Ability.GetSourceSkillDataAsset(),
			TargetAbilitySystemComponent);
	}
}

FGameplayEffectSpecHandle
UAbilityMovementManager::MakeMovementContactDamageSpec(
	const UPdGameplayAbility& Ability,
	const float DamageMagnitude) const
{
	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	const FSkillGameplayEffectConfig ContactDamage = SkillDataAsset
		? SkillDataAsset->GetResolvedDamageConfig()
		: FSkillGameplayEffectConfig();
	if (!SkillDataAsset
		|| !ContactDamage.GameplayEffectClass
		|| DamageMagnitude <= 0.0f)
	{
		return FGameplayEffectSpecHandle();
	}

	return Ability.MakeConfiguredDamageEffectSpec(
		ContactDamage,
		DamageMagnitude);
}

float UAbilityMovementManager::CalculateMovementContactDamageMagnitude(
	const UPdGameplayAbility& Ability) const
{
	const USkillDefinition* SkillDataAsset =
		Ability.GetSourceSkillDataAsset();
	return SkillDataAsset
		? Ability.CalculateSkillDamageMagnitude(
			SkillDataAsset->GetResolvedDamageConfig())
		: 0.0f;
}
