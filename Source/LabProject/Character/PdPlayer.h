#pragma once

#include "Character/CharacterBase.h"
#include "Common/WeaponDefinitionData.h"
#include "Interface/InteractableInterface.h"
#include "PdPlayer.generated.h"

class UBoxComponent;
class UCameraComponent;
class UCableComponent;
class UGrappleComponent;
class UPaintCanvasComponent;
class UPlayerActionComponent;
class UPlayerAimComponent;
class UPlayerCameraComponent;
class UPlayerInteractionComponent;
class UPlayerPawnDefinition;
class USpringArmComponent;
class APdPlayerState;
class AActor;
class UAnimMontage;
class UMaterialInterface;
class UStaticMeshComponent;

UCLASS()
class LABPROJECT_API APdPlayer : public ACharacterBase
{
	GENERATED_BODY()

public:
	APdPlayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Timing hooks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void NotifyControllerChanged() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void HandleDeath_Implementation() override;
	virtual void ResetDeathStateForRespawn() override;

	UFUNCTION(BlueprintCallable, Category = "!Interaction", meta = (DisplayName = "HasCurrentInteractActors?"))
	bool HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const;

	UFUNCTION(BlueprintPure, Category = "!Interaction")
	AActor* GetCurrentInteractActor() const;

	UFUNCTION(BlueprintCallable, Category = "!Interaction")
	bool InteractWithCurrentTarget();

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Aim")
	void SetWeaponAimActive(bool bEnabled, const FWeaponAimCameraSettings& AimCameraSettings);

	bool IsGrappling() const;

	UGrappleComponent* GetGrappleComponent() const { return GrappleComponent; }
	UPlayerActionComponent* GetPlayerActionComponent() const { return PlayerActionComponent; }
	UPlayerAimComponent* GetPlayerAimComponent() const { return PlayerAimComponent; }
	UPlayerCameraComponent* GetPlayerCameraComponent() const { return PlayerCameraComponent; }
	UPlayerInteractionComponent* GetPlayerInteractionComponent() const { return PlayerInteractionComponent; }

	bool RequestCancelHitReactForMovement(float BlendOutTime = 0.08f);

	void HidePaintCanvas();

	bool HasActivePaintCanvas() const;

	bool ExportActivePaintCanvasToSpeechBubble();

	bool ApplyActivePaintCanvasToFaceDecal(
		UMaterialInterface* FaceDecalMaterial,
		FName AttachSocketName,
		const FTransform& FaceDecalTransformOffset,
		FVector FaceDecalSize,
		FName TextureParameterName);

	void RestoreCachedLobbyPaintCanvasFaceDecal();

	UPaintCanvasComponent* GetPaintCanvasComponent() const { return PaintCanvasComponent; }

	UFUNCTION(BlueprintCallable, Category = "!Ability|Camera")
	void SetAbilityCameraOverrideActive(bool bEnabled, const FWeaponAimCameraSettings& CameraSettings);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Camera", meta = (ClampMin = "0.0", ForceUnits = "s"))
	void SetAbilityCameraOverrideActiveForDuration(
		bool bEnabled,
		const FWeaponAimCameraSettings& CameraSettings,
		float Duration);

	UFUNCTION(BlueprintPure, Category = "!Weapon|Aim")
	bool IsWeaponAimActive() const;

	bool GetWeaponAimViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	bool CanInteractWithActor(AActor* InteractableActor) const;

	UFUNCTION(BlueprintCallable, Category = "!Interaction|Animation")
	void PlayInteractionMontage(UAnimMontage* Montage, float PlayRate = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "!Interaction|Animation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	void StopInteractionMontage(float BlendOutTime = 0.15f);

	UFUNCTION(BlueprintPure, Category = "!Interaction|Animation")
	bool IsInteractionMontagePlaying() const;

protected:
	// Ability-system timing hooks
	virtual AActor* GetAbilitySystemOwnerActor() const override;
	virtual void ApplyCurrentRotationPolicy(UCharacterMovementComponent* MovementComponent) override;
	virtual bool ShouldUseContinuousCharacterTick() const override;
	void ResolvePlayerPawnDefinition();
	void ApplyPlayerPawnDefinition();
	void UpdateAimOffsetForReplicationComponent();
	void ApplyReplicatedAimOffsetFromComponent(float AimYaw, float AimPitch);

	APdPlayerState* GetPdPlayerState() const;

protected:
	friend class UPlayerAimComponent;
	friend class UPlayerInteractionComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Definition",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerPawnDefinition> PlayerPawnDefinition;

	UPROPERTY(BlueprintReadWrite, Transient, Category = "!Interaction")
	TArray<TScriptInterface<IInteractableInterface>> CurrentInteractActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Interaction", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float InteractionServerValidationDistance = 250.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Player|Component", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerInteractionComponent> PlayerInteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Player|Component", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerCameraComponent> PlayerCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Player|Component", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerAimComponent> PlayerAimComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Player|Component", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerActionComponent> PlayerActionComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Grapple")
	TObjectPtr<UGrappleComponent> GrappleComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Paint")
	TObjectPtr<UPaintCanvasComponent> PaintCanvasComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Paint|Export", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> SpeechBubblePlaneComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Grapple", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCableComponent> HookComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

};
