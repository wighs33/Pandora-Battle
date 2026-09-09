#pragma once

#include "Common/WeaponDefinitionData.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "PlayerAimComponent.generated.h"

class APdPlayer;
class UCharacterMovementComponent;
struct FPlayerAimSettings;

/**
 * Owns replicated weapon-aim state, compressed animation aim offsets, and
 * the authoritative sampling timer. Camera presentation stays in
 * UPlayerCameraComponent.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerAimComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ApplySettings(const FPlayerAimSettings& Settings);
	void StartReplication();
	void StopReplication();

	void SetWeaponAimActive(
		bool bEnabled,
		const FWeaponAimCameraSettings& CameraSettings);
	bool IsWeaponAimActive() const { return bWeaponAimActive; }

	void ApplyMovementSettings(UCharacterMovementComponent* MovementComponent);

private:
	UFUNCTION(Server, Reliable)
	void ServerSetWeaponAimActive(
		bool bEnabled,
		FWeaponAimCameraSettings CameraSettings);

	UFUNCTION()
	void OnRep_WeaponAimActive();

	UFUNCTION()
	void OnRep_ReplicatedAimOffset();

	APdPlayer* GetPlayerOwner() const;
	void ApplyWeaponAimState(
		bool bEnabled,
		const FWeaponAimCameraSettings& CameraSettings);
	void CacheMovementDefaults(UCharacterMovementComponent* MovementComponent);
	void UpdateReplicatedAimOffset();

	UPROPERTY(ReplicatedUsing = OnRep_WeaponAimActive, Transient)
	bool bWeaponAimActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedAimOffset, Transient)
	uint8 ReplicatedAimYaw = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedAimOffset, Transient)
	uint8 ReplicatedAimPitch = 0;

	UPROPERTY(Transient)
	FWeaponAimCameraSettings ActiveWeaponAimCameraSettings;

	UPROPERTY(Transient)
	float ReplicationInterval = 0.1f;

	UPROPERTY(Transient)
	FRotator DefaultRotationRate = FRotator(0.0f, 500.0f, 0.0f);

	UPROPERTY(Transient)
	FRotator AimingRotationRate = FRotator(0.0f, 3000.0f, 0.0f);

	UPROPERTY(Transient)
	bool bDefaultAllowPhysicsRotationDuringAnimRootMotion = false;

	UPROPERTY(Transient)
	bool bHasCachedMovementDefaults = false;

	FTimerHandle ReplicationTimerHandle;
};
