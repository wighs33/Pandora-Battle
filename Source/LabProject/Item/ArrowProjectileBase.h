#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/AttributeSet/DamageRules.h"
#include "GameFramework/Actor.h"
#include "ArrowProjectileBase.generated.h"

class UNiagaraSystem;
class ACharacterBase;
class UBoxComponent;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USoundBase;
class UStaticMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AArrowProjectileBase : public AActor
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	AArrowProjectileBase();

	UFUNCTION(BlueprintCallable, Category = "!Arrow")
	bool LaunchArrowActor(const FVector& Direction);

	/** 발사할 때 정한 피해를 담는다. 명중할 때 무기가 바뀌었거나 해제됐어도 이 값으로 피해를 준다. */
	void SetLaunchDamage(const DamageRules::FOutgoingDamage& Damage) { LaunchDamage = Damage; }

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
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

	UFUNCTION()
	void OnRep_FlightState();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UPrimitiveComponent* GetCollisionComponent() const;
	UProjectileMovementComponent* GetProjectileMovementComponent() const;
	ACharacterBase* GetOwningCharacter() const;
	bool IsIgnoredImpactActor(const AActor* OtherActor) const;
	void PerformImpactTrace();
	void StopProjectileMotion();
	bool TryHandleImpact(AActor* OtherActor, UPrimitiveComponent* OtherComp);
	void RefreshFlightEffects();

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

	/** 발사와 명중은 서버가 정하고, 소리와 궤적은 각 화면이 이 두 값이 바뀌는 것을 보고 재생한다. */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_FlightState)
	bool bLaunched = false;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_FlightState)
	bool bHasImpacted = false;

	UPROPERTY(Transient)
	bool bImpactTraceActive = false;

	UPROPERTY(Transient)
	FVector PreviousImpactTraceLocation = FVector::ZeroVector;

	DamageRules::FOutgoingDamage LaunchDamage;

	/** 이 화면에서 연출을 이미 재생했는지. 같은 연출이 두 번 나오지 않게 한다. */
	bool bLaunchEffectsPlayed = false;
	bool bImpactEffectsPlayed = false;

	/** 이 화면이 꽂히기 전의 화살을 본 적이 있는지. 처음 봤을 때 이미 꽂혀 있던 화살은 충돌음을 내지 않는다. */
	bool bSeenBeforeImpact = false;
};
