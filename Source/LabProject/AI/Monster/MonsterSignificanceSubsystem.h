#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MonsterSignificanceSubsystem.generated.h"

class AMonsterCharacter;

/**
 * 클라이언트에서 화면에 보이지 않는 몬스터의 이동 시뮬레이션을 느리게 돌린다.
 *
 * 복제받은 몬스터(시뮬레이티드 프록시)의 이동은 매 프레임 캡슐 스윕과 바닥 검사를 하는데, 화면 밖의 몬스터는 위치가 조금 늦게 따라와도 보이지 않는다.
 * 서버 위치는 복제로 계속 받으므로, 다시 화면에 들어오면 원래 주기로 돌아가 보간이 이어진다. 판정은 서버가 하므로 게임 결과는 바뀌지 않는다.
 */
UCLASS()
class LABPROJECT_API UMonsterSignificanceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	void RegisterMonster(AMonsterCharacter* Monster);
	void UnregisterMonster(AMonsterCharacter* Monster);

private:
	void UpdateMovementTickIntervals();

	TArray<TWeakObjectPtr<AMonsterCharacter>> Monsters;
	FTimerHandle UpdateTimerHandle;
};
