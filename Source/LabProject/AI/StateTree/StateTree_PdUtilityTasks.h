#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "StateTree_PdUtilityTasks.generated.h"

class AActor;
class ACharacter;
class AMonsterAIController;
class APawn;
class UCharacterMovementComponent;

USTRUCT()
struct FStateTreePdSaveLocationTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Pawn = nullptr;

	UPROPERTY(EditAnywhere, Category = Output)
	FVector CachedLocation = FVector::ZeroVector;
};

USTRUCT(meta = (DisplayName = "Pd Save Location", Category = "AI|Data"))
struct LABPROJECT_API FStateTreePdSaveLocationTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FStateTreePdSaveLocationTaskInstanceData;

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	FStateTreePdSaveLocationTask();
};

USTRUCT()
struct FStateTreePdTrackPlayerTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AMonsterAIController> Controller = nullptr;

	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<APawn> TargetPlayerPawn = nullptr;
};

/**
 * AMonsterAIController의 네이티브 인지 경로가 기억한 플레이어를 내보낸다.
 * 인지 델리게이트는 컨트롤러가 가지므로, 이 태스크에 들어가고 나와도
 * 동적 델리게이트 바인딩이 쌓이지 않는다.
 * 들어갈 때 표적을 고르고, 틱에서는 표적이 없거나 쓸 수 없을 때만 다시 고른다.
 */
USTRUCT(meta = (DisplayName = "Pd Track Perceived Player", Category = "AI|Perception"))
struct LABPROJECT_API FStateTreePdTrackPlayerTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FStateTreePdTrackPlayerTaskInstanceData;

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	FStateTreePdTrackPlayerTask();
};

USTRUCT()
struct FStateTreePdMovementParametersTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ACharacter> Pawn = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float WalkSpeedWhileActive = 500.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "deg/s"))
	float RotationRateWhileActive = 90.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float GroundFrictionWhileActive = 0.5f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", ForceUnits = "cm/s^2"))
	float AccelerationWhileActive = 500.0f;

	// 기존 StateTree 애셋이 예전 값을 역직렬화할 수 있도록만 남겨 둔다.
	// 실행 중 복원은 태스크에 들어갈 때 잡아 둔 값을 쓴다.
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The movement values present on task entry are restored automatically."))
	float WalkSpeedOnExit = 200.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The movement values present on task entry are restored automatically."))
	float RotationRateOnExit = 360.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The movement values present on task entry are restored automatically."))
	float GroundFrictionOnExit = 8.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "The movement values present on task entry are restored automatically."))
	float AccelerationOnExit = 5000.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCharacterMovementComponent> SavedMovementComponent;

	UPROPERTY(Transient)
	float SavedMaxWalkSpeed = 0.0f;

	UPROPERTY(Transient)
	FRotator SavedRotationRate = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	float SavedGroundFriction = 0.0f;

	UPROPERTY(Transient)
	float SavedMaxAcceleration = 0.0f;

	UPROPERTY(Transient)
	bool bHasSavedMovementParameters = false;
};

/**
 * 상태가 활성인 동안 설정한 이동 값을 적용하고, 끝나면 상태에 들어갈 때
 * 같은 이동 컴포넌트에서 잡아 둔 값으로 되돌린다.
 */
USTRUCT(meta = (DisplayName = "Pd Movement Parameters", Category = "AI|Movement"))
struct LABPROJECT_API FStateTreePdMovementParametersTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FStateTreePdMovementParametersTaskInstanceData;

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	FStateTreePdMovementParametersTask();
};
