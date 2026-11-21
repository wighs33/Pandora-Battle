#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_StrafeFlight.generated.h"

class ACharacter;

/**
 * 구간 동안 공중에서 지금 바라보는 쪽(공격 대상 쪽)으로 곧게 날아간다. 날개가 경기장 벽이나 건물에 겹치지 않게, 날개 끝이 닿는
 * 원기둥(몸 중심축에서 반지름 WingReach, 높이 WingTop) 안이 빈 자리까지만 가고, 그 거리를 구간 안에 일정한 속도로 간다.
 * 날개는 이륙·착지 때도 다 펴지므로, 지금 자리에서부터 원기둥이 비어 있지 않으면 이 노티파이가 든 공격은 고르지 않는다(HasRoomToFly).
 *
 * 제자리 비행 애니메이션과 함께 쓴다. 서버가 루트 모션 소스로 캡슐을 옮기므로 원격 화면 동기화는 이동 컴포넌트가 맡는다.
 * 몸은 공중에 있고 캡슐만 땅에 남으므로, 구간 동안 캡슐이 플레이어를 막거나 플레이어에게 막히지 않게 모든 화면에서 폰 충돌을 끈다.
 */
UCLASS(meta = (DisplayName = "Strafe Flight"))
class LABPROJECT_API UAnimNotifyState_StrafeFlight : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 지금 자리와 방향에서 날개를 펴도 벽이나 건물에 닿지 않는지. 이 노티파이가 든 공격을 고르기 전에 몬스터가 묻는다. */
	bool HasRoomToFly(const ACharacter& Character) const;

protected:
	/** 가장 빠른 비행 속도. 갈 수 있는 거리가 짧으면 구간 안에 그 거리만큼만 가도록 늦춘다. */
	UPROPERTY(EditAnywhere, Category = "!Flight", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float FlightSpeed = 900.0f;

	/** 날개 끝이 몸 중심축에서 닿는 가장 먼 수평 거리. 비행 몽타주의 이륙부터 착지까지 모든 자세를 덮도록 잰 값이다. */
	UPROPERTY(EditAnywhere, Category = "!Flight", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float WingReach = 1800.0f;

	/** 날개 끝이 닿는 가장 높은 곳(발 기준). */
	UPROPERTY(EditAnywhere, Category = "!Flight", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float WingTop = 2400.0f;

private:
	/**
	 * 발이 Feet에 있을 때 날개 원기둥 안에 벽이나 건물이 없는지. 여러 높이에서 축으로부터 사방으로 WingReach만큼 선을 그어 본다.
	 * bFrontOnly면 Forward 쪽 반원만 본다. 앞으로 나아가며 살필 때 뒤쪽 반원은 앞서 살핀 자리와 거의 겹치기 때문이다.
	 */
	bool IsWingSpaceClear(const ACharacter& Character, const FVector& Feet, const FVector& Forward, bool bFrontOnly) const;

	/** 지금 자리에서 정면으로 MaxDistance까지 가는 동안 날개가 벽이나 건물에 닿지 않는 가장 먼 거리. */
	float FindClearDistance(const ACharacter& Character, float MaxDistance) const;

	/** 캐릭터의 발 위치. */
	static FVector GetFeetLocation(const ACharacter& Character);

	/** 비행 구간 동안 캡슐과 폰 사이의 충돌을 끄고, 끝나면 클래스 기본값으로 되돌린다. */
	static void SetCapsuleBlocksPawns(ACharacter& Character, bool bBlocks);
};
