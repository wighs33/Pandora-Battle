#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/NetSerialization.h"
#include "ProjectileBase.generated.h"

class UProjectileMovementComponent;
class UPrimitiveComponent;
class USphereComponent;
class UGameplayEffect;
class UAbilitySystemComponent;
class AEffectAreaBase;
class ACharacterBase;
class UNiagaraComponent;
class UNiagaraSystem;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AProjectileBase();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!Projectile")
	void InitializeProjectile(const FVector& InTargetLocation, float InSpeed, const FGameplayEffectSpecHandle& InDamageEffectSpecHandle);

	void InitializeCosmeticProjectile(const FVector& InTargetLocation, float InSpeed, float InLifeSpan);

	UFUNCTION(BlueprintCallable, Category = "!Projectile")
	void PrepareProjectile(const FGameplayEffectSpecHandle& InDamageEffectSpecHandle);

	void PrepareCosmeticReadiedProjectile(float InLifeSpan = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "!Projectile|Readied")
	void StartReadiedScaleGrowth(
		FVector InStartScale,
		FVector InTargetScale,
		float InDuration,
		FName InNiagaraVector2DParameterName,
		FVector2D InNiagaraStartSize,
		FVector2D InNiagaraTargetSize);

	UFUNCTION(BlueprintCallable, Category = "!Projectile|Readied")
	float GetReadiedScaleGrowthAlpha() const;

	UFUNCTION(BlueprintCallable, Category = "!Projectile")
	void LaunchProjectile(const FVector& InTargetLocation, float InSpeed, const FGameplayEffectSpecHandle& InDamageEffectSpecHandle);

	UFUNCTION(BlueprintCallable, Category = "!Projectile|Trajectory")
	void ConfigureArcTrajectory(bool bInUseArcTrajectory, float InArcHeight, float InArcGravityScale);

	UFUNCTION(BlueprintCallable, Category = "!Projectile|Debuff")
	void SetDebuffEffectSpecHandle(const FGameplayEffectSpecHandle& InDebuffEffectSpecHandle);

	UFUNCTION(BlueprintCallable, Category = "!Projectile|Impact")
	void SetImpactEffectAreaSpawnConfigs(
		const TArray<FProjectileImpactEffectAreaSpawnConfig>& InImpactEffectAreaSpawnConfigs,
		int32 InSourceSkillLevel);

	void SetImpactAreaDamageRadius(float InImpactAreaDamageRadius);

	UFUNCTION(BlueprintCallable, Category = "!Projectile|Impact")
	void ConfigureImpactPersistence(bool bInStickOnImpact, float InPostImpactLifeSpan);

	UFUNCTION(BlueprintCallable, Category = "!Projectile|VFX")
	void ConfigureProjectileVisuals(
		UNiagaraSystem* InMuzzleFX,
		UNiagaraSystem* InProjectileFX,
		UNiagaraSystem* InHitFX,
		bool bInSpawnHitNiagaraOnGround,
		FGameplayTag InSpawnGameplayCueTag,
		FGameplayTag InImpactGameplayCueTag);

	UNiagaraComponent* GetProjectileEffectComponent() const { return ProjectileEffect; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;

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

	void StartProjectileMovement() const;
	FVector CalculateArcLaunchVelocity() const;
	void ConfigureCollision() const;
	void DisableProjectileCollision() const;
	void ConfigureIgnoredActors() const;
	void HandleImpact(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FHitResult& Hit);
	void StopAtImpact(const FVector& ImpactLocation);
	FName ResolveImpactBoneName(
		const UPrimitiveComponent* ImpactComponent,
		const FHitResult& Hit,
		const FVector& ImpactLocation) const;
	void AttachToImpactComponent(UPrimitiveComponent* OtherComp, FName ImpactBoneName);
	bool TryApplyDamageToTarget(AActor* TargetActor);
	bool TryApplyDamageInImpactArea(const FVector& ImpactLocation);
	bool TryApplyDebuffToTarget(AActor* TargetActor, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC) const;
	void TrySpawnImpactEffectAreas(AActor* TargetActor, bool bDamageApplied);
	bool ResolveImpactEffectAreaSpawnTransform(
		const FProjectileImpactEffectAreaSpawnConfig& SpawnConfig,
		AActor* TargetActor,
		FTransform& OutSpawnTransform) const;
	AActor* ResolveDamageTargetActor(AActor* OtherActor, const UPrimitiveComponent* OtherComponent) const;
	bool IsIgnoredImpactActor(const AActor* OtherActor) const;
	void ExecuteSpawnGameplayCue() const;
	void ExecuteImpactGameplayCue();
	void ExecuteImpactGameplayCueAtLocation(const FVector& CueLocation);
	void ApplyProjectileLoopVisual() const;
	void ApplyProjectileEffectSystem(UNiagaraSystem* DesiredSystem) const;
	void ExecuteImpactNiagaraAtLocation(const FVector& CueLocation);
	FTransform ResolveImpactNiagaraSpawnTransform(const FVector& CueLocation) const;
	void StopReadiedScaleGrowth();
	void UpdateReadiedScaleGrowth();
	float GetSyncedWorldTimeSeconds() const;
	void ApplyReadiedGrowthValue(float Alpha);
	void SetReadiedNiagaraVector2DParameter(FVector2D Value) const;
	FName GetNormalizedReadiedNiagaraParameterName() const;
	void MarkProjectileFlightDataDirty();
	void MarkProjectileVisualsDirty();
	void MarkReadiedScaleGrowthDirty();
	void MarkImpactStateDirty();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExecuteImpactGameplayCue(
		FVector_NetQuantize CueLocation,
		UNiagaraSystem* InHitFX,
		bool bInSpawnHitNiagaraOnGround,
		FGameplayTag InImpactGameplayCueTag,
		bool bKeepProjectileVisual,
		ACharacterBase* InStuckCharacter,
		FName InStuckBoneName);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Impact")
	TArray<FProjectileImpactEffectAreaSpawnConfig> ImpactEffectAreaSpawnConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Impact", meta = (ClampMin = "1"))
	int32 SourceSkillLevel = 1;

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
	bool bReadiedScaleGrowthActive = false;

	UPROPERTY(Replicated, Transient)
	FVector ReadiedScaleGrowthStartScale = FVector::OneVector;

	UPROPERTY(Replicated, Transient)
	FVector ReadiedScaleGrowthTargetScale = FVector::OneVector;

	UPROPERTY(Replicated, Transient)
	float ReadiedScaleGrowthDuration = 0.0f;

	UPROPERTY(Replicated, Transient)
	float ReadiedScaleGrowthServerStartTime = 0.0f;

	UPROPERTY(Replicated, Transient)
	FName ReadiedScaleGrowthNiagaraVector2DParameterName = NAME_None;

	UPROPERTY(Replicated, Transient)
	FVector2D ReadiedScaleGrowthNiagaraStartSize = FVector2D::UnitVector;

	UPROPERTY(Replicated, Transient)
	FVector2D ReadiedScaleGrowthNiagaraTargetSize = FVector2D::UnitVector;

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
