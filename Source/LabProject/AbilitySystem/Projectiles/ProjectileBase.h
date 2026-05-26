#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/NetSerialization.h"
#include "ProjectileBase.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UGameplayEffect;
class UAbilitySystemComponent;
class AEffectAreaBase;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AProjectileBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!Projectile")
	void InitializeProjectile(const FVector& InTargetLocation, float InSpeed, const FGameplayEffectSpecHandle& InDamageEffectSpecHandle);

	UFUNCTION(BlueprintCallable, Category = "!Projectile|Damage")
	void SetDamageEffectSpecHandle(const FGameplayEffectSpecHandle& InDamageEffectSpecHandle);

	UFUNCTION(BlueprintCallable, Category = "!Projectile|Impact")
	void SetImpactEffectAreaSpawnConfigs(
		const TArray<FProjectileImpactEffectAreaSpawnConfig>& InImpactEffectAreaSpawnConfigs,
		int32 InSourceSkillLevel);

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	UFUNCTION()
	void OnRep_ProjectileFlightData();

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
	void ConfigureCollision() const;
	void ConfigureIgnoredActors() const;
	void HandleImpact(AActor* OtherActor, UPrimitiveComponent* OtherComp);
	bool TryApplyDamageToTarget(AActor* TargetActor);
	bool TryApplyDebuffToTarget(AActor* TargetActor, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC) const;
	void TrySpawnImpactEffectAreas(AActor* TargetActor, bool bDamageApplied);
	bool ResolveImpactEffectAreaSpawnTransform(
		const FProjectileImpactEffectAreaSpawnConfig& SpawnConfig,
		AActor* TargetActor,
		FTransform& OutSpawnTransform) const;
	AActor* ResolveDamageTargetActor(AActor* OtherActor) const;
	bool IsIgnoredImpactActor(const AActor* OtherActor) const;
	void ExecuteSpawnGameplayCue() const;
	void ExecuteImpactGameplayCue();
	void ExecuteImpactGameplayCueAtLocation(const FVector& CueLocation);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExecuteImpactGameplayCue(FVector_NetQuantize CueLocation);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Projectile|Components")
	TObjectPtr<USphereComponent> SphereCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Projectile|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileFlightData, Category = "!Projectile")
	FVector_NetQuantize TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing = OnRep_ProjectileFlightData, Category = "!Projectile", meta = (ClampMin = "0.0"))
	float Speed = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Damage")
	FGameplayEffectSpecHandle DamageEffectSpecHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Debuff")
	TSubclassOf<UGameplayEffect> DebuffEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Debuff", meta = (ClampMin = "1.0"))
	float DebuffEffectLevel = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Impact")
	TArray<FProjectileImpactEffectAreaSpawnConfig> ImpactEffectAreaSpawnConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|Impact", meta = (ClampMin = "1"))
	int32 SourceSkillLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag SpawnGameplayCueTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Projectile|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag ImpactGameplayCueTag;

	UPROPERTY(Transient)
	bool bHasImpacted = false;

	UPROPERTY(Transient)
	bool bImpactCueExecuted = false;
};
