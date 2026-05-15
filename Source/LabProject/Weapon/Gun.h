#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "Gun.generated.h"

class UAnimMontage;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AGun : public AWeaponBase
{
	GENERATED_BODY()

public:
	// Input commands
	virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter) override;

	// Query helpers
	virtual bool SupportsAutomaticFire() const override;
	virtual float GetAutomaticFireInterval() const override;

protected:
	// Network timing callbacks
	UFUNCTION(Server, Reliable)
	void ServerHandlePrimaryAttack(FVector_NetQuantize RequestedViewLocation, FVector_NetQuantizeNormal RequestedViewDirection);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastExecuteMuzzleFlashCue();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnImpactDecal(FVector_NetQuantize ImpactLocation, FVector_NetQuantizeNormal ImpactNormal, float DecalSize);

	// Action helpers
	void ExecuteMuzzleFlashCue(APdPlayer* PlayerCharacter) const;
	void SpawnImpactDecal(const FVector& ImpactLocation, const FVector& ImpactNormal, float DecalSize) const;
	bool TryConsumePrimaryAttackCooldown();
	bool TraceGunShot(
		APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		FHitResult& OutHitResult) const;
	bool HandlePrimaryAttackOnServer(
		APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection);

	// Query helpers
	bool ShouldSkipMulticastMuzzleFlashCue() const;
	bool HasConfiguredImpactDecal() const;
	bool ShouldSpawnImpactDecalForHit(const FHitResult& HitResult) const;
	float MakeImpactDecalSize() const;
	float GetGunFireInterval() const;
	const TArray<TEnumAsByte<EObjectTypeQuery>>& GetGunTraceObjectTypes() const;
	float GetGunTraceRange() const;
	float GetGunTraceRadius() const;
	bool TryGetGunAimTargetLocation(
		const FVector& ViewTraceStart,
		const FVector& ViewTraceDirection,
		float TraceRange,
		const TArray<AActor*>& ActorsToIgnore,
		FVector& OutTargetLocation) const;
	AActor* ResolveDamageTargetActor(AActor* HitActor) const;

	UPROPERTY(Transient)
	float NextPrimaryAttackTimeSeconds = -1.0f;
};
