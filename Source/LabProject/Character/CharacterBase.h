#pragma once

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Character.h"
#include "GameplayCueInterface.h"
#include "GameplayTagContainer.h"
#include "CharacterBase.generated.h"

class APlayerController;
class AProjectileBase;
class UAbilitySystemComponent;
class UAnimInstance;
class UCharacterAbilityRuntimeComponent;
class UCharacterBaseDefinition;
class UCharacterDeathComponent;
class UCharacterHealthBarComponent;
class UCharacterMovementComponent;
class UCharacterPresentationComponent;
class UCombatComponent;
class UDamageIndicatorComponent;
class UEquipmentComponent;
class UHealthBarViewModel;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UPdAbilitySystemComponent;
class USkinEquipmentComponent;
class UUserWidget;
class UWidgetClassDefinition;
class UWidgetComponent;
struct FStreamableHandle;

/**
 * Shared character facade.
 *
 * Runtime state lives in focused components so players, enemies, and monsters
 * can reuse only the behavior they need. This actor keeps the stable Blueprint
 * and network API, owns the component composition, and supplies overridable
 * character-specific hooks.
 */
UCLASS()
class LABPROJECT_API ACharacterBase : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	ACharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void NotifyControllerChanged() override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual void UnPossessed() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "!Damage", meta = (DisplayName = "On Damage Taken"))
	void OnDamageTaken(float DamageAmount, bool bCriticalHit, FVector WorldLocation);

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void HandleGameplayCue(
		AActor* Self,
		FGameplayTag GameplayCueTag,
		EGameplayCueEvent::Type EventType,
		const FGameplayCueParameters& Parameters) override;

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem")
	UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const;

	UFUNCTION(BlueprintCallable,
		Category = "!AbilitySystem",
		meta = (DeprecatedFunction,
			DeprecationMessage = "Client-forwarded montage gameplay events are disabled. Use an authoritative server montage notify."))
	void ServerSendGameplayEventToSelf(FGameplayEventData EventData);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "!AbilitySystem")
	void MulticastSendGameplayEventToActor(AActor* TargetActor, FGameplayEventData EventData);

	void InitializeAbilitySystemActorInfo();
	void ClearAbilitySystemActorInfo();

	UFUNCTION(BlueprintPure, Category = "!Character|Definition")
	UCharacterBaseDefinition* GetCharacterDefinition() const;

	UFUNCTION(BlueprintPure, Category = "!Character|Runtime")
	UCharacterAbilityRuntimeComponent* GetCharacterAbilityRuntimeComponent() const { return CharacterAbilityRuntimeComponent; }

	UFUNCTION(BlueprintPure, Category = "!Character|Death")
	UCharacterDeathComponent* GetCharacterDeathComponent() const { return CharacterDeathComponent; }

	UFUNCTION(BlueprintPure, Category = "!Character|Presentation")
	UCharacterPresentationComponent* GetCharacterPresentationComponent() const { return CharacterPresentationComponent; }

	UFUNCTION(BlueprintPure, Category = "!Character|Health Bar")
	UCharacterHealthBarComponent* GetCharacterHealthBarComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	UEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Combat")
	UCombatComponent* GetCombatComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Skin")
	USkinEquipmentComponent* GetSkinEquipmentComponent() const { return SkinEquipmentComponent; }

	UFUNCTION(BlueprintPure, Category = "!DamageIndicator")
	UDamageIndicatorComponent* GetDamageIndicatorComponent() const;

	UFUNCTION(BlueprintCallable, Category = "!Ability|Aura")
	UNiagaraComponent* FindBodyAuraNiagaraComponent(FName ComponentName) const;

	UFUNCTION(BlueprintCallable, Category = "!Ability|Aura")
	void ApplyBodyAuraNiagara(FName ComponentName, UNiagaraSystem* NiagaraSystem, bool bActivate = true, bool bResetSystem = true);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "!Ability|Aura")
	void MulticastApplyBodyAuraNiagara(FName ComponentName, UNiagaraSystem* NiagaraSystem, bool bActivate = true, bool bResetSystem = true);

	UFUNCTION(BlueprintCallable, Category = "!Ability|Aura")
	void ApplyBodyAuraNiagaraWithOffset(
		FName ComponentName,
		UNiagaraSystem* NiagaraSystem,
		bool bActivate,
		bool bResetSystem,
		FVector RelativeLocationOffset,
		FVector RelativeScale);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "!Ability|Aura")
	void MulticastApplyBodyAuraNiagaraWithOffset(
		FName ComponentName,
		UNiagaraSystem* NiagaraSystem,
		bool bActivate,
		bool bResetSystem,
		FVector RelativeLocationOffset,
		FVector RelativeScale);

	void ClearBodyAuraNiagaraIfMatching(FName ComponentName, const UNiagaraSystem* ExpectedNiagaraSystem);

	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void RefreshHealthBarViewModel();

	UFUNCTION(BlueprintPure, Category = "!ViewModel")
	UHealthBarViewModel* GetHealthBarViewModel() const;

	void UpdateHealthBarVisibilityForLocalViewer(
		APlayerController* LocalPlayerController,
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float MaxDistanceSquared);
	void SetHealthBarVisibleForLocalViewer(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "!Animation|Aim")
	float GetAimYawForAnimation() const;

	UFUNCTION(BlueprintPure, Category = "!Animation|Aim")
	float GetAimPitchForAnimation() const;

	UFUNCTION(BlueprintCallable, Category = "!Animation")
	void ResetAnimationToDefault();

	UFUNCTION(BlueprintCallable, Category = "!Animation")
	void SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass);

	void LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const;

	UFUNCTION(BlueprintCallable, Category = "!Team")
	void ApplyTeamOverlayMaterial();

	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Presentation")
	void ApplySkillOverlayMaterial(UMaterialInterface* OverlayMaterial);

	void ApplySkillPresentationOverlay(UObject* PresentationSource, UMaterialInterface* OverlayMaterial);
	void ClearSkillPresentationOverlay(UObject* PresentationSource);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastApplySkillOverlayMaterial(UMaterialInterface* OverlayMaterial);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpawnProjectileCosmetic(
		TSubclassOf<AProjectileBase> ProjectileClass,
		FVector_NetQuantize SpawnLocation,
		FRotator SpawnRotation,
		FVector_NetQuantize TargetLocation,
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

	UFUNCTION()
	int32 GetMatchTeamColorIndex() const;

	UFUNCTION()
	bool IsSameTeam(const ACharacterBase* OtherCharacter) const;

	UFUNCTION(BlueprintPure, Category = "!Team")
	bool CanDamageCharacterByTeam(const ACharacterBase* OtherCharacter) const;

	UFUNCTION(BlueprintPure, Category = "!Status")
	bool IsStatusFrozen() const;

	/**
	 * Reapplies the character-specific steady-state rotation policy after a
	 * temporary gameplay system has released control of CharacterMovement.
	 */
	void ReapplyCurrentRotationPolicy();

	bool IsDeathHandled() const;

	virtual void HandleDamageTaken(
		float DamageAmount,
		bool bCriticalHit = false,
		bool bAllowHitReact = true,
		AActor* DamageInstigator = nullptr,
		AActor* DamageCauser = nullptr);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "!Damage")
	void HandleDeath();

	UFUNCTION(BlueprintCallable, Category = "!Damage|Respawn")
	virtual void ResetDeathStateForRespawn();

	void ResetDeathStateForRespawnAtTransform(const FTransform& RespawnTransform);

	UFUNCTION(BlueprintCallable, Category = "!Damage|Death|Dissolve")
	void StartDeathDissolve(float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "!Presentation")
	void ClearCharacterOverlayMaterial();

	UFUNCTION(BlueprintPure, Category = "!Faction")
	int32 GetFactionId() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "!AbilitySystem|Cue", meta = (DisplayName = "On Dash Cue Activated"))
	void OnDashCueActivated(const FGameplayCueParameters& Parameters);

	UFUNCTION(BlueprintImplementableEvent, Category = "!AbilitySystem|Cue", meta = (DisplayName = "On Dash Cue Removed"))
	void OnDashCueRemoved(const FGameplayCueParameters& Parameters);

