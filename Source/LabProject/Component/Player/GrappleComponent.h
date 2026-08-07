#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetSerialization.h"
#include "TimerManager.h"
#include "GrappleComponent.generated.h"

class APdPlayer;
class UCableComponent;
class UPrimitiveComponent;

DECLARE_MULTICAST_DELEGATE(FGrappleStateDelegate);

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UGrappleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrappleComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetHookComponent(UCableComponent* InHookComponent);

	bool TraceGrappleFromView(const FVector& ViewLocation, const FVector& ViewDirection, FHitResult& OutHitResult) const;

	bool ValidateTargetDataAndTrace(const FHitResult& ClientHitResult, FHitResult& OutServerHitResult) const;

	bool StartGrappleFromValidatedHit(const FHitResult& HitResult);

	void StopGrapple();

	void ResetForRespawn();

	bool IsGrappling() const { return bIsGrappling; }

	void ConfigureHookComponent();

	FGrappleStateDelegate OnGrappleStarted;
	FGrappleStateDelegate OnGrappleFinished;

private:
	UFUNCTION()
	void FinishGrapple();

	UFUNCTION()
	void AttachGrappleHookToTarget();

	UFUNCTION()
	void StartGrappleMove();

	UFUNCTION()
	void OnRep_IsGrappling(bool bWasGrappling);

	UFUNCTION()
	void OnRep_GrappleTarget();

	UFUNCTION(Client, Reliable)
	void ClientCorrectGrappleEnd(
		FVector_NetQuantize100 ServerLocation,
		uint8 ServerMovementMode,
		uint8 ServerCustomMovementMode);

	APdPlayer* GetPlayerOwner() const;
	void SetGrappleState(bool bNewIsGrappling);
	void SetGrappleTarget(UPrimitiveComponent* HitComponent, const FVector& ImpactPoint);
	void ClearGrappleTarget();
	void MarkGrappleReplicationDirty();
	void ApplyReplicatedGrappleState();
	void ResetGrappleHookVisual();
	bool ResolveSafeGrappleDestination(const FHitResult& HitResult, FVector& OutDestination) const;
	bool IsCapsuleLocationClear(const FVector& Location) const;
	bool RecoverPlayerFromPenetration();
	void RestoreMovementAfterGrapple();
	void ResetGrappleMovementState();

private:
	UPROPERTY(Transient)
	TObjectPtr<UCableComponent> HookComponent;

	UPROPERTY(EditAnywhere, Category = "!Grapple", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TraceStartOffset = 300.0;

	UPROPERTY(EditAnywhere, Category = "!Grapple", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TraceDistance = 3000.0;

	UPROPERTY(EditAnywhere, Category = "!Grapple", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double TraceRadius = 25.0;

	UPROPERTY(EditAnywhere, Category = "!Grapple")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	UPROPERTY(EditAnywhere, Category = "!Grapple", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double MoveDuration = 0.5;

	UPROPERTY(EditAnywhere, Category = "!Grapple", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double MoveStartDelay = 0.2;

	UPROPERTY(EditAnywhere, Category = "!Grapple", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double HookAttachDelay = 0.15;

	UPROPERTY(EditAnywhere, Category = "!Grapple|Safety", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double EndpointClearance = 10.0;

	UPROPERTY(EditAnywhere, Category = "!Grapple")
	bool bDrawTraceDebug = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsGrappling, Transient)
	bool bIsGrappling = false;

	UPROPERTY(ReplicatedUsing = OnRep_GrappleTarget, Transient)
	TObjectPtr<UPrimitiveComponent> PendingGrappleHitComponent;

	UPROPERTY(ReplicatedUsing = OnRep_GrappleTarget, Transient)
	FVector_NetQuantize PendingGrappleImpactPoint = FVector::ZeroVector;

	FTimerHandle GrappleMoveStartTimerHandle;
	FTimerHandle GrappleHookAttachTimerHandle;

	FVector GrappleMoveStartLocation = FVector::ZeroVector;
	FVector GrappleMoveDestination = FVector::ZeroVector;
	FVector LastSafeGrappleLocation = FVector::ZeroVector;
	FRotator GrappleMoveStartRotation = FRotator::ZeroRotator;
	FRotator GrappleMoveTargetRotation = FRotator::ZeroRotator;
	double GrappleMoveElapsedTime = 0.0;
	TEnumAsByte<EMovementMode> MovementModeBeforeGrapple = MOVE_Walking;
	uint8 CustomMovementModeBeforeGrapple = 0;
	bool bGrappleMoveActive = false;
	bool bMovementModeOverridden = false;
};
