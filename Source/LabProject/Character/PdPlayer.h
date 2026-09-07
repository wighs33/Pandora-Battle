#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterBase.h"
#include "Common/WeaponDefinitionData.h"
#include "Interface/InteractableInterface.h"
#include "PdPlayer.generated.h"

class UBoxComponent;
class UCameraComponent;
class UCableComponent;
class UCombatComponent;
class UGrappleComponent;
class UPaintCanvasComponent;
class UPlayerActionComponent;
class UPlayerAimComponent;
class UPlayerCameraComponent;
class UPlayerInteractionComponent;
class UPlayerPawnDefinition;
class USpringArmComponent;
class AActor;
class UAnimMontage;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * 플레이어 전용 컴포넌트를 구성하고 조작·카메라·로드아웃의 생명주기를 연결한다.
 *
 * 능력과 경기 데이터는 PlayerState에, 상호작용·조준·그래플의 실행 상태는 각 컴포넌트에 둔다.
 */
UCLASS()
class LABPROJECT_API APdPlayer : public ACharacterBase
{
	GENERATED_BODY()

public:
	APdPlayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void PreInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual void NotifyControllerChanged() override;
	virtual void Tick(float DeltaSeconds) override;

	//------------------------------------------------------------------------------------------------------------------

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
	virtual AActor* GetAbilitySystemOwnerActor() const override;
	virtual void ApplyCurrentRotationPolicy(UCharacterMovementComponent* MovementComponent) override;
	virtual bool ShouldUseContinuousCharacterTick() const override;
	virtual bool IsAdditionalCharacterRuntimeContentReady() const override;
	virtual void HandleCharacterRuntimeInitialized() override;
	void BeginPlayerPawnDefinitionPreload();
	void HandlePlayerPawnDefinitionPreloaded(FSoftObjectPath DefinitionPath, uint32 RequestGeneration);
	void ReleasePlayerPawnDefinitionPreload();
	void ApplySelectedPlayerLoadout();
	void ApplyPlayerPawnDefinition();
	void UpdateAimOffsetForReplicationComponent();
	void ApplyReplicatedAimOffsetFromComponent(float AimYaw, float AimPitch);

protected:
	friend class UPlayerAimComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Definition",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPlayerPawnDefinition> PlayerPawnDefinition;

	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPROPERTY(VisibleAnywhere, Category = "!Player|Component")
	TObjectPtr<UCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionBox;

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

private:
	TSharedPtr<FStreamableHandle> PlayerPawnDefinitionLoadHandle;
	uint32 PlayerPawnDefinitionLoadGeneration = 0;
	bool bPlayerPawnDefinitionReady = false;
};
