#pragma once

#include "CoreMinimal.h"
#include "Common/WeaponDefinitionData.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectKey.h"
#include "WeaponBase.generated.h"

class ACharacterBase;
class AArrowProjectileBase;
class APdPlayer;
class UItemDefinition;
class UAnimMontage;
class USceneComponent;
class USkeletalMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UGameplayEffect;
class UPrimitiveComponent;
class UStatusEffectDefinition;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	// Actor lifecycle
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Commands
	UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
	void SetBeginOverlapEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
	void StartAttackTrace();

	void StartAttackTraceForSection(FName AttackSectionName);
	void ResetAttackHitTracking();

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
	void StopAttackTrace();

	bool PlayWeaponAttackMontage(FName StartingSection = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Animation")
	bool JumpToWeaponMontageSectionAndResume(FName SectionName);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Animation")
	void StopWeaponMontage(float BlendOutTime = 0.1f);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|VFX")
	bool HasSkillWeaponTrailComponent() const;

	UFUNCTION(BlueprintCallable, Category = "!Weapon|VFX")
	bool StartSkillWeaponTrail(UNiagaraSystem* TrailSystem);

	bool StartSkillWeaponTrail();

	UFUNCTION(BlueprintCallable, Category = "!Weapon|VFX")
	void StopSkillWeaponTrail();

	void PlayComboWindowStartEffect(UNiagaraSystem* EffectSystem);

	void ConfigureSkillSlash(
		UNiagaraSystem* SlashSystem,
		const FVector& SlashScale,
		const FVector& SlashSpawnLocationOffset,
		FName SlashSpawnSocketName,
		const FRotator& SlashSpawnRotationOffset,
		float AttackTraceEndMultiplier,
		bool bEnableHitTrace,
		TSubclassOf<UGameplayEffect> AdditionalDamageEffectClass = nullptr,
		FGameplayTag AdditionalDamageDataTag = FGameplayTag(),
		float AdditionalDamageMagnitude = 0.0f,
		int32 AdditionalDamageLevel = 1,
		UObject* AdditionalDamageSourceObject = nullptr,
		const FGameplayEffectSpecHandle& DebuffEffectSpecHandle = FGameplayEffectSpecHandle(),
		UStatusEffectDefinition* StatusEffectDefinition = nullptr,
		float AdditionalDamageDelay = 0.12f);
	void PlaySkillSlashVisual();
	void ClearSkillSlash();

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Trace")
	void SetTemporaryAttackTraceEndZMultiplier(UObject* SourceObject, float Multiplier);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Trace")
	void ClearTemporaryAttackTraceEndZMultiplier(UObject* SourceObject);

	UFUNCTION(BlueprintPure, Category = "!Weapon|Trace")
	float GetTemporaryAttackTraceEndZMultiplier() const;

	void InitializeFromItemDefinition(const UItemDefinition* InItemDefinition);

	// Query helpers
	bool SupportsAimInput() const;
	bool CanUseRangedWeapon(
		const ACharacterBase* AttackingCharacter,
		bool bRequirePlayerAim) const;
	FGameplayTag GetAimCrosshairWidgetTag() const;
	const FWeaponAimCameraSettings& GetAimCameraSettings() const;
	virtual bool ShouldTriggerHitReactOnDamage() const;

	// Input commands
	virtual bool HandleAimStart(APdPlayer* PlayerCharacter);
	virtual void HandleAimEnd(APdPlayer* PlayerCharacter);
	virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter);
	virtual bool HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor);
	virtual bool HandleAIPrimaryAttackAtLocation(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation);
	virtual bool SupportsAutomaticFire() const;
	virtual float GetAutomaticFireInterval() const;

	// Delegate callbacks
	virtual bool OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter);

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon|Trace")
	TObjectPtr<USceneComponent> AttackTraceStart;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon|Trace")
	TObjectPtr<USceneComponent> AttackTraceEnd;

