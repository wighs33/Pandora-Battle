#include "Component/Character/EnemyTrainingBotComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AI/TrainingBotAIController.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/EnemyBase.h"
#include "Common/EquipmentAbilityData.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Character/EnemyCombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Definition/Item/ItemDefinition.h"
#include "Item/ArrowProjectileBase.h"
#include "Templates/UnrealTemplate.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyTrainingBotComponent)

UEnemyTrainingBotComponent::UEnemyTrainingBotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	const UEnemyBaseDefinition* NativeDefaults =
		GetDefault<UEnemyBaseDefinition>();
	if (NativeDefaults)
	{
		Settings = NativeDefaults->GetTrainingBotSettings();
	}
}

void UEnemyTrainingBotComponent::ApplySettings(
	const FEnemyTrainingBotSettings& InSettings)
{
	Settings = InSettings;
	const auto SanitizeNonNegative =
		[](const float Value)
		{
			return FMath::IsFinite(Value)
				? FMath::Max(Value, 0.0f)
				: 0.0f;
		};
	Settings.HitStunDuration =
		SanitizeNonNegative(Settings.HitStunDuration);
	Settings.RespawnDelay =
		SanitizeNonNegative(Settings.RespawnDelay);
}

AEnemyBase* UEnemyTrainingBotComponent::GetEnemyOwner() const
{
	return Cast<AEnemyBase>(GetOwner());
}

const AEnemyBase* UEnemyTrainingBotComponent::GetEnemyOwnerConst() const
{
	return Cast<AEnemyBase>(GetOwner());
}

void UEnemyTrainingBotComponent::InitializeRuntime()
{
	CacheRespawnTransform();
}

void UEnemyTrainingBotComponent::ShutdownRuntime()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy)
	{
		return;
	}

	Enemy->GetWorldTimerManager().ClearTimer(HitStunTimerHandle);
	Enemy->GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	Enemy->GetWorldTimerManager().ClearTimer(WeaponChangeTimerHandle);

	PendingWeaponDefinition = nullptr;
	bPendingUnarmed = false;
	bWeaponChangeInProgress = false;
	bRespawnScheduled = false;
	bRespawnResetInProgress = false;
}

bool UEnemyTrainingBotComponent::ShouldUseHitReaction() const
{
	if (!Settings.bEnableHitReaction)
	{
		return false;
	}

	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	const AController* Controller = Enemy ? Enemy->GetController() : nullptr;
	return Controller && Controller->IsA<ATrainingBotAIController>();
}

bool UEnemyTrainingBotComponent::ShouldUseRespawn() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	const AController* Controller = Enemy ? Enemy->GetController() : nullptr;
	return Settings.bRespawnOnDeath
		&& Controller
		&& Controller->IsA<ATrainingBotAIController>();
}

bool UEnemyTrainingBotComponent::ShouldSuppressDeathHandling() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	if (!Enemy || !ShouldUseRespawn())
	{
		return false;
	}

	return bRespawnResetInProgress
		|| bRespawnScheduled
		|| Enemy->GetWorldTimerManager().IsTimerActive(RespawnTimerHandle);
}

void UEnemyTrainingBotComponent::HandleDeathAfterBase()
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	if (!Enemy || !Enemy->HasAuthority() || !ShouldUseRespawn())
	{
		return;
	}

	ScheduleRespawn();
}

void UEnemyTrainingBotComponent::HandleDamageTaken(
	const float DamageAmount,
	const bool bCriticalHit,
	const bool bAllowHitReact)
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy
		|| !bAllowHitReact
		|| !Enemy->HasAuthority()
		|| DamageAmount <= 0.0f
		|| !ShouldUseHitReaction())
	{
		return;
	}

	UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy->GetEnemyAbilitySystemComponent();
	if (!AbilitySystemComponent
		|| AbilitySystemComponent->HasMatchingGameplayTag(
			LabGameplayTags::State_Dead))
	{
		return;
	}

	const UBasicAttributeSet* AttributeSet =
		AbilitySystemComponent->GetSet<UBasicAttributeSet>();
	if (AttributeSet && AttributeSet->GetHealth() <= 0.0f)
	{
		return;
	}

	TriggerHitReaction(DamageAmount, bCriticalHit);
}

