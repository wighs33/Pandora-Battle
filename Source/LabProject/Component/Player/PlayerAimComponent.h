#pragma once

#include "Common/WeaponDefinitionData.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "PlayerAimComponent.generated.h"

class APdPlayer;
class UCharacterMovementComponent;
struct FPlayerAimSettings;

/**
 * 복제되는 무기 조준 상태·압축된 애니메이션 조준 오프셋·서버 권한의 샘플링 타이머를 관리한다.
 * 카메라 연출은 UPlayerCameraComponent가 담당한다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	UPlayerAimComponent();

	void ApplySettings(const FPlayerAimSettings& Settings);
	void StartReplication();
	void StopReplication();

	void SetWeaponAimActive(
		bool bEnabled,
		const FWeaponAimCameraSettings& CameraSettings);
	bool IsWeaponAimActive() const { return bWeaponAimActive; }

	void ApplyMovementSettings(UCharacterMovementComponent* MovementComponent);

private:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerSetWeaponAimActive(
		bool bEnabled,
		FWeaponAimCameraSettings CameraSettings);

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_WeaponAimActive();

	UFUNCTION()
	void OnRep_ReplicatedAimOffset();
	void UpdateReplicatedAimOffset();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	APdPlayer* GetPlayerOwner() const;
	void ApplyWeaponAimState(
		bool bEnabled,
		const FWeaponAimCameraSettings& CameraSettings);
	void CacheMovementDefaults(UCharacterMovementComponent* MovementComponent);

private:
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
