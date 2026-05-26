#pragma once

#include "CoreMinimal.h"
#include "Common/WeaponDefinitionData.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "WeaponBase.generated.h"

class APdCharacterBase;
class APdPlayer;
class UItemDefinition;
class UAnimMontage;
class USceneComponent;
class USkeletalMeshComponent;

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
	UFUNCTION(BlueprintCallable, Category = "!Weapon|Damage")
	void RequestServerApplyDamage(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
	void SetBeginOverlapEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
	void StartAttackTrace();

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
	void StopAttackTrace();

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Animation")
	bool PlayWeaponMontage(FName StartingSection = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Animation")
	bool JumpToWeaponMontageSectionAndResume(FName SectionName);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Animation")
	void StopWeaponMontage(float BlendOutTime = 0.1f);

	void InitializeFromItemDefinition(const UItemDefinition* InItemDefinition);

	// Query helpers
	bool SupportsAimInput() const;
	FGameplayTag GetAimCrosshairWidgetTag() const;
	const FWeaponAimCameraSettings& GetAimCameraSettings() const;

	// Input commands
	virtual bool HandleAimStart(APdPlayer* PlayerCharacter);
	virtual void HandleAimEnd(APdPlayer* PlayerCharacter);
	virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter);
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
	UFUNCTION(Server, Reliable)
	void ServerApplyDamage(AActor* TargetActor);

	// Query helpers
	const UItemDefinition* GetSourceItemDefinition() const;
	bool TryGetOwnerMeshSocketLocation(const APdPlayer* PlayerCharacter, FName SocketName, FVector& OutLocation) const;
	bool ResolveServerAimViewPoint(
		const APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		FVector& OutViewLocation,
		FVector& OutViewDirection) const;
	bool TryGetWeaponAimTargetLocation(
		const APdPlayer* PlayerCharacter,
		float TraceRange,
		const TArray<AActor*>& ActorsToIgnore,
		FVector& OutTargetLocation) const;
	virtual UAnimMontage* GetConfiguredWeaponMontage() const;
	virtual FName GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const;

	// Action helpers
	APdCharacterBase* GetOwningCharacter() const;
	bool CanDamageTracedActor(AActor* HitActor) const;
	FVector GetAttackTraceHalfSize() const;
	void PerformAttackTrace();
	void DebugSuccessfulHit(const FHitResult& HitResult) const;
	void ApplyDamageToTarget(AActor* TargetActor);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDrawAttackTraceDebug(
		const FVector& StartLocation,
		const FVector& EndLocation,
		const FVector& HalfSize,
		const FRotator& TraceRotation,
		const TArray<FHitResult>& Hits);

	void DrawAttackTraceDebug(
		const FVector& StartLocation,
		const FVector& EndLocation,
		const FVector& HalfSize,
		const FRotator& TraceRotation,
		const TArray<FHitResult>& Hits) const;

	UPROPERTY(Replicated, Transient)
	TObjectPtr<UItemDefinition> SourceItemDefinition;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActorsInCurrentAttack;

	UPROPERTY(Transient)
	bool bAttackTraceActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace", meta = (ClampMin = "0.001"))
	float AttackTraceInterval = 0.033333f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace")
	FVector AttackTraceHalfSize = FVector(20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug")
	bool bDrawAttackTraceDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug", meta = (ClampMin = "0.0"))
	float AttackTraceDebugDrawTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug")
	FLinearColor AttackTraceDebugTraceColor = FLinearColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Weapon|Trace|Debug")
	FLinearColor AttackTraceDebugHitColor = FLinearColor::Green;

	FTimerHandle AttackTraceTimerHandle;
};
