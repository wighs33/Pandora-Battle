#pragma once

#include "CoreMinimal.h"
#include "Weapon/RangedWeaponBase.h"
#include "Gun.generated.h"

class UPrimitiveComponent;
struct FPdRewindRequest;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AGun : public ARangedWeaponBase
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	virtual bool SupportsAutomaticFire() const override;
	virtual float GetAutomaticFireInterval() const override;
	virtual bool ShouldTriggerHitReactOnDamage() const override;

protected:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	// ClientViewServerTime은 사격 순간 클라이언트 화면이 보여 주던 서버 시각이다. 서버가 되감기 판정에 사용한다.
	UFUNCTION(Server, Reliable)
	void ServerHandlePrimaryAttack(
		FVector_NetQuantize RequestedViewLocation,
		FVector_NetQuantizeNormal RequestedViewDirection,
		double ClientViewServerTime);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastExecuteMuzzleFlashCue();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnImpactDecal(FVector_NetQuantize ImpactLocation, FVector_NetQuantizeNormal ImpactNormal, float DecalSize);

public:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter) override;
	virtual bool HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor) override;
	virtual bool HandleAIPrimaryAttackAtLocation(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation) override;

protected:
	bool HandlePrimaryAttackOnServer(
		APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		double ClientViewServerTime = -1.0);
	bool HandleAIPrimaryAttackAtLocationOnServer(ACharacterBase* AttackingCharacter, const FVector& TargetLocation);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ExecuteMuzzleFlashCue(ACharacterBase* Character) const;
	void SpawnImpactDecal(const FVector& ImpactLocation, const FVector& ImpactNormal, float DecalSize) const;
	bool TryConsumePrimaryAttackCooldown();
	bool TraceGunShot(
		APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		FHitResult& OutHitResult,
		double RewindServerTime = -1.0) const;
	void RecordLagCompensatedShot(
		APdPlayer* PlayerCharacter,
		const FPdRewindRequest& RewindRequest,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		const FHitResult* JudgedHitResult) const;
	void RecordClientPerceivedShot(
		APdPlayer* PlayerCharacter,
		const FVector& ViewLocation,
		const FVector& ViewDirection) const;
	bool TraceAIGunShotAtLocation(
		ACharacterBase* AttackingCharacter,
		const FVector& TargetLocation,
		FHitResult& OutHitResult,
		FVector& OutShotDirection) const;

	bool ShouldSkipMulticastMuzzleFlashCue() const;
	bool HasConfiguredImpactDecal() const;
	bool ShouldSpawnImpactDecalForHit(const FHitResult& HitResult) const;
	float MakeImpactDecalSize() const;
	float GetGunFireInterval() const;
	TArray<TEnumAsByte<EObjectTypeQuery>> GetGunTraceObjectTypes() const;
	float GetGunTraceRange() const;
	float GetGunTraceRadius() const;
	FVector GetGunTraceStartLocation(const ACharacterBase* Character) const;
	bool SelectFirstValidGunImpact(const TArray<FHitResult>& HitResults, FHitResult& OutHitResult) const;
	bool IsFriendlyDamageTargetActor(AActor* HitActor, const UPrimitiveComponent* HitComponent) const;
	AActor* ResolveDamageTargetActor(AActor* HitActor, const UPrimitiveComponent* HitComponent) const;

protected:
	UPROPERTY(Transient)
	float NextPrimaryAttackTimeSeconds = -1.0f;
};
