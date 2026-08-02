#pragma once

#include "AIController.h"
#include "TimerManager.h"
#include "PetAIController.generated.h"

class UBehaviorTree;

UCLASS(Blueprintable)
class LABPROJECT_API APetAIController : public AAIController
{
	GENERATED_BODY()

public:
	APetAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION(BlueprintCallable, Category = "!AI|Pet")
	void RefreshFollowTarget();

protected:
	void InitializeBlackboardValues(APawn* InPawn);
	AActor* ResolveFollowTarget() const;
	void UpdateDirectFollowFallback(float DeltaSeconds);
	void StartFollowTargetRefreshTimer();
	void StopFollowTargetRefreshTimer();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Pet")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Pet", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float FollowTargetRefreshInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Pet", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float DirectMoveAcceptanceRadius = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Pet")
	bool bUsePathfindingForFollow = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Pet")
	FVector DirectFollowOffset = FVector(-140.0f, 80.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Pet", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float DirectFollowTeleportDistance = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Blackboard")
	FName FollowTargetActorKeyName = TEXT("FollowTargetActor");

	FTimerHandle FollowTargetRefreshTimerHandle;
	bool bBehaviorTreeRunning = false;
	bool bDirectFollowFallbackActive = false;
};