void UEnemyTrainingBotComponent::TriggerHitReaction(
	const float DamageAmount,
	const bool bCriticalHit)
{
	static_cast<void>(DamageAmount);
	static_cast<void>(bCriticalHit);

	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy)
	{
		return;
	}

	StartHitStun();

	FGameplayTagContainer CancelTags;
	CancelTags.AddTag(LabGameplayTags::Action_Attack);
	CancelTags.AddTag(LabGameplayTags::Action_RangedAttack);
	if (UPdAbilitySystemComponent* AbilitySystemComponent =
			Enemy->GetEnemyAbilitySystemComponent())
	{
		AbilitySystemComponent->CancelAbilities(&CancelTags);
	}

	if (!TryActivateHitReactAbility())
	{
		Enemy->MulticastPlayTrainingHitReactMontage();
	}
}

void UEnemyTrainingBotComponent::StartHitStun()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	if (AAIController* AIController =
			Cast<AAIController>(Enemy->GetController()))
	{
		AIController->StopMovement();
	}

	if (UCharacterMovementComponent* Movement =
			Enemy->GetCharacterMovement())
	{
		if (!bHitStunned)
		{
			PreHitStunMovementMode = Movement->MovementMode;
			PreHitStunCustomMovementMode = Movement->CustomMovementMode;
		}
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	bHitStunned = true;
	Enemy->GetWorldTimerManager().ClearTimer(HitStunTimerHandle);
	if (Settings.HitStunDuration <= 0.0f)
	{
		EndHitStun();
		return;
	}

	Enemy->GetWorldTimerManager().SetTimer(
		HitStunTimerHandle,
		this,
		&ThisClass::EndHitStun,
		Settings.HitStunDuration,
		false);
}

void UEnemyTrainingBotComponent::EndHitStun()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	bHitStunned = false;
	if (UCharacterMovementComponent* Movement =
			Enemy->GetCharacterMovement())
	{
		const EMovementMode RestoreMode =
			PreHitStunMovementMode == MOVE_None
				? MOVE_Walking
				: PreHitStunMovementMode.GetValue();
		Movement->SetMovementMode(
			RestoreMode,
			PreHitStunCustomMovementMode);
	}
}

bool UEnemyTrainingBotComponent::TryActivateHitReactAbility()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	UPdAbilitySystemComponent* AbilitySystemComponent =
		Enemy ? Enemy->GetEnemyAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const auto TryActivateByTag =
		[AbilitySystemComponent](const FGameplayTag& AbilityTag)
		{
			if (!AbilityTag.IsValid())
			{
				return false;
			}

			FGameplayTagContainer AbilityTags;
			AbilityTags.AddTag(AbilityTag);

			TArray<FGameplayAbilitySpecHandle> AbilityHandles;
			AbilitySystemComponent->FindAllAbilitiesWithTags(
				AbilityHandles,
				AbilityTags,
				false);
			for (const FGameplayAbilitySpecHandle& AbilityHandle :
				AbilityHandles)
			{
				if (AbilitySystemComponent->TryActivateAbility(
						AbilityHandle,
						true))
				{
					return true;
				}
			}
			return false;
		};

	return TryActivateByTag(LabGameplayTags::GameplayAbility_HitReaction)
		|| TryActivateByTag(LabGameplayTags::Action_HitReact);
}

UAnimMontage* UEnemyTrainingBotComponent::ResolveHitReactMontage() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	const UEquipmentComponent* Equipment =
		Enemy ? Enemy->GetEquipmentComponent() : nullptr;
	if (Equipment)
	{
		FHitReactData HitReactData;
		if (Equipment->GetHitReactData(HitReactData)
			&& HitReactData.HitReactMontage)
		{
			return HitReactData.HitReactMontage;
		}
	}
	return nullptr;
}

void UEnemyTrainingBotComponent::PlayHitReactMontageLocal() const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	USkeletalMeshComponent* Mesh = Enemy ? Enemy->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = ResolveHitReactMontage();
	if (AnimInstance && Montage)
	{
		AnimInstance->Montage_Play(Montage);
	}
}

void UEnemyTrainingBotComponent::PlayUnequipMontageLocal(
	UAnimMontage* UnequipMontage,
	const float PlayRate) const
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	UAnimInstance* AnimInstance =
		Enemy && Enemy->GetMesh()
			? Enemy->GetMesh()->GetAnimInstance()
			: nullptr;
	if (!AnimInstance || !UnequipMontage)
	{
		return;
	}

	AnimInstance->Montage_Stop(0.08f);
	AnimInstance->Montage_Play(UnequipMontage, PlayRate);
}

