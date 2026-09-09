#pragma once

#include "Components/ActorComponent.h"
#include "Definition/Character/CharacterBaseDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterDeathComponent.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;

/**
 * Owns common character death, damage presentation, ragdoll, dissolve, and
 * respawn restoration state. Game-specific death overrides remain on the
 * owning character and call into this component through ACharacterBase.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UCharacterDeathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterDeathComponent();

	void ApplySettings(const FCharacterDeathSettings& InSettings);
	void InitializeDeathRuntime();
	void ShutdownDeathRuntime();
	void TickRuntime(float DeltaSeconds);
	bool NeedsCharacterTick() const { return bDeathDissolveActive; }

	bool IsDeathHandled() const { return bDeathHandled; }
	void HandleDeadTagChanged(
		int32 NewCount,
		UAbilitySystemComponent* BoundAbilitySystemComponent);
	void HandleRemoteDeath();
	void ApplyDeathPhysics();
	void ResetDeathStateForRespawn();

	void StartDeathDissolveLocal(float DurationSeconds);
	void ResetDeathDissolve();
	float GetSafeDissolveDuration(float RequestedDuration) const;
	void ClearCharacterOverlayMaterialLocal();

	void HandleDamageTaken(float DamageAmount, bool bCriticalHit);
	void HandleRemoteDamageTaken(
		float DamageAmount,
		bool bCriticalHit,
		FVector WorldLocation);

private:
	ACharacterBase* GetCharacterOwner() const;
	void CacheInitialRespawnState();
	void InitializeDeathDissolveMaterials();
	void UpdateDeathDissolve(float DeltaSeconds);
	void SetDeathDissolveValue(float DissolveValue);
	void ConfigureWeaponDamageMesh(USkeletalMeshComponent* CharacterMesh) const;

	UPROPERTY(Transient)
	FCharacterDeathSettings Settings;

	UPROPERTY(Transient)
	bool bDeathHandled = false;

	UPROPERTY(Transient)
	bool bHasCachedRespawnInitialState = false;

	UPROPERTY(Transient)
	FTransform InitialMeshRelativeTransform = FTransform::Identity;

	UPROPERTY(Transient)
	TEnumAsByte<ECollisionEnabled::Type> InitialCapsuleCollisionEnabled =
		ECollisionEnabled::QueryAndPhysics;

	UPROPERTY(Transient)
	TEnumAsByte<ECollisionEnabled::Type> InitialMeshCollisionEnabled =
		ECollisionEnabled::QueryOnly;

	UPROPERTY(Transient)
	TEnumAsByte<EMovementMode> InitialRespawnMovementMode = MOVE_Walking;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DeathDissolveMaterialInstances;

	UPROPERTY(Transient)
	bool bDeathDissolveActive = false;

	UPROPERTY(Transient)
	float DeathDissolveElapsedSeconds = 0.0f;

	UPROPERTY(Transient)
	float DeathDissolveDurationSeconds = 0.0f;
};
