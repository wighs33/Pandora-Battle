#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArrowProjectileBase.generated.h"

class UNiagaraSystem;
class ACharacterBase;
class AWeaponBase;
class UBoxComponent;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USoundBase;
class UStaticMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AArrowProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AArrowProjectileBase();

	// Public API
	UFUNCTION(BlueprintCallable, Category = "!Arrow")
	bool LaunchArrowActor(const FVector& Direction);

protected:
	// Timing hooks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// Delegate callbacks
	UFUNCTION()
	void HandleCollisionOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleCollisionHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	// Query helpers
	UPrimitiveComponent* GetCollisionComponent() const;
	UProjectileMovementComponent* GetProjectileMovementComponent() const;
	ACharacterBase* GetOwningCharacter() const;
	AWeaponBase* GetOwningWeapon() const;
	bool IsIgnoredImpactActor(const AActor* OtherActor) const;
	float GetImpactTraceRadius() const;
	void PerformImpactTrace();
	void StopProjectileMotion();
	bool TryHandleImpact(AActor* OtherActor, UPrimitiveComponent* OtherComp);

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Arrow|Components")
	TObjectPtr<UBoxComponent> CollisionBox = nullptr;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Arrow|Components")
	TObjectPtr<UStaticMeshComponent> ArrowMesh = nullptr;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Arrow|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Arrow|Launch")
	float LaunchSpeed = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Arrow|Launch")
	float LaunchGravityScale = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Arrow|Impact")
	float ImpactLifeSpan = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Arrow|Effects")
	TObjectPtr<UNiagaraSystem> TrailSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Arrow|Effects")
	TObjectPtr<USoundBase> LaunchSound = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Arrow|Effects")
	TObjectPtr<USoundBase> ImpactSound = nullptr;

	UPROPERTY(Transient)
	bool bHasImpacted = false;

	UPROPERTY(Transient)
	bool bImpactTraceActive = false;

	UPROPERTY(Transient)
	FVector PreviousImpactTraceLocation = FVector::ZeroVector;
};
