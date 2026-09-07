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
struct FStreamableHandle;

/**
 * 몬스터의 감지 대상과 StateTree 실행 수명을 관리한다.
 *
 * 조종 중인 몬스터의 정의와 초기화 완료를 기준으로 행동을 시작하며, 공격과 사망 표현은 캐릭터에 맡긴다.
 */

UCLASS(Blueprintable)
class LABPROJECT_API AMonsterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AMonsterAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
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

	/** 현재 표적을 유지할 수 없을 때 감지된 플레이어 중 다음 표적을 선택한다. */
	void RefreshPerceivedPlayerPawn(
		const AActor* ExcludedActor = nullptr,
		APawn* NewlySensedPawn = nullptr);

	/** 기억 유지 거리를 벗어난 현재 표적만 잊는다. */
	bool ForgetPerceivedPlayerIfOutOfRange();
	void StartMonsterStateTreeIfReady();
	void StopMonsterAI();

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!AI|Monster", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> NativeStateTreeAI;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	//------------------------------------------------------------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> NativeSightConfig;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!AI|Perception", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APawn> PerceivedPlayerPawn;

	UPROPERTY(Transient)
	TObjectPtr<UStateTree> ResolvedMonsterStateTree;

	bool ConfigureStateTreeAI();
	bool IsExperienceReadyOrWait();
	bool ResolveMonsterStateTreeFromEnemyDefinition();
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
	void HandleMonsterStateTreeLoaded(uint32 RequestGeneration);

	void HandleExperienceLoaded(const UExperienceDefinition* Experience);

	UFUNCTION()
	void HandleNavigationDataAvailable(ANavigationData* NavigationData);

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleTargetPerceptionForgotten(AActor* Actor);

	TWeakObjectPtr<UExperienceManagerComponent> ExperienceManagerWaitingForLoad;
	FDelegateHandle ExperienceLoadedDelegateHandle;
	TSharedPtr<FStreamableHandle> StateTreeLoadHandle;
	uint32 StateTreeLoadGeneration = 0;
	bool bComponentConfigurationValid = false;
	bool bAIStopped = true;
	bool bStateTreeConfigured = false;
	bool bWaitingForNavigationData = false;
	bool bLoggedConfigurationError = false;
	bool bLoggedComponentConfigurationError = false;
	bool bLoggedNavigationError = false;
};