protected:
	// Network timing callbacks
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartSkillWeaponTrail(UNiagaraSystem* TrailSystem);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopSkillWeaponTrail();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnSkillSlashNiagara(
		UNiagaraSystem* SlashSystem,
		const FVector& SpawnLocation,
		const FRotator& SpawnRotation,
		const FVector& SpawnScale);

	// Query helpers
	const UItemDefinition* GetSourceItemDefinition() const;
	bool CanServerUseRangedWeapon(
		const ACharacterBase* AttackingCharacter,
		bool bRequirePlayerAim) const;
	bool TryGetOwnerMeshSocketLocation(const ACharacterBase* Character, FName SocketName, FVector& OutLocation) const;
	bool ResolveServerAimViewPoint(
		const APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		FVector& OutViewLocation,
		FVector& OutViewDirection) const;
	bool ResolveAimTargetBeyondLaunchPoint(
		const FVector& ViewLocation,
		const FVector& ViewDirection,
		const FVector& LaunchStartLocation,
		float TraceRange,
		const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
		const TArray<AActor*>& ActorsToIgnore,
		EDrawDebugTrace::Type DebugDrawType,
		FVector& OutTargetLocation,
		FHitResult* OutAimHitResult = nullptr) const;
	float GetWeaponAttackSpeedPlayRate() const;
	virtual UAnimMontage* GetConfiguredWeaponMontage() const;
	virtual FName GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const;

	// Action helpers
	ACharacterBase* GetOwningCharacter() const;
	bool IsCurrentWeaponForOwner() const;
	bool CanDamageMeleeTracedHit(const FHitResult& HitResult) const;
	FVector GetAttackTraceEndLocation(const FVector& TraceStartLocation) const;
	bool IsAttackDebugVisualizationEnabled() const;
	bool PlayConfiguredWeaponMontage(FName StartingSection, float PlayRate);
	void StartAttackTraceInternal(bool bResetHitActors);
	void PerformAttackTrace();
	bool ApplyDamageFromAuthoritativeMeleeTrace(const FHitResult& HitResult);
	bool ApplyDamageFromAuthoritativeRangedTrace(const FHitResult& HitResult);
	bool ApplyDamageToTarget(AActor* TargetActor);
	bool HasActiveSkillAdditionalDamage() const;
	void ApplyActiveSkillAdditionalDamageToTarget(ACharacterBase* TargetCharacter);
	void ApplySkillDebuffToTarget(
		ACharacterBase* TargetCharacter,
		const FGameplayEffectSpecHandle& DebuffEffectSpecHandle,
		UStatusEffectDefinition* StatusEffectDefinition);
	void ApplySkillAdditionalDamageToTarget(
		ACharacterBase* TargetCharacter,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		FGameplayTag DamageDataTag,
		float DamageMagnitude,
		int32 DamageLevel,
		UObject* DamageSourceObject,
		const FGameplayEffectSpecHandle& DebuffEffectSpecHandle,
		UStatusEffectDefinition* StatusEffectDefinition);
	void ClearActiveSkillAdditionalDamage();
	bool ApplySkillWeaponTrailVisual(bool bActivate);
	void SpawnSkillSlashNiagara();
	bool SpawnSkillSlashNiagaraLocal(
		UNiagaraSystem* SlashSystem,
		const FVector& SpawnLocation,
		const FRotator& SpawnRotation,
		const FVector& SpawnScale);
	bool ConsumeMatchingPredictedSkillSlash(
		UNiagaraSystem* SlashSystem,
		const FVector& SpawnLocation);
	UNiagaraComponent* ResolveSkillTrailComponent() const;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDrawInterpolatedAttackTraceDebug(
		const TArray<FVector>& StartLocations,
		const TArray<FVector>& EndLocations,
		const TArray<FHitResult>& Hits);

	void DrawInterpolatedAttackTraceDebug(
		const TArray<FVector>& StartLocations,
		const TArray<FVector>& EndLocations,
		const TArray<FHitResult>& Hits) const;

private:
	friend class AArrowProjectileBase;

	bool ApplyDamageFromAuthoritativeProjectileImpact(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent,
		const AArrowProjectileBase* ProjectileSource);

protected:
	UPROPERTY(Replicated, Transient)
	TObjectPtr<UItemDefinition> SourceItemDefinition;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActorsInCurrentAttack;

	UPROPERTY(Transient)
	FName TrackedAttackSectionName = NAME_None;

	UPROPERTY(Transient)
	bool bAttackTraceActive = false;

	UPROPERTY(Transient)
	bool bHasPreviousAttackTraceSegment = false;

	UPROPERTY(Transient)
	FVector PreviousAttackTraceStartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector PreviousAttackTraceEndLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> ActiveSkillTrailSystem;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> ActiveSkillSlashSystem;

	UPROPERTY(Transient)
	FVector ActiveSkillSlashScale = FVector::OneVector;

	UPROPERTY(Transient)
	FVector ActiveSkillSlashSpawnLocationOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	FName ActiveSkillSlashSpawnSocketName = NAME_None;

	UPROPERTY(Transient)
	FRotator ActiveSkillSlashSpawnRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float ActiveSkillAttackTraceEndMultiplier = 1.0f;

	UPROPERTY(Transient)
	bool bSkillSlashHitTraceEnabled = false;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ActiveSkillAdditionalDamageEffectClass;

	UPROPERTY(Transient)
	FGameplayTag ActiveSkillAdditionalDamageDataTag;

	UPROPERTY(Transient)
	float ActiveSkillAdditionalDamageMagnitude = 0.0f;

	UPROPERTY(Transient)
	int32 ActiveSkillAdditionalDamageLevel = 1;

	UPROPERTY(Transient)
	TObjectPtr<UObject> ActiveSkillAdditionalDamageSourceObject;

	FGameplayEffectSpecHandle ActiveSkillDebuffEffectSpecHandle;

	UPROPERTY(Transient)
	TObjectPtr<UStatusEffectDefinition> ActiveSkillStatusEffectDefinition;

	TWeakObjectPtr<UNiagaraSystem> PredictedSkillSlashSystem;
	FVector PredictedSkillSlashLocation = FVector::ZeroVector;
	double PredictedSkillSlashWorldTime = -1.0;
	bool bHasPendingPredictedSkillSlash = false;

	UPROPERTY(Transient)
	float ActiveSkillAdditionalDamageDelay = 0.12f;

	TMap<FObjectKey, float> TemporaryAttackTraceEndZMultipliers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace", meta = (ClampMin = "0.001"))
	float AttackTraceInterval = 0.033333f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace",
		meta = (ClampMin = "0.0", ClampMax = "20.0", ForceUnits = "cm"))
	float AttackTraceRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug")
	bool bDrawAttackTraceDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug", meta = (ClampMin = "0.0"))
	float AttackTraceDebugDrawTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug")
	FLinearColor AttackTraceDebugTraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug")
	FLinearColor AttackTraceDebugHitColor = FLinearColor::Green;

	FTimerHandle AttackTraceTimerHandle;
};
