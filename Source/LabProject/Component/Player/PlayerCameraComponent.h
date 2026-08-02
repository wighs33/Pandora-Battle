#pragma once

#include "Common/WeaponDefinitionData.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Definition/Player/PlayerPawnDefinition.h"

#include "PlayerCameraComponent.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMeshComponent;
class USpringArmComponent;

/**
 * Local presentation policy for APdPlayer camera interpolation and
 * near-camera material occlusion. Network aim ownership remains on APdPlayer.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerCameraComponent();

	void InitializeCamera(
		USpringArmComponent* InCameraBoom,
		UCameraComponent* InFollowCamera,
		const FPlayerCameraPresentationSettings& Settings);
	void ShutdownCamera();
	void TickPresentation(float DeltaSeconds);

	void SetWeaponAimActive(bool bEnabled, const FWeaponAimCameraSettings& Settings);
	void SetAbilityCameraOverrideActive(bool bEnabled, const FWeaponAimCameraSettings& Settings);
	void SetAbilityCameraOverrideActiveForDuration(
		bool bEnabled,
		const FWeaponAimCameraSettings& Settings,
		float Duration);
	bool GetAimViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	bool IsWeaponAimCameraActive() const { return bWeaponAimCameraActive; }
	bool IsAbilityCameraOverrideActive() const { return bAbilityCameraOverrideActive; }
	const FWeaponAimCameraSettings& GetActiveWeaponAimSettings() const { return ActiveWeaponAimCameraSettings; }
	const FWeaponAimCameraSettings& GetActiveAbilityOverrideSettings() const { return ActiveAbilityCameraOverrideSettings; }

	bool HasCachedDefaults() const { return bHasCachedDefaults; }
	float GetDefaultFOV() const { return DefaultCameraFOV; }
	FVector GetDefaultBoomSocketOffset() const { return DefaultCameraBoomSocketOffset; }
	FRotator GetDefaultCameraRelativeRotation() const { return DefaultFollowCameraRelativeRotation; }

	static FWeaponAimCameraSettings SanitizeAimCameraSettings(FWeaponAimCameraSettings Settings);

private:
	void CacheCameraDefaults();
	void ClearAbilityCameraOverride();
	void UpdateAimCamera(float DeltaSeconds);
	void UpdateOcclusionMaterialState();
	void AppendOcclusionMeshComponents(
		float ProbeRadius,
		TSet<TWeakObjectPtr<UMeshComponent>>& OutMeshComponents) const;
	bool SetOcclusionEnabledForMesh(UMeshComponent* MeshComponent, bool bEnabled);
	void ResetOcclusionMaterialState();

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient)
	FPlayerCameraPresentationSettings PresentationSettings;

	UPROPERTY(Transient)
	bool bWeaponAimCameraActive = false;

	UPROPERTY(Transient)
	FWeaponAimCameraSettings ActiveWeaponAimCameraSettings;

	UPROPERTY(Transient)
	bool bAbilityCameraOverrideActive = false;

	UPROPERTY(Transient)
	FWeaponAimCameraSettings ActiveAbilityCameraOverrideSettings;

	UPROPERTY(Transient)
	bool bHasCachedDefaults = false;

	UPROPERTY(Transient)
	float DefaultCameraFOV = 0.0f;

	UPROPERTY(Transient)
	FVector DefaultCameraBoomSocketOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator DefaultFollowCameraRelativeRotation = FRotator::ZeroRotator;

	FTimerHandle AbilityCameraOverrideTimerHandle;

	TMap<TWeakObjectPtr<UMeshComponent>, TArray<TWeakObjectPtr<UMaterialInstanceDynamic>>>
		OcclusionMaterialInstances;
	TSet<TWeakObjectPtr<UMeshComponent>> OcclusionDisabledMeshComponents;
};