bool UEnemyTrainingBotComponent::RequestWeaponChange(
	const UItemDefinition* WeaponDefinition)
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority() || !WeaponDefinition)
	{
		return false;
	}

	UEquipmentComponent* Equipment = Enemy->GetEquipmentComponent();
	UEnemyCombatComponent* Combat = Enemy->GetEnemyCombatComponent();
	if (!Equipment || !Combat)
	{
		return false;
	}

	if (Equipment->GetCurrentWeaponActor()
		&& Equipment->GetCurrentWeaponDefinition() == WeaponDefinition
		&& !bWeaponChangeInProgress)
	{
		FEnemyCombatSettings UpdatedSettings = Combat->GetSettings();
		UpdatedSettings.StartingWeaponDefinition =
			TSoftObjectPtr<UItemDefinition>(
				FSoftObjectPath(WeaponDefinition->GetPathName()));
		Combat->ApplySettings(UpdatedSettings);
		return true;
	}

	PendingWeaponDefinition = WeaponDefinition;
	bPendingUnarmed = false;
	bWeaponChangeInProgress = true;
	CancelWeaponChangeAttackState();

	float UnequipDuration = 0.0f;
	PlayCurrentUnequipMontage(UnequipDuration);
	Enemy->GetWorldTimerManager().ClearTimer(WeaponChangeTimerHandle);
	if (UnequipDuration <= UE_KINDA_SMALL_NUMBER)
	{
		FinishPendingWeaponChange();
		return true;
	}

	Enemy->GetWorldTimerManager().SetTimer(
		WeaponChangeTimerHandle,
		this,
		&ThisClass::FinishPendingWeaponChange,
		UnequipDuration,
		false);
	return true;
}

bool UEnemyTrainingBotComponent::RequestUnarmed()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return false;
	}

	UEquipmentComponent* Equipment = Enemy->GetEquipmentComponent();
	UEnemyCombatComponent* Combat = Enemy->GetEnemyCombatComponent();
	if (!Equipment || !Combat)
	{
		return false;
	}

	if (!Equipment->GetCurrentWeaponActor()
		&& !Equipment->GetCurrentWeaponDefinition()
		&& !bWeaponChangeInProgress)
	{
		Combat->ClearStartingWeaponDefinition();
		return true;
	}

	PendingWeaponDefinition = nullptr;
	bPendingUnarmed = true;
	bWeaponChangeInProgress = true;
	CancelWeaponChangeAttackState();

	float UnequipDuration = 0.0f;
	PlayCurrentUnequipMontage(UnequipDuration);
	Enemy->GetWorldTimerManager().ClearTimer(WeaponChangeTimerHandle);
	if (UnequipDuration <= UE_KINDA_SMALL_NUMBER)
	{
		FinishPendingWeaponChange();
		return true;
	}

	Enemy->GetWorldTimerManager().SetTimer(
		WeaponChangeTimerHandle,
		this,
		&ThisClass::FinishPendingWeaponChange,
		UnequipDuration,
		false);
	return true;
}

void UEnemyTrainingBotComponent::CancelWeaponChangeAttackState(
	const float BlendOutTime)
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	if (AAIController* AIController =
			Cast<AAIController>(Enemy->GetController()))
	{
		AIController->StopMovement();
	}

	if (UPdAbilitySystemComponent* AbilitySystemComponent =
			Enemy->GetEnemyAbilitySystemComponent())
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(LabGameplayTags::Action_Attack);
		CancelTags.AddTag(LabGameplayTags::Action_Punch);
		CancelTags.AddTag(LabGameplayTags::Action_RangedAttack);
		AbilitySystemComponent->CancelAbilities(&CancelTags);
	}

	if (UAnimInstance* AnimInstance =
			Enemy->GetMesh()
				? Enemy->GetMesh()->GetAnimInstance()
				: nullptr)
	{
		AnimInstance->Montage_Stop(BlendOutTime);
	}

	UEquipmentComponent* Equipment = Enemy->GetEquipmentComponent();
	if (AWeaponBase* Weapon =
			Equipment ? Equipment->GetCurrentWeaponActor() : nullptr)
	{
		Weapon->StopAttackTrace();
		Weapon->StopWeaponMontage(BlendOutTime);
	}
}

