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
class AGameplayAbilityTargetActor;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitConfirmCancel;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitTargetData;
class UGameplayEffect;
class UMaterialInterface;
class UStatusEffectDefinition;

UCLASS(Blueprintable)
class LABPROJECT_API UProjectileAbility : public UPdGameplayAbility
{
	GENERATED_BODY()

public:
	UProjectileAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool ShouldAutoConfirmOnInputRelease() const override { return false; }

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

	bool TryValidateServerProjectileTargetLocation(
		const FHitResult& ClientHitResult,
		const FVector& TargetDataEndPoint,
		bool bUsingGroundTargeting,
		FVector& OutValidatedLocation) const;
	void StartPlayerAiming();
	void BeginConfirmedShot();
	void ConfirmPlayerShot();
	bool ExecuteProjectileShot(FVector TargetLocation);
	void ExecuteFallbackProjectileShot();
	AProjectileBase* SpawnReadiedProjectile();
	void DestroyReadiedProjectile();
	bool TryStartSocketBarrage(FVector TargetLocation);
	AProjectileBase* SpawnPreparedSocketBarrageProjectile(FName SocketName);
	void LaunchSocketBarrageProjectile(AProjectileBase* Projectile);
	void SpawnSocketBarrageProjectileCosmetic(
		TSubclassOf<AProjectileBase> ProjectileClass,
		const FVector& SpawnLocation,
		const FRotator& SpawnRotation,
		const FVector& TargetLocation,
		float ProjectileSpeed,
		int32 SocketIndex) const;
	void ApplySocketBarrageProjectileLaunchScale(AProjectileBase* Projectile, int32 SocketIndex) const;
	void ResolveSocketBarrageProjectileLaunchScale(
		int32 SocketIndex,
		FVector& OutScale,
		FName& OutNiagaraVector2DParameterName,
		FVector2D& OutNiagaraSize) const;
	float ResolveSocketBarrageCosmeticLifeSpan(const FVector& SpawnLocation, const FVector& TargetLocation, float ProjectileSpeed) const;
	FVector ResolveSocketBarrageLaunchTargetLocation(const FVector& ProjectileLocation) const;
	FVector ResolveCharacterTargetSocketProjectileTargetLocation(const FVector& FromLocation) const;
	void FireNextSocketBarrageProjectile();
	void ClearSocketBarrageState(bool bDestroyPendingProjectiles);
	bool IsSocketBarrageActive() const;
	bool ShouldWaitForServerSocketBarrageEnd() const;
	void StartShootProjectileEventTask();
	void WaitForPlayerTargetData();
	bool ShouldRetargetUsingAim(const FVector& TargetLocation) const;
	bool TryResolveProjectileAimTargetLocation(FVector& OutTargetLocation) const;
	FVector ResolveDefaultTargetLocation() const;
	FGameplayEffectSpecHandle MakeDamageEffectSpec(float ChargeDamageAlpha = 1.0f) const;
	FGameplayEffectSpecHandle MakeStatusEffectSpec() const;
	UAnimMontage* GetConfiguredShootMontage() const;
	TSubclassOf<AProjectileBase> GetConfiguredProjectileClass() const;
	TSubclassOf<UGameplayEffect> GetConfiguredDamageEffectClass() const;
	const UStatusEffectDefinition* GetConfiguredStatusEffectDataAsset() const;
	TSubclassOf<UGameplayEffect> GetConfiguredStatusEffectClass() const;
	float GetConfiguredStatusEffectLevel() const;
	float GetConfiguredStatusEffectDuration() const;
	float GetConfiguredProjectileSpeed() const;
	float GetConfiguredProjectileRadius() const;
	bool ShouldUseConfiguredProjectileArcTrajectory() const;
	float GetConfiguredProjectileArcHeight() const;
	float GetConfiguredProjectileArcGravityScale() const;
	FGameplayTag GetConfiguredDamageDataTag() const;
	FGameplayTag GetConfiguredShootProjectileEventTag() const;
	bool IsConfiguredImmediateFireMode() const;
	TArray<FName> GetConfiguredProjectileSocketNames() const;
	float GetConfiguredProjectileSocketFireInterval() const;
	void GetConfiguredProjectileVisuals(
		UNiagaraSystem*& OutMuzzleFX,
		UNiagaraSystem*& OutProjectileFX,
		UNiagaraSystem*& OutHitFX,
		bool& bOutSpawnHitNiagaraOnGround,
		FGameplayTag& OutSpawnGameplayCueTag,
		FGameplayTag& OutImpactGameplayCueTag) const;
	void ApplyConfiguredProjectileVisuals(AProjectileBase* Projectile) const;
	void ApplyConfiguredProjectileTrajectory(AProjectileBase* Projectile) const;
	void ApplyConfiguredProjectileImpactPersistence(AProjectileBase* Projectile) const;
	float GetConfiguredTargetTraceMaxRange() const;
	FCollisionProfileName GetConfiguredTargetTraceProfile() const;
	float GetConfiguredMinimumTargetDistanceFromSpawn() const;
	bool GetConfiguredTraceAffectsAimPitch() const;
	bool GetConfiguredDrawTargetTraceDebug() const;
	FName GetConfiguredSpawnSocketName() const;
	FVector GetSpawnLocationForSocket(FName SocketName) const;
	FVector GetConfiguredSpawnLocationOffset() const;
	float GetConfiguredMinimumForwardSpawnOffset() const;
	bool IsConfiguredReadiedProjectileChargeGrowthEnabled() const;
	FVector GetConfiguredReadiedProjectileStartScale() const;
	FVector GetConfiguredReadiedProjectileTargetScale() const;
	float GetConfiguredReadiedProjectileScaleDuration() const;
	FName GetConfiguredReadiedProjectileNiagaraVector2DParameterName() const;
	FVector2D GetConfiguredReadiedProjectileNiagaraStartSize() const;
	FVector2D GetConfiguredReadiedProjectileNiagaraTargetSize() const;
	void ApplyConfiguredStatusEffect(AProjectileBase* Projectile) const;
	void ApplyReadiedProjectileScaleGrowth(AProjectileBase* Projectile) const;
	bool ShouldUseGroundTargeting() const;
	TSubclassOf<AGameplayAbilityTargetActor> GetConfiguredGroundTargetActorClass() const;
	float GetConfiguredGroundTargetingMaxRange() const;
	FCollisionProfileName GetConfiguredGroundTargetingTraceProfile() const;
	float GetConfiguredGroundTargetingTraceStartHeight() const;
	float GetConfiguredGroundTargetingTraceDepth() const;
	float GetConfiguredGroundTargetingCollisionRadius() const;
	float GetConfiguredGroundTargetingCollisionHeight() const;
	bool GetConfiguredGroundTargetingTraceAffectsAimPitch() const;
	bool GetConfiguredDrawGroundTargetingDebug() const;
	UMaterialInterface* GetConfiguredGroundTargetingDecal() const;
	float GetConfiguredGroundTargetingDecalSize() const;
	float GetConfiguredGroundTargetingDecalFinalSize() const;
	bool ShouldGrowConfiguredGroundTargetingDecal() const;
	FLinearColor GetConfiguredGroundTargetingDecalColor() const;
	float CalculateConfiguredImpactAreaDamageRadius(float ChargeDamageAlpha) const;
	bool TryBuildGroundTargetingDecalGrowth(float& OutStartSize, float& OutTargetSize, float& OutDuration) const;
	bool HasPlayerController() const;
	void PauseProjectileMontageForAiming();
	void ResumeProjectileMontageAfterAiming();
	void CleanupAimingState();
	void EndProjectileAbilityAfterResolvedShot();

