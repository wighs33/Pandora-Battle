#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MonsterAIController.generated.h"

class UStateTree;
class UStateTreeAIComponent;
class UExperienceDefinition;
class UExperienceManagerComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class ANavigationData;
struct FAIStimulus;

UCLASS(Blueprintable)
class LABPROJECT_API AMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMonsterAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category = "!AI|Monster")
	UStateTreeAIComponent* GetStateTreeAI() const { return NativeStateTreeAI; }

	UFUNCTION(BlueprintPure, Category = "!AI|Monster")
	UAIPerceptionComponent* GetMonsterPerceptionComponent() const { return AIPerception; }

	UFUNCTION(BlueprintPure, Category = "!AI|Monster")
	APawn* GetPerceivedPlayerPawn() const;

	/** Revalidates the remembered target and selects the closest perceived player when needed. */
	void RefreshPerceivedPlayerPawn(
		const AActor* ExcludedActor = nullptr,
		APawn* NewlySensedPawn = nullptr);

	void ForgetAllPerceivedActors();

	/** Forgets only when the current target has moved beyond the sight retention radius. */
	bool ForgetPerceivedPlayerIfOutOfRange();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!AI|Monster", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> NativeStateTreeAI;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> NativeSightConfig;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APawn> PerceivedPlayerPawn;

	UPROPERTY(Transient)
	TObjectPtr<UStateTree> ResolvedMonsterStateTree;

	bool ConfigureStateTreeAI();
	bool ResolveMonsterStateTreeFromExperience();
	bool HasRequiredNavigationData() const;
	void ConfigurePerception();
	void GatherComponentConfigurationErrors(TArray<FText>& OutErrors) const;
	bool ValidateComponentConfiguration();
	bool IsValidPerceivedPlayerTarget(APawn* PlayerPawn) const;
	bool IsWithinTargetRetentionDistance(const AActor* TargetActor) const;
	void SetPerceivedPlayerPawn(APawn* PlayerPawn);
	void StopWaitingForExperience();
	void WaitForNavigationData();
	void StopWaitingForNavigationData();
	void StartMonsterStateTreeIfReady();

	void HandleExperienceLoaded(const UExperienceDefinition* Experience);

	UFUNCTION()
	void HandleNavigationDataAvailable(ANavigationData* NavigationData);

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleTargetPerceptionForgotten(AActor* Actor);

	TWeakObjectPtr<UExperienceManagerComponent> ExperienceManagerWaitingForLoad;
	FDelegateHandle ExperienceLoadedDelegateHandle;
	bool bStateTreeConfigured = false;
	bool bWaitingForNavigationData = false;
	bool bLoggedConfigurationError = false;
	bool bLoggedComponentConfigurationError = false;
	bool bLoggedNavigationError = false;
};