protected:
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHandleDamageTaken(float DamageAmount, bool bCriticalHit, FVector_NetQuantize WorldLocation);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastHandleDeath();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartDeathDissolve(float DurationSeconds);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastResetDeathStateForRespawnAtTransform(const FTransform& RespawnTransform);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastClearCharacterOverlayMaterial();

	virtual AActor* GetAbilitySystemOwnerActor() const;
	virtual AActor* GetAbilitySystemAvatarActor() const;
	virtual void RestoreRotationSettingsAfterFrozen(UCharacterMovementComponent* MovementComponent);
	virtual void ApplyCurrentRotationPolicy(UCharacterMovementComponent* MovementComponent);
	virtual TSubclassOf<UUserWidget> ResolveHealthBarWidgetClass(const UWidgetClassDefinition* WidgetDefinition) const;
	virtual bool ShouldApplyResolvedHealthBarWidgetClass(UClass* CurrentWidgetClass, TSubclassOf<UUserWidget> ResolvedWidgetClass) const;
	virtual bool ShouldUseContinuousCharacterTick() const;
	virtual FVector GetDamageIndicatorWorldLocation() const;
	virtual bool IsAdditionalCharacterRuntimeContentReady() const;
	virtual void HandleCharacterRuntimeInitialized();

	void ApplyCharacterDefinition();
	void BeginCharacterDefinitionPreload();
	void HandleCharacterDefinitionPreloaded(uint32 RequestGeneration);
	void ReleaseCharacterDefinitionPreload();
	void TryInitializeCharacterRuntime();
	bool IsCharacterRuntimeInitialized() const { return bCharacterRuntimeInitialized; }
	void ApplyCameraCollisionIgnoreToCharacterComponents() const;
	void ApplySkillDamageCollisionToCharacterComponents() const;
	void RefreshCharacterTickEnabled();
	void ApplyMovementSpeedFromAttribute();
	void UpdateAimOffsetForAnimation();
	void SetAimOffsetForAnimation(float AimYaw, float AimPitch);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Character|Definition")
	TSoftObjectPtr<UCharacterBaseDefinition> CharacterDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterBaseDefinition> LoadedCharacterDefinition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Character|Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterAbilityRuntimeComponent> CharacterAbilityRuntimeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Character|Death", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterDeathComponent> CharacterDeathComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Character|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterPresentationComponent> CharacterPresentationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinEquipmentComponent> SkinEquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!DamageIndicator", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDamageIndicatorComponent> DamageIndicatorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Ability|Aura", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> BodyAuraNiagaraComponent;

	// Kept under its original property and subobject names for serialized
	// Blueprint compatibility. The actual object is a
	// UCharacterHealthBarComponent.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Widget", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidget;

	// Per-instance gameplay identity is intentionally not definition data.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Faction")
	int32 FactionId = 0;

	TSharedPtr<FStreamableHandle> CharacterDefinitionLoadHandle;
	uint32 CharacterDefinitionLoadGeneration = 0;
	bool bCharacterBeginPlayCalled = false;
	bool bCharacterDefinitionReady = false;
	bool bCharacterRuntimeInitialized = false;

private:
	friend class UCharacterAbilityRuntimeComponent;
	friend class UCharacterDeathComponent;
	friend class UCharacterHealthBarComponent;
	friend class UCharacterPresentationComponent;
};