	UPROPERTY(Transient)
	bool bEndAfterProjectileFired = false;

	UPROPERTY(Transient)
	bool bWaitingForPlayerConfirm = false;

	UPROPERTY(Transient)
	bool bPausedForPlayerAim = false;

	UPROPERTY(Transient)
	bool bPlayerProjectileConfirmed = false;

	UPROPERTY(Transient)
	bool bProjectileExecutionRequested = false;

	UPROPERTY(Transient)
	bool bProjectileSpawnSucceeded = false;

	UPROPERTY(Transient)
	bool bCleaningUpTargetDataTask = false;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitConfirmCancel> ConfirmCancelTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ShootProjectileEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ShootMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitTargetData> TargetDataTask;

	UPROPERTY(Transient)
	TObjectPtr<AProjectileBase> ReadiedProjectile;

	UPROPERTY(Transient)
	TArray<FName> SocketBarrageSocketNames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AProjectileBase>> SocketBarrageProjectiles;

	UPROPERTY(Transient)
	FVector SocketBarrageTargetLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bSocketBarrageUseCharacterTargetSocket = false;

	FTimerHandle SocketBarrageTimerHandle;
	int32 NextSocketBarrageProjectileIndex = 0;
	bool bSocketBarrageEndAbilityAfterFire = false;

};
