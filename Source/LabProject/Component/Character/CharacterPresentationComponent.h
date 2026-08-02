#pragma once

#include "Components/ActorComponent.h"
#include "Definition/Character/CharacterBaseDefinition.h"
#include "GameplayCueInterface.h"
#include "GameplayTagContainer.h"
#include "CharacterPresentationComponent.generated.h"

class ACharacterBase;
class AProjectileBase;
class UAnimInstance;
class UMaterialInterface;
class UMatchRuleDefinition;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * Shared visual state for characters: animation layers, aim-offset data,
 * team/skill overlays, body auras, dash cues, and cosmetic projectiles.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UCharacterPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterPresentationComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void ApplySettings(const FCharacterPresentationSettings& InSettings);
	void InitializePresentation(UNiagaraComponent* InDefaultBodyAuraComponent);
	void ShutdownPresentation();

	void ResetAnimationToDefault();
	void SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass);
	void LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const;
	void UpdateAimOffset();
	void SetAimOffset(float AimYaw, float AimPitch);
	float GetAimYaw() const { return AimYaw; }
	float GetAimPitch() const { return AimPitch; }

	void BindMatchTeamColorChanged();
	void UnbindMatchTeamColorChanged();
	void ApplyTeamOverlayMaterial();
	void ApplySkillOverlayMaterial(UMaterialInterface* OverlayMaterial);
	void ApplySkillPresentationOverlay(
		UObject* PresentationSource,
		UMaterialInterface* OverlayMaterial);
	void ClearSkillPresentationOverlay(UObject* PresentationSource);
	void ClearCharacterOverlayMaterialLocal();

	void HandleDashGameplayCue(
		EGameplayCueEvent::Type EventType,
		const FGameplayCueParameters& Parameters);

	UNiagaraComponent* FindBodyAuraNiagaraComponent(FName ComponentName) const;
	void ApplyBodyAuraNiagara(
		FName ComponentName,
		UNiagaraSystem* NiagaraSystem,
		bool bActivate,
		bool bResetSystem);
	void ApplyBodyAuraNiagaraWithOffset(
		FName ComponentName,
		UNiagaraSystem* NiagaraSystem,
		bool bActivate,
		bool bResetSystem,
		FVector RelativeLocationOffset,
		FVector RelativeScale);
	void ClearBodyAuraNiagaraIfMatching(
		FName ComponentName,
		const UNiagaraSystem* ExpectedNiagaraSystem);

	void SpawnProjectileCosmetic(
		TSubclassOf<AProjectileBase> ProjectileClass,
		FVector SpawnLocation,
		FRotator SpawnRotation,
		FVector TargetLocation,
		float Speed,
		bool bUseArcTrajectory,
		float ArcHeight,
		float ArcGravityScale,
		UNiagaraSystem* MuzzleFX,
		UNiagaraSystem* ProjectileFX,
		UNiagaraSystem* HitFX,
		bool bSpawnHitNiagaraOnGround,
		FGameplayTag SpawnGameplayCueTag,
		FGameplayTag ImpactGameplayCueTag,
		FVector SpawnScale,
		FName NiagaraVector2DParameterName,
		FVector2D NiagaraSize,
		float LifeSpan);

private:
	UFUNCTION()
	void OnRep_CurrentAnimLayer();

	void HandleMatchTeamColorChanged(int32 NewTeamColorIndex);
	void RefreshCharacterOverlayMaterial();
	UMaterialInterface* GetPreferredSkillOverlayMaterial();
	const UMatchRuleDefinition* GetTeamOverlayMatchRuleDefinition() const;
	void QueueTeamOverlayMaterialRetry();
	FVector GetClampedBodyAuraRelativeLocationOffset(
		FVector RelativeLocationOffset) const;
	FVector GetClampedBodyAuraRelativeScale(FVector RelativeScale) const;
	ACharacterBase* GetCharacterOwner() const;
	const ACharacterBase* GetCharacterOwnerConst() const;

	UPROPERTY(Transient)
	FCharacterPresentationSettings Settings;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> DefaultBodyAuraComponent;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentAnimLayer, Transient)
	TSubclassOf<UAnimInstance> CurrentAnimLayer;

	UPROPERTY(Transient)
	float AimYaw = 0.0f;

	UPROPERTY(Transient)
	float AimPitch = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ActiveSkillOverlayMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> SkillPresentationOverlaySources;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SkillPresentationOverlayMaterials;

	FTimerHandle TeamOverlayMaterialRetryTimerHandle;
	int32 TeamOverlayMaterialRetryCount = 0;
	TWeakObjectPtr<class APdPlayerState> TeamColorBoundPlayerState;
};
