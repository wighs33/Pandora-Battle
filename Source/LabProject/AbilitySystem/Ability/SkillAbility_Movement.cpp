#include "AbilitySystem/Ability/SkillAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Settings/GameSettingsSubsystem.h"

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

void USkillAbility::ApplyActiveMovementSpeedBonus(FActiveGameplayEffectHandle& InOutEffectHandle)
{
	if (InOutEffectHandle.IsValid())
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = GetSourceSkillDataAsset();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const TSubclassOf<UGameplayEffect> MovementSpeedEffectClass =
		SettingDefinition
			? SettingDefinition->MovementSpeedGameplayEffectClass
			: nullptr;
	if (!SkillDataAsset
		|| !SkillDataAsset->Movement.bOverrideMovementSpeedWhileActive
		|| SkillDataAsset->Movement.MovementSpeedBonusPercent <= 0.0
		|| !Character
		|| !Character->HasAuthority()
		|| !AbilitySystemComponent
		|| !MovementSpeedEffectClass)
	{
		return;
	}

	FGameplayEffectSpecHandle MovementSpeedSpec = MakeOutgoingGameplayEffectSpec(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActorInfo(),
		GetCurrentActivationInfo(),
		MovementSpeedEffectClass,
		GetAbilityLevel());
	if (!MovementSpeedSpec.IsValid() || !MovementSpeedSpec.Data.IsValid())
	{
		return;
	}

	MovementSpeedSpec.Data->SetSetByCallerMagnitude(
		LabGameplayTags::Data_MovementSpeed,
		static_cast<float>(SkillDataAsset->Movement.MovementSpeedBonusPercent));
	InOutEffectHandle = ApplyGameplayEffectSpecToOwner(
		GetCurrentAbilitySpecHandle(),
		GetCurrentActorInfo(),
		GetCurrentActivationInfo(),
		MovementSpeedSpec);
}

void USkillAbility::RemoveActiveMovementSpeedBonus(FActiveGameplayEffectHandle& InOutEffectHandle)
{
	if (!InOutEffectHandle.IsValid())
	{
		return;
	}

	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	UPdAbilitySystemComponent* AbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (Character && Character->HasAuthority() && AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(InOutEffectHandle, 1);
	}

	InOutEffectHandle.Invalidate();
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