bool UEnemyTrainingBotComponent::PlayCurrentUnequipMontage(
	float& OutDuration)
{
	OutDuration = 0.0f;
	AEnemyBase* Enemy = GetEnemyOwner();
	UEquipmentComponent* Equipment =
		Enemy ? Enemy->GetEquipmentComponent() : nullptr;
	if (!Enemy
		|| !Equipment
		|| (!Equipment->GetCurrentWeaponActor()
			&& !Equipment->GetCurrentWeaponDefinition()))
	{
		return false;
	}

	FUnequipData UnequipData;
	if (!Equipment->GetUnequipData(UnequipData)
		|| !UnequipData.UnequipMontage)
	{
		return false;
	}

	constexpr float PlayRate = 1.0f;
	OutDuration = FMath::Max(
		UnequipData.UnequipMontage->GetPlayLength() / PlayRate,
		0.0f);
	Enemy->MulticastPlayTrainingBotUnequipMontage(
		UnequipData.UnequipMontage,
		PlayRate);
	return true;
}

void UEnemyTrainingBotComponent::FinishPendingWeaponChange()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	Enemy->GetWorldTimerManager().ClearTimer(WeaponChangeTimerHandle);

	UEquipmentComponent* Equipment = Enemy->GetEquipmentComponent();
	UEnemyCombatComponent* Combat = Enemy->GetEnemyCombatComponent();
	if (!Equipment || !Combat)
	{
		PendingWeaponDefinition = nullptr;
		bPendingUnarmed = false;
		bWeaponChangeInProgress = false;
		return;
	}

	if (Equipment->GetCurrentWeaponActor()
		|| Equipment->GetCurrentWeaponDefinition())
	{
		Equipment->UnequipCurrentWeapon();
	}
	Enemy->ResetAnimationToDefault();

	const UItemDefinition* RequestedWeaponDefinition =
		PendingWeaponDefinition.Get();
	const bool bRequestedUnarmed =
		bPendingUnarmed || !RequestedWeaponDefinition;
	if (bRequestedUnarmed)
	{
		Combat->ClearStartingWeaponDefinition();
	}
	else
	{
		Combat->EquipEnemyWeaponDefinition(RequestedWeaponDefinition);
	}

	PendingWeaponDefinition = nullptr;
	bPendingUnarmed = false;
	bWeaponChangeInProgress = false;
}

void UEnemyTrainingBotComponent::CacheRespawnTransform()
{
	const AEnemyBase* Enemy = GetEnemyOwnerConst();
	if (!Enemy || bHasRespawnTransform)
	{
		return;
	}

	RespawnTransform = Enemy->GetActorTransform();
	bHasRespawnTransform = true;
}

void UEnemyTrainingBotComponent::ScheduleRespawn()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy
		|| !Enemy->HasAuthority()
		|| bRespawnResetInProgress
		|| bRespawnScheduled
		|| Enemy->GetWorldTimerManager().IsTimerActive(RespawnTimerHandle))
	{
		return;
	}

	CacheRespawnTransform();
	if (AAIController* AIController =
			Cast<AAIController>(Enemy->GetController()))
	{
		AIController->StopMovement();
	}

	Enemy->SetAttackTarget(nullptr);
	if (UEnemyCombatComponent* Combat =
			Enemy->GetEnemyCombatComponent())
	{
		Combat->ShutdownRuntime();
	}

	Enemy->GetWorldTimerManager().ClearTimer(HitStunTimerHandle);
	bHitStunned = false;

	Enemy->GetWorldTimerManager().ClearTimer(RespawnTimerHandle);
	if (Settings.RespawnDelay <= 0.0f)
	{
		Respawn();
		return;
	}

	Enemy->StartDeathDissolve(Settings.RespawnDelay);
	bRespawnScheduled = true;
	Enemy->GetWorldTimerManager().SetTimer(
		RespawnTimerHandle,
		this,
		&ThisClass::Respawn,
		Settings.RespawnDelay,
		false);
}

void UEnemyTrainingBotComponent::Respawn()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	bRespawnScheduled = false;
	TGuardValue<bool> RespawnGuard(bRespawnResetInProgress, true);

	CacheRespawnTransform();
	UEnemyCombatComponent* Combat = Enemy->GetEnemyCombatComponent();
	const UItemDefinition* PreservedWeaponDefinition =
		Combat
			? Combat->GetCurrentOrStartingEnemyWeaponDefinition()
			: nullptr;

	ResetRuntimeStateForRespawn();
	if (Combat)
	{
		Combat->ResetAttributesForRespawn();
	}

	Enemy->SetActorTransform(
		RespawnTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	Enemy->MulticastResetTrainingBotRespawnVisuals(RespawnTransform);

	Enemy->InitializeBehaviorTreeCombat();
	if (PreservedWeaponDefinition)
	{
		RequestWeaponChange(PreservedWeaponDefinition);
	}
	else
	{
		RequestUnarmed();
	}

	if (ATrainingBotAIController* TrainingController =
			Cast<ATrainingBotAIController>(Enemy->GetController()))
	{
		TrainingController->RefreshTargetFromPlayers();
	}
	Enemy->ForceNetUpdate();
}

