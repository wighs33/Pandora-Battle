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
class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMeshComponent;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	// Actor lifecycle
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Commands
	UFUNCTION(BlueprintCallable, Category = "!Weapon|Damage")
	void RequestServerApplyDamage(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
	void SetBeginOverlapEnabled(bool bEnabled);

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

protected:
	// Delegate callbacks
	UFUNCTION()
	void OnCollisionBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

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
	virtual UBoxComponent* GetCollisionBox() const;
	void InitializeCollisionBox(UBoxComponent* CollisionBox);
	virtual UAnimMontage* GetConfiguredWeaponMontage() const;
	virtual FName GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const;

	// Action helpers
	APdCharacterBase* GetOwningCharacter() const;
	bool CanProcessOverlapWith(AActor* OtherActor, UPrimitiveComponent* OtherComp) const;
	bool TryTraceOverlapTarget(UPrimitiveComponent* OtherComp, FHitResult& OutHitResult) const;
	void DebugSuccessfulHit(const FHitResult& HitResult) const;
	void ApplyDamageToTarget(AActor* TargetActor);

	UPROPERTY(Replicated, Transient)
	TObjectPtr<UItemDefinition> SourceItemDefinition;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActorsInCurrentAttack;

};
