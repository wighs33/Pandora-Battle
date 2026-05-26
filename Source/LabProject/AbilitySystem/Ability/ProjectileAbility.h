#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Common/WeaponDefinitionData.h"
#include "Engine/CollisionProfile.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "ProjectileAbility.generated.h"

class AProjectileBase;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitConfirmCancel;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitTargetData;
class UGameplayEffect;

UCLASS(Blueprintable)
class LABPROJECT_API UProjectileAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UProjectileAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!Ability|Projectile")
	FVector GetSpawnLocation() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "!Ability|Projectile")
	void ShootProjectile(FVector TargetLocation);

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	UFUNCTION()
	void HandleMontageFinished();

	UFUNCTION()
	void HandleShootProjectileEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleConfirmPressed();

	UFUNCTION()
	void HandleCancelPressed();

	UFUNCTION()
	void HandleTargetDataValid(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void HandleTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);

	void StartPlayerAiming();
	void BeginConfirmedShot();
	void ConfirmPlayerShot();
	void StartShootProjectileEventTask();
	void WaitForPlayerTargetData();
	bool ShouldUseAimFallbackForTarget(const FVector& TargetLocation) const;
	bool TryResolveProjectileAimTargetLocation(FVector& OutTargetLocation) const;
	FVector ResolveFallbackTargetLocation() const;
	FGameplayEffectSpecHandle MakeDamageEffectSpec() const;
	UAnimMontage* GetConfiguredShootMontage() const;
	TSubclassOf<AProjectileBase> GetConfiguredProjectileClass() const;
	TSubclassOf<UGameplayEffect> GetConfiguredDamageEffectClass() const;
	float GetConfiguredProjectileSpeed() const;
	FGameplayTag GetConfiguredDamageDataTag() const;
	FGameplayTag GetConfiguredShootProjectileEventTag() const;
	float GetConfiguredTargetTraceMaxRange() const;
	FCollisionProfileName GetConfiguredTargetTraceProfile() const;
	float GetConfiguredMinimumTargetDistanceFromSpawn() const;
	bool GetConfiguredTraceAffectsAimPitch() const;
	bool GetConfiguredDrawTargetTraceDebug() const;
	FName GetConfiguredSpawnSocketName() const;
	FVector GetConfiguredSpawnLocationOffset() const;
	float GetConfiguredMinimumForwardSpawnOffset() const;
	FWeaponAimCameraSettings GetConfiguredProjectileCameraSettings() const;
	FGameplayTag GetConfiguredProjectileCrosshairWidgetTag() const;
	float GetConfiguredProjectileAimReleaseDelay() const;
	bool HasPlayerController() const;
	bool IsLocallyControlledPlayer() const;
	void ApplyProjectileAimCamera(bool bEnabled) const;
	void ReleaseProjectileAimAnimationState() const;
	void ShowProjectileCrosshair(bool bEnabled) const;
	void PauseProjectileMontageForAiming();
	void ResumeProjectileMontageAfterAiming();
	void StartProjectileAimReleaseDelay();
	void ReleaseProjectileAimState();
	void CleanupAimingState();
	void EndAbilityFromActivation(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo);

	UPROPERTY(Transient)
	bool bEndAfterProjectileFired = false;

	UPROPERTY(Transient)
	bool bWaitingForPlayerConfirm = false;

	UPROPERTY(Transient)
	bool bPausedForPlayerAim = false;

	UPROPERTY(Transient)
	bool bPlayerProjectileConfirmed = false;

	UPROPERTY(Transient)
	bool bWaitingForProjectileAimRelease = false;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitConfirmCancel> ConfirmCancelTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ShootProjectileEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ShootMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;

	FTimerHandle ProjectileAimReleaseTimerHandle;
};