void UEnemyTrainingBotComponent::ResetRuntimeStateForRespawn()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy || !Enemy->HasAuthority())
	{
		return;
	}

	if (UEnemyCombatComponent* Combat =
			Enemy->GetEnemyCombatComponent())
	{
		Combat->ShutdownRuntime();
	}
	Enemy->GetWorldTimerManager().ClearTimer(HitStunTimerHandle);
	Enemy->GetWorldTimerManager().ClearTimer(WeaponChangeTimerHandle);

	CleanupArrowProjectilesForRespawn();
	if (AAIController* AIController =
			Cast<AAIController>(Enemy->GetController()))
	{
		AIController->StopMovement();
	}

	Enemy->SetAttackTarget(nullptr);
	bHitStunned = false;
	PreHitStunMovementMode = MOVE_Walking;
	PreHitStunCustomMovementMode = 0;
	PendingWeaponDefinition = nullptr;
	bPendingUnarmed = false;
	bWeaponChangeInProgress = false;

	if (UPdAbilitySystemComponent* AbilitySystemComponent =
			Enemy->GetEnemyAbilitySystemComponent())
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	if (UAnimInstance* AnimInstance =
			Enemy->GetMesh()
				? Enemy->GetMesh()->GetAnimInstance()
				: nullptr)
	{
		AnimInstance->Montage_Stop(0.0f);
	}

	UEquipmentComponent* Equipment = Enemy->GetEquipmentComponent();
	if (AWeaponBase* Weapon =
			Equipment ? Equipment->GetCurrentWeaponActor() : nullptr)
	{
		Weapon->StopAttackTrace();
		Weapon->StopWeaponMontage(0.0f);
	}

	if (Equipment
		&& (Equipment->GetCurrentWeaponActor()
			|| Equipment->GetCurrentWeaponDefinition()
			|| Equipment->GetCurrentWeaponId().IsValid()))
	{
		Equipment->UnequipCurrentWeapon();
	}

	Enemy->ResetAnimationToDefault();
	if (UEnemyCombatComponent* Combat =
			Enemy->GetEnemyCombatComponent())
	{
		Combat->ClearStartingWeaponDefinition();
	}
}

void UEnemyTrainingBotComponent::CleanupArrowProjectilesForRespawn()
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy)
	{
		return;
	}

	TArray<AArrowProjectileBase*> ArrowProjectiles;
	const auto AddArrowProjectile =
		[&ArrowProjectiles](AActor* Actor)
		{
			AArrowProjectileBase* ArrowProjectile =
				Cast<AArrowProjectileBase>(Actor);
			if (IsValid(ArrowProjectile))
			{
				ArrowProjectiles.AddUnique(ArrowProjectile);
			}
		};

	TArray<AActor*> AttachedActors;
	Enemy->GetAttachedActors(AttachedActors, true, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		AddArrowProjectile(AttachedActor);
	}

	if (UWorld* World = Enemy->GetWorld())
	{
		for (TActorIterator<AArrowProjectileBase> ArrowIt(World);
			ArrowIt;
			++ArrowIt)
		{
			AArrowProjectileBase* ArrowProjectile = *ArrowIt;
			if (!IsValid(ArrowProjectile))
			{
				continue;
			}

			if (ArrowProjectile->GetAttachParentActor() == Enemy
				|| ArrowProjectile->GetOwner() == Enemy
				|| ArrowProjectile->GetInstigator() == Enemy)
			{
				AddArrowProjectile(ArrowProjectile);
			}
		}
	}

	for (AArrowProjectileBase* ArrowProjectile : ArrowProjectiles)
	{
		if (!IsValid(ArrowProjectile))
		{
			continue;
		}

		if (Enemy->HasAuthority()
			|| !ArrowProjectile->GetIsReplicated())
		{
			ArrowProjectile->Destroy();
		}
		else
		{
			ArrowProjectile->SetActorHiddenInGame(true);
		}
	}
}

void UEnemyTrainingBotComponent::ResetRespawnVisualsLocal(
	const FTransform& InRespawnTransform)
{
	AEnemyBase* Enemy = GetEnemyOwner();
	if (!Enemy)
	{
		return;
	}

	CleanupArrowProjectilesForRespawn();
	if (!Enemy->HasAuthority())
	{
		Enemy->SetActorTransform(
			InRespawnTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	Enemy->ResetDeathStateForRespawn();
}
