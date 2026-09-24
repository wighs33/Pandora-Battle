#pragma once

#include "DetourCrowdAIController.h"
#include "TrainingBotAIController.generated.h"

class UBehaviorTree;

UCLASS(Blueprintable)
class LABPROJECT_API ATrainingBotAIController : public ADetourCrowdAIController
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	// Public API ------------------------------------------------------------------------------------------------------
	ATrainingBotAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!AI|Training Bot")
	void SetBlackboardTarget(AActor* InTarget);

	UFUNCTION(BlueprintCallable, Category = "!AI|Training Bot")
	void ClearBlackboardTarget();

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!AI|Training Bot")
	void RefreshTargetFromPlayers();

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void InitializeBlackboardValues(APawn* InPawn);
	AActor* FindBestPlayerTarget() const;
	void StartTargetRefreshTimer();
	void StopTargetRefreshTimer();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Training Bot")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Training Bot", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float TargetRefreshInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Blackboard")
	FName TargetActorKeyName = TEXT("TargetActor");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Blackboard")
	FName LastKnownTargetLocationKeyName = TEXT("LastKnownTargetLocation");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Blackboard")
	FName DistanceToTargetKeyName = TEXT("DistanceToTarget");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Blackboard")
	FName AttackRangeKeyName = TEXT("AttackRange");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Blackboard")
	FName HasRangedWeaponKeyName = TEXT("bHasRangedWeapon");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Blackboard")
	FName HasLineOfSightKeyName = TEXT("bHasLineOfSight");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Blackboard")
	FName SpawnLocationKeyName = TEXT("SpawnLocation");

	FTimerHandle TargetRefreshTimerHandle;
};
