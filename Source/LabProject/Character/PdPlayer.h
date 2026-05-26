#pragma once

#include "Character/PdCharacterBase.h"
#include "Common/WeaponDefinitionData.h"
#include "Interface/InteractableInterface.h"
#include "SavedGameData/PlayerPandoraData.h"
#include "TimerManager.h"
#include "PdPlayer.generated.h"

class UBoxComponent;
class UCameraComponent;
class UPrimitiveComponent;
class USpringArmComponent;
class APdPlayerState;
class AActor;
class UPdSaveGame;
class UPandoraTreeComponent;
class UPandoraComponent;
class UPandoraDefinition;

/**
 * <?åÎ†à?¥Ïñ¥ Ï∫êÎ¶≠??
 * - ?ÅÌò∏?ëÏö© ?Ä??Í∞êÏ?Î•??¥Îãπ?©Îãà??
 * - ?åÎ†à?¥Ïñ¥ ?ÑÏö© Ïπ¥Î©î??Íµ¨ÏÑ±??Í∞ÄÏßëÎãà??
 * - ASC ?åÏú†?êÎ? PlayerStateÎ°??¨Ïö©?©Îãà??
 */
UCLASS()
class LABPROJECT_API APdPlayer : public APdCharacterBase
{
	GENERATED_BODY()

public:
	/** ?åÎ†à?¥Ïñ¥ Í∏∞Î≥∏ ?ÅÌÉúÎ•?Ï¥àÍ∏∞?îÌï©?àÎã§. */
	APdPlayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Timing hooks
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Tick(float DeltaSeconds) override;

	/** PlayerState Í∏∞Ï? ASCÎ•?Î∞òÌôò?©Îãà?? */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** ?ÑÏû¨ ?ÅÌò∏?ëÏö© ?Ä?ÅÏù¥ ?àÎäîÏßÄ Î∞òÌôò?©Îãà?? */
	UFUNCTION(BlueprintCallable, Category = "!Interaction", meta = (DisplayName = "HasCurrentInteractActors?"))
	bool HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const;

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Aim")
	void SetWeaponAimActive(bool bEnabled, const FWeaponAimCameraSettings& AimCameraSettings);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Camera")
	void SetAbilityCameraOverrideActive(bool bEnabled, const FWeaponAimCameraSettings& CameraSettings);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Camera", meta = (ClampMin = "0.0", ForceUnits = "s"))
	void SetAbilityCameraOverrideActiveForDuration(bool bEnabled, const FWeaponAimCameraSettings& CameraSettings, float Duration);

	UFUNCTION(BlueprintPure, Category = "!Weapon|Aim")
	bool IsWeaponAimActive() const { return bIsWeaponAimActive; }

	bool GetWeaponAimViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	bool CanInteractWithActor(AActor* InteractableActor) const;

protected:
	// Delegate callbacks
	/** ?ÅÌò∏?ëÏö© Î∞ïÏä§ ÏßÑÏûÖ??Ï≤òÎ¶¨?©Îãà?? */
	UFUNCTION()
	void HandleInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** ?ÅÌò∏?ëÏö© Î∞ïÏä§ ?¥ÌÉà??Ï≤òÎ¶¨?©Îãà?? */
	UFUNCTION()
	void HandleInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// Ability-system timing hooks
	/** ASC ?åÏú† ?°ÌÑ∞Î•?Î∞òÌôò?©Îãà?? */
	virtual AActor* GetAbilitySystemOwnerActor() const override;
	void ApplyWeaponAimState(bool bEnabled, const FWeaponAimCameraSettings& AimCameraSettings);
	void UpdateWeaponAimCamera(float DeltaSeconds);
	void ClearAbilityCameraOverride();

	UFUNCTION(Server, Reliable)
	void ServerSetWeaponAimActive(bool bEnabled, FWeaponAimCameraSettings AimCameraSettings);

	/** PlayerStateÎ•??ÑÎ°ú?ùÌä∏ ?Ä?ÖÏúºÎ°?Î∞òÌôò?©Îãà?? */
	APdPlayerState* GetPdPlayerState() const;
	void InitializePandoraTreeFromSave(AController* NewController);
	FString GetPlayerSaveId(AController* InController) const;

	UFUNCTION(BlueprintCallable, Category = "!Pandora|Save")
	void SavePlayerPandoraData(const FPlayerPandoraData& InPlayerPandoraData);

	UFUNCTION()
	void HandlePandoraTreePandorasChanged();

	UFUNCTION()
	void HandlePandoraTreePointsChanged(int32 NewPointsAvailable);

	UFUNCTION()
	void HandlePandoraSelectionChanged(UPandoraDefinition* NewPandoraDefinition);

	UFUNCTION()
	void HandlePandoraLoadoutChanged();

	/** ?°ÌÑ∞Î•??ÅÌò∏?ëÏö© ?îÌä∏Î¶¨Î°ú Î≥Ä?òÌï©?àÎã§. */
	bool TryMakeInteractableEntry(AActor* OtherActor, TScriptInterface<IInteractableInterface>& OutInteractableActor) const;

protected:
	/** ?ÑÏû¨ ?ÅÌò∏?ëÏö© Í∞Ä?•Ìïú ?°ÌÑ∞ Î™©Î°ù?ÖÎãà?? */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "!Interaction")
	TArray<TScriptInterface<IInteractableInterface>> CurrentInteractActors;

	/** ?ÅÌò∏?ëÏö© Í∞êÏ? Î∞ïÏä§?ÖÎãà?? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Interaction", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float InteractionServerValidationDistance = 250.0f;

	/** Ïπ¥Î©î??Î∂êÏûÖ?àÎã§. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Ï∂îÏ†Å Ïπ¥Î©î?ºÏûÖ?àÎã§. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim")
	bool bIsWeaponAimActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	bool bWeaponAimCameraActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	FWeaponAimCameraSettings ActiveWeaponAimCameraSettings;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Ability|Camera")
	bool bAbilityCameraOverrideActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Ability|Camera")
	FWeaponAimCameraSettings ActiveAbilityCameraOverrideSettings;

	FTimerHandle AbilityCameraOverrideTimerHandle;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	bool bHasCachedWeaponAimCameraDefaults = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	float DefaultCameraFOV = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	FVector DefaultCameraBoomSocketOffset = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	FRotator DefaultFollowCameraRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Pandora|Save")
	TObjectPtr<UPdSaveGame> PlayerSaveGameData;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraTreeComponent> BoundPandoraTreeComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPandoraComponent> BoundPandoraComponent;

	UPROPERTY(Transient)
	bool bRestoringPandoraSelectionFromSave = false;

	UPROPERTY(Transient)
	FString CachedPlayerSaveId;
};
