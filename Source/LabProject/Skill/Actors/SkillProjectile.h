#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/NetSerialization.h"
#include "Skill/Actors/SkillProjectilePresentation.h"
#include "SkillProjectile.generated.h"

class UProjectileMovementComponent;
class UPrimitiveComponent;
class USphereComponent;
class UGameplayEffect;
class ASkillEffectArea;
class ACharacterBase;
class UNiagaraComponent;
class UNiagaraSystem;
class UStatusEffectDefinition;

DECLARE_MULTICAST_DELEGATE_TwoParams(FProjectileSkillImpact, AActor*, const FHitResult&);

/**
 * 스킬 투사체의 비행·충돌·연출 상태를 복제하고 순서를 조율한다.
 * 비행 계산은 PdSkillProjectileFlight, 충돌 판정과 피해는 PdSkillProjectileHit,
 * 이펙트와 대기 성장 연출은 PdSkillProjectilePresentation이 맡는다.
 */
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API ASkillProjectile : public AActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	ASkillProjectile();

	void InitializeProjectile(const FVector& InTargetLocation, float InSpeed, const FGameplayEffectSpecHandle& InDamageEffectSpecHandle);

	void PrepareProjectile(const FGameplayEffectSpecHandle& InDamageEffectSpecHandle);

	void PrepareCosmeticReadiedProjectile(float InLifeSpan = 0.0f);

	void StartReadiedScaleGrowth(
		FVector InStartScale,
		FVector InTargetScale,
		float InDuration,
		FName InNiagaraVector2DParameterName,
		FVector2D InNiagaraStartSize,
		FVector2D InNiagaraTargetSize);

	float GetReadiedScaleGrowthAlpha() const;

	void LaunchProjectile(const FVector& InTargetLocation, float InSpeed, const FGameplayEffectSpecHandle& InDamageEffectSpecHandle);

	void ConfigureArcTrajectory(bool bInUseArcTrajectory, float InArcHeight, float InArcGravityScale);

	void SetDebuffEffectSpecHandle(
		const FGameplayEffectSpecHandle& InDebuffEffectSpecHandle,
		UStatusEffectDefinition* InStatusEffectDefinition);

	void SetImpactAreaDamageRadius(float InImpactAreaDamageRadius);

	void ConfigureImpactPersistence(bool bInStickOnImpact, float InPostImpactLifeSpan);

	void ConfigureProjectileVisuals(
		UNiagaraSystem* InMuzzleFX,
		UNiagaraSystem* InProjectileFX,
		UNiagaraSystem* InHitFX,
		bool bInSpawnHitNiagaraOnGround,
		FGameplayTag InSpawnGameplayCueTag,
		FGameplayTag InImpactGameplayCueTag);

protected:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(NetMulticast, Reliable)
	void MulticastExecuteImpactGameplayCue(
		FVector_NetQuantize CueLocation,
		UNiagaraSystem* InHitFX,
		bool bInSpawnHitNiagaraOnGround,
		FGameplayTag InImpactGameplayCueTag,
		bool bKeepProjectileVisual,
		ACharacterBase* InStuckCharacter,
		FName InStuckBoneName);

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_ProjectileFlightData();

	UFUNCTION()
	void OnRep_ProjectileVisuals();

	UFUNCTION()
	void OnRep_ReadiedScaleGrowth();

	UFUNCTION()
	void OnRep_ImpactState();

	UFUNCTION()
	void HandleSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSphereHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION()
	void HandleProjectileStopped(const FHitResult& Hit);

	void HandleImpact(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FHitResult& Hit);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ResetImpactState();
	void StartProjectileMovement() const;
	void StopAtImpact(const FVector& ImpactLocation);
	void AttachToImpactComponent(UPrimitiveComponent* OtherComp, FName ImpactBoneName);
	void ExecuteImpactGameplayCueAtLocation(const FVector& CueLocation);
	void ApplyProjectileLoopVisual() const;
	void ExecuteImpactNiagaraAtLocation(const FVector& CueLocation);
	void StopReadiedScaleGrowth();
	void UpdateReadiedScaleGrowth();
	float GetSyncedWorldTimeSeconds() const;
	void ApplyReadiedGrowthValue(float Alpha);
	void MarkProjectileFlightDataDirty();
	void MarkProjectileVisualsDirty();
	void MarkReadiedScaleGrowthDirty();
	void MarkImpactStateDirty();

public:
	FProjectileSkillImpact OnSkillImpact;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Projectile|Components")
	TObjectPtr<USphereComponent> SphereCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Projectile|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Projectile|Components")
	TObjectPtr<UNiagaraComponent> ProjectileEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileFlightData, Category = "!Projectile")
	FVector_NetQuantize TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileFlightData, Category = "!Projectile", meta = (ClampMin = "0.0"))
	float Speed = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileFlightData, Category = "!Projectile|Trajectory")
	bool bUseArcTrajectory = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileFlightData, Category = "!Projectile|Trajectory", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ArcHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileFlightData, Category = "!Projectile|Trajectory", meta = (ClampMin = "0.0"))
	float ArcGravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Damage")
	FGameplayEffectSpecHandle DamageEffectSpecHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Debuff")
	FGameplayEffectSpecHandle DebuffEffectSpecHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Debuff")
	TObjectPtr<UStatusEffectDefinition> StatusEffectDefinition;

	UPROPERTY(Transient)
	float ImpactAreaDamageRadius = 0.0f;

	UPROPERTY(Transient)
	bool bStickOnImpact = false;

	UPROPERTY(Transient)
	float PostImpactLifeSpan = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileVisuals, Category = "!Projectile|VFX")
	TObjectPtr<UNiagaraSystem> MuzzleFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileVisuals, Category = "!Projectile|VFX")
	TObjectPtr<UNiagaraSystem> ProjectileFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileVisuals, Category = "!Projectile|VFX")
	TObjectPtr<UNiagaraSystem> HitFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "!Projectile|VFX")
	bool bSpawnHitNiagaraOnGround = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "!Projectile|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag SpawnGameplayCueTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "!Projectile|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag ImpactGameplayCueTag;

	UPROPERTY(ReplicatedUsing = OnRep_ReadiedScaleGrowth, Transient)
	FSkillProjectileGrowth ReadiedGrowth;

	UPROPERTY(ReplicatedUsing = OnRep_ImpactState, Transient)
	bool bHasImpacted = false;

	UPROPERTY(Replicated, Transient)
	bool bKeepProjectileVisualAfterImpact = false;

	UPROPERTY(Transient)
	bool bImpactCueExecuted = false;

	UPROPERTY(Transient)
	bool bImpactNiagaraExecuted = false;

	UPROPERTY(Transient)
	bool bCosmeticOnly = false;
};
