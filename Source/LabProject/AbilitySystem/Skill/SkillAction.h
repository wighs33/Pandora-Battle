#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "UObject/Object.h"
#include "SkillAction.generated.h"

class USkillAbility;
class USkillAction;

/** 같은 실행 단계에 참여하는 기능들이 공유하는 대상과 위치. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillActionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Skill")
	FGameplayEventData EventData;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FSkillActionFinished, USkillAction*, bool);

/** 에셋에는 설정만 저장한다. 실행할 때 전체 트리를 복제해 시전자별 상태를 분리한다. */
UCLASS(Abstract, BlueprintType, EditInlineNew, DefaultToInstanced)
class LABPROJECT_API USkillAction : public UObject
{
	GENERATED_BODY()

public:
	void Start(USkillAbility* InAbility, const FSkillActionContext& InContext);
	void Cancel();
	bool IsRunning() const { return bRunning; }
	const FSkillActionContext& GetResultContext() const { return ExecutionContext; }
	FSkillActionFinished OnFinished;
	virtual UWorld* GetWorld() const override;

protected:
	virtual void OnStart() PURE_VIRTUAL(USkillAction::OnStart, );
	virtual void OnStop() {}
	void Finish(bool bSucceeded = true);
	USkillAbility* GetAbility() const { return OwningAbility.Get(); }
	const FSkillActionContext& GetContext() const { return ExecutionContext; }
	void SetContext(const FSkillActionContext& InContext) { ExecutionContext = InContext; }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<USkillAbility> OwningAbility;

	UPROPERTY(Transient)
	FSkillActionContext ExecutionContext;

	bool bRunning = false;
};

/** 자식 기능을 앞에서부터 실행한다. 앞 단계의 실패는 뒤 단계 실행을 막는다. */
UCLASS(meta = (DisplayName = "Sequence"))
class LABPROJECT_API USkillSequenceAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = "Skill")
	TArray<TObjectPtr<USkillAction>> Actions;
protected:
	virtual void OnStart() override;
	virtual void OnStop() override;
private:
	void StartNext();
	void ChildFinished(USkillAction* Child, bool bSucceeded);
	int32 NextIndex = 0;
};

/** 자식 기능을 모두 시작하고 모두 끝날 때 완료한다. 하나가 실패하면 나머지도 정리한다. */
UCLASS(meta = (DisplayName = "Parallel"))
class LABPROJECT_API USkillParallelAction : public USkillAction
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Instanced, Category = "Skill")
	TArray<TObjectPtr<USkillAction>> Actions;
protected:
	virtual void OnStart() override;
	virtual void OnStop() override;
private:
	void ChildFinished(USkillAction* Child, bool bSucceeded);
	int32 Remaining = 0;
};
