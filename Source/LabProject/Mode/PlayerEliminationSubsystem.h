#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlayerEliminationSubsystem.generated.h"

class AActor;
class APlayerState;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlayerKillScored, APlayerState* /*KillerPlayerState*/, APlayerState* /*VictimPlayerState*/);

/**
 * 서버에서 체력이 0이 된 플레이어의 처치를 정리한다.
 *
 * 킬 로그를 모든 참가자에게 보내고 사망·처치 기록과 처치 경험치를 반영한다. 다른 플레이어가 점수를 얻었으면
 * OnPlayerKillScored로 알리며, 경기 규칙(골든킬 등)은 이 이벤트를 구독한다. AttributeSet은 체력이 0이 된 순간만 알린다.
 */
UCLASS()
class LABPROJECT_API UPlayerEliminationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void HandleEliminated(AActor* VictimActor, AActor* DamageInstigator, AActor* DamageCauser);

	FOnPlayerKillScored OnPlayerKillScored;

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BroadcastKillLog(const APlayerState* VictimPlayerState, const APlayerState* KillerPlayerState) const;
};
