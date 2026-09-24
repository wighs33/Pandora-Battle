#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForceMoveGateActor.generated.h"

class APdPlayerController;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class LABPROJECT_API AForceMoveGateActor : public AActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	AForceMoveGateActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "!ForceMoveGate")
	bool ShouldRaiseWhenForceMoveTriggered() const { return bRaiseWhenForceMoveTriggered; }

	UFUNCTION(BlueprintPure, Category = "!ForceMoveGate")
	bool IsGateRaised() const { return bGateRaised; }

private:
	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayGateMovement(bool bInRaised, APdPlayerController* TriggeringPlayerController);

public:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!ForceMoveGate")
	void HandleForceMoveTriggered(APdPlayerController* TriggeringPlayerController);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "!ForceMoveGate", meta = (DisplayName = "On Gate Raise Started"))
	void BP_OnGateRaiseStarted(APdPlayerController* TriggeringPlayerController);

	UFUNCTION(BlueprintImplementableEvent, Category = "!ForceMoveGate", meta = (DisplayName = "On Gate Lower Started"))
	void BP_OnGateLowerStarted();

private:
	UFUNCTION()
	void OnRep_GateRaised();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyGateRaisedStateImmediately();
	void StartGateMovement(bool bInRaised, APdPlayerController* TriggeringPlayerController);
	void FinishGateMovement();
	FVector GetRaisedRelativeLocation() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!ForceMoveGate")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!ForceMoveGate")
	TObjectPtr<UStaticMeshComponent> GateMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!ForceMoveGate")
	bool bRaiseWhenForceMoveTriggered = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!ForceMoveGate")
	bool bStartRaised = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!ForceMoveGate")
	FVector RaiseOffset = FVector(0.0f, 0.0f, 400.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!ForceMoveGate", meta = (ClampMin = "0.0"))
	float RaiseDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!ForceMoveGate")
	bool bLogGateMovement = true;

private:
	UPROPERTY(ReplicatedUsing = OnRep_GateRaised)
	bool bGateRaised = false;

	FVector ClosedRelativeLocation = FVector::ZeroVector;
	FVector MovementStartRelativeLocation = FVector::ZeroVector;
	FVector MovementTargetRelativeLocation = FVector::ZeroVector;
	float MovementElapsedSeconds = 0.0f;
	bool bMovementActive = false;
	bool bInitializedClosedLocation = false;
};
