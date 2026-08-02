#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "Gun.generated.h"

class UAnimMontage;
class UPrimitiveComponent;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AGun : public AWeaponBase
{
	GENERATED_BODY()

public:
	// Input commands
	virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter) override;
	virtual bool HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor) override;
	virtual bool HandleAIPrimaryAttackAtLocation(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation) override;

	// Query helpers
	virtual bool SupportsAutomaticFire() const override;
	virtual float GetAutomaticFireInterval() const override;
	virtual bool ShouldTriggerHitReactOnDamage() const override;

protected:
	// Network timing callbacks
	UFUNCTION(Server, Reliable)
	void ServerHandlePrimaryAttack(FVector_NetQuantize RequestedViewLocation, FVector_NetQuantizeNormal RequestedViewDirection);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastExecuteMuzzleFlashCue();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnImpactDecal(FVector_NetQuantize ImpactLocation, FVector_NetQuantizeNormal ImpactNormal, float DecalSize);

	// Action helpers
	void ExecuteMuzzleFlashCue(ACharacterBase* Character) const;
	void SpawnImpactDecal(const FVector& ImpactLocation, const FVector& ImpactNormal, float DecalSize) const;
	bool TryConsumePrimaryAttackCooldown();
	bool TraceGunShot(
		APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		FHitResult& OutHitResult) const;
	bool TraceAIGunShotAtLocation(
		ACharacterBase* AttackingCharacter,
		const FVector& TargetLocation,
		FHitResult& OutHitResult,
		FVector& OutShotDirection) const;
	bool HandlePrimaryAttackOnServer(
		APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection);
	bool HandleAIPrimaryAttackOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor);
	bool HandleAIPrimaryAttackAtLocationOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation);

	// Query helpers
	bool ShouldSkipMulticastMuzzleFlashCue() const;
	bool HasConfiguredImpactDecal() const;
	bool ShouldSpawnImpactDecalForHit(const FHitResult& HitResult) const;
	float MakeImpactDecalSize() const;
	float GetGunFireInterval() const;
	TArray<TEnumAsByte<EObjectTypeQuery>> GetGunTraceObjectTypes() const;
	float GetGunTraceRange() const;
	float GetGunTraceRadius() const;
	FVector GetGunTraceStartLocation(const ACharacterBase* Character) const;
	FVector GetAITargetAimLocation(const AActor* TargetActor) const;
	void AppendEnemyCapsuleTraceHits(
		const FVector& TraceStart,
		const FVector& TraceEnd,
		float TraceRadius,
		const TArray<AActor*>& ActorsToIgnore,
		TArray<FHitResult>& InOutHitResults) const;
	bool SelectFirstValidGunImpact(const TArray<FHitResult>& HitResults, FHitResult& OutHitResult) const;
	bool IsFriendlyDamageTargetActor(AActor* HitActor, const UPrimitiveComponent* HitComponent) const;
	AActor* ResolveDamageTargetActor(AActor* HitActor, const UPrimitiveComponent* HitComponent) const;

	UPROPERTY(Transient)
	float NextPrimaryAttackTimeSeconds = -1.0f;
};
