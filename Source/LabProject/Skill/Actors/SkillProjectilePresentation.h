#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SkillProjectilePresentation.generated.h"

class AActor;
class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 발사 전 대기 중인 투사체가 커지는 연출의 복제 상태.
 * 서버가 시작한 시각을 함께 복제해 클라이언트도 같은 진행도로 크기를 맞춘다.
 */
USTRUCT()
struct LABPROJECT_API FSkillProjectileGrowth
{
	GENERATED_BODY()

	UPROPERTY()
	bool bActive = false;

	UPROPERTY()
	FVector StartScale = FVector::OneVector;

	UPROPERTY()
	FVector TargetScale = FVector::OneVector;

	UPROPERTY()
	float Duration = 0.0f;

	UPROPERTY()
	float ServerStartTime = 0.0f;

	UPROPERTY()
	FName NiagaraVector2DParameterName = NAME_None;

	UPROPERTY()
	FVector2D NiagaraStartSize = FVector2D::UnitVector;

	UPROPERTY()
	FVector2D NiagaraTargetSize = FVector2D::UnitVector;

	/** 서버 시각 기준 진행도(0~1). 지속 시간이 없으면 1이다. */
	float GetAlpha(float ServerTimeSeconds) const;
	FVector GetScale(float Alpha) const;
	FVector2D GetNiagaraSize(float Alpha) const;
	/** Niagara 사용자 파라미터 이름. "User." 접두사가 없으면 붙인다. */
	FName GetNiagaraParameterName() const;
};

/** 투사체의 Niagara·GameplayCue 표현. 언제 무엇을 보일지는 투사체가 정하고, 여기서는 그리기만 한다. */
namespace PdSkillProjectilePresentation
{
	/** 루프 이펙트를 원하는 시스템으로 바꾸거나 끈다. 같은 시스템이 이미 돌고 있으면 처음부터 다시 틀지 않는다. */
	LABPROJECT_API void ApplyLoopEffect(UNiagaraComponent* Effect, UNiagaraSystem* DesiredSystem);
	LABPROJECT_API void ExecuteCue(AActor& Projectile, FGameplayTag CueTag, const FVector& Location);
	/** 충돌 이펙트를 생성한다. bOnGround면 바로 아래 지면에 지면 법선 방향으로 놓는다. */
	LABPROJECT_API void SpawnImpactEffect(AActor& Projectile, UNiagaraSystem* HitEffect, const FVector& Location, bool bOnGround);
	/** 투사체의 모든 Niagara 컴포넌트에 같은 Vector2D 사용자 파라미터를 넣는다. */
	LABPROJECT_API void SetNiagaraVector2D(const AActor& Projectile, FName ParameterName, FVector2D Value);
}
