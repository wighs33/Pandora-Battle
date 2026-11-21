#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_BreathAttack.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 입에서 뿜는 불길. 구간 동안 불꽃 이펙트를 입에서 불길 끝까지 매 프레임 이어 놓고(화면을 그리는 인스턴스), 서버는 같은 자리·방향으로
 * 뻗은 불길에 닿은 대상에게 몬스터의 공격 피해를 준다.
 *
 * 불길 방향은 머리 뼈를 따르지 않고, 입에서 몸 중심 정면 AimDistance 앞의 바닥을 겨눈다. 머리 애니메이션이 흔들리거나 오르내려도
 * 불길은 늘 나는 길 한가운데에 떨어지고, 이펙트와 판정이 같은 방향을 쓰므로 보이는 불길과 맞는 자리가 같다.
 * 불길은 입에서 그 방향으로 Reach까지 뻗다가 바닥이나 벽에 닿으면 멈추는 선분이고, 굵기는 Radius다.
 * 한 대상은 HitInterval마다 한 번씩 맞으므로, 불길이 스쳐 가면 한 번, 불길 속에 머물면 계속 맞는다.
 *
 * 불길이 바닥에 닿은 자리에는 ImpactInterval마다 바닥 불을 띄워 ImpactLifetime 동안 태운다. 입의 불꽃은 길이가 들쭉날쭉해
 * 바닥에 닿는 모습이 늘 보이지는 않으므로, 지금 맞는 자리와 방금 훑고 지나간 자리를 바닥 불로 보여 준다.
 */
UCLASS(meta = (DisplayName = "Breath Attack"))
class LABPROJECT_API UAnimNotifyState_BreathAttack : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 불꽃 이펙트. 구간 동안 켜 두었다가 끝나면 남은 불꽃이 사그라들게 끈다. */
	UPROPERTY(EditAnywhere, Category = "!Breath")
	TObjectPtr<UNiagaraSystem> Effect;

	/** 입에서 불길이 닿는 곳까지 같은 이펙트를 이 개수만큼 고르게 이어 놓는다. 이펙트의 불꽃이 입에서 바닥까지보다 짧을 때 늘린다. */
	UPROPERTY(EditAnywhere, Category = "!Breath", meta = (ClampMin = "1", ClampMax = "4"))
	int32 FlameSegments = 1;

	/** 불길이 시작하는 입 소켓. */
	UPROPERTY(EditAnywhere, Category = "!Breath", meta = (AnimNotifyBoneName = "true"))
	FName SocketName;

	/** 소켓 기준 불길 시작 위치. */
	UPROPERTY(EditAnywhere, Category = "!Breath")
	FVector LocationOffset = FVector::ZeroVector;

	/** 불길이 떨어지는 자리: 몸 중심에서 정면으로 이만큼 앞의 바닥. 이펙트 정면(X축)을 입에서 이 자리로 돌린다. */
	UPROPERTY(EditAnywhere, Category = "!Breath", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float AimDistance = 1000.0f;

	/** 불길이 바닥에 닿은 자리에 띄우는 불. 비우면 띄우지 않는다. 계속 타는(반복) 이펙트여도 ImpactLifetime 뒤에 끈다. */
	UPROPERTY(EditAnywhere, Category = "!Breath|Impact")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere, Category = "!Breath|Impact", meta = (ClampMin = "0.01"))
	float ImpactEffectScale = 1.0f;

	/** 바닥 불을 띄우는 간격. 나는 속도와 곱한 거리마다 불이 하나씩 놓인다. */
	UPROPERTY(EditAnywhere, Category = "!Breath|Impact", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float ImpactInterval = 0.2f;

	/** 바닥 불을 켜 두는 시간. 끈 뒤에는 남은 불꽃이 사그라든다. */
	UPROPERTY(EditAnywhere, Category = "!Breath|Impact", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float ImpactLifetime = 0.8f;

	/** 불길이 닿는 가장 먼 거리. 입이 가장 높이 떴을 때도 바닥에 닿을 만큼 둔다. */
	UPROPERTY(EditAnywhere, Category = "!Breath|Damage", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float Reach = 3000.0f;

	/** 불길 굵기(반지름). 몸이 이 안에 들면 맞는다. */
	UPROPERTY(EditAnywhere, Category = "!Breath|Damage", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float Radius = 250.0f;

	/** 한 대상이 다시 맞기까지의 간격. */
	UPROPERTY(EditAnywhere, Category = "!Breath|Damage", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float HitInterval = 0.75f;

private:
	/** 불길이 시작하는 입 위치. */
	FVector GetFlameStart(const USkeletalMeshComponent& MeshComp) const;

	/** Start에서 몸 중심 정면 AimDistance 앞의 바닥을 향하는 방향. */
	FRotator GetAimRotation(const USkeletalMeshComponent& MeshComp, const FVector& Start) const;

	/** Start에서 Aim 방향으로 Reach까지 뻗은 불길의 끝. 바닥이나 벽에 닿으면 OutHit에 닿은 곳을 담고 그 자리에서 끝난다. */
	FVector TraceFlame(const USkeletalMeshComponent& MeshComp, const FVector& Start, const FRotator& Aim, FHitResult& OutHit) const;

	/** 화면을 그리는 인스턴스: 이번 프레임에 ImpactInterval 경계를 넘었으면 불길이 닿은 바닥에 불을 띄운다. */
	void SpawnImpactEffect(const USkeletalMeshComponent& MeshComp, const FHitResult& FlameHit, float FrameDeltaTime) const;

	/** 이 노티파이가 이 메시에 띄운 Segment번째 불꽃. 화면을 그리지 않는 인스턴스나 구간 밖에서는 없다. */
	UNiagaraComponent* FindSpawnedEffect(const USkeletalMeshComponent& MeshComp, int32 Segment) const;

	/** 불꽃들을 입(Start)에서 불길 끝(End)까지 고르게 놓고 Aim 방향으로 돌린다. */
	void PlaceSpawnedEffects(const USkeletalMeshComponent& MeshComp, const FVector& Start, const FVector& End, const FRotator& Aim) const;

	/** 이 노티파이가 띄운 Segment번째 불꽃 컴포넌트에 붙여, 같은 메시의 다른 이펙트와 구분한다. */
	FName GetSpawnedComponentTag(int32 Segment) const;
};
