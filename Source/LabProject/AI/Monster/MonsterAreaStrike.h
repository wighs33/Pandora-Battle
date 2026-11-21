#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MonsterAreaStrike.generated.h"

class UMaterialInterface;
class UNiagaraSystem;

/**
 * 몬스터가 바닥 한 곳에 떨어뜨리는 범위 공격 하나(메테오).
 *
 * 서버가 떨어질 자리에 만들고 복제한다. 각 화면은 생기는 순간부터 그 자리에 원을 그려 ImpactDelay 동안 채우고,
 * 떨어지기 FallLeadTime 전에 낙하 효과, 떨어지는 순간 폭발 효과를 띄운다. 서버는 떨어지는 순간 원 안의 대상에게
 * 이 공격을 만든 몬스터(Owner)의 공격 피해를 준다. 효과와 수치는 BP 기본값으로 정한다.
 */
UCLASS(Blueprintable)
class LABPROJECT_API AMonsterAreaStrike : public AActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;

	// Public API ------------------------------------------------------------------------------------------------------
	AMonsterAreaStrike();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Strike", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float Radius = 350.0f;

	/** 생긴 뒤 떨어질 때까지의 시간. 바닥 원은 이 시간 동안 가득 찬다. */
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Strike", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float ImpactDelay = 1.8f;

	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Strike")
	TObjectPtr<UMaterialInterface> TelegraphMaterial;

	/** 떨어지기 전에 띄우는 낙하 효과. 떨어지는 자리를 원점으로 만든 효과를 쓴다. */
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Strike")
	TObjectPtr<UNiagaraSystem> FallEffect;

	/** 낙하 효과를 떨어지는 순간보다 이만큼 먼저 띄운다. */
	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Strike", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float FallLeadTime = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "!AI|Monster|Strike")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

private:
	void PlayFallEffect();
	void Impact();

	FTimerHandle FallEffectTimerHandle;
	FTimerHandle ImpactTimerHandle;
};
