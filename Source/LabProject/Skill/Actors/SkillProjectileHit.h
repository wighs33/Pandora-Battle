#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"

class AActor;
class UPrimitiveComponent;
class UStatusEffectDefinition;
struct FHitResult;

/** 투사체가 맞힌 대상에게 줄 피해와 상태 이상. */
struct FSkillProjectileDamage
{
	FGameplayEffectSpecHandle DamageSpec;
	FGameplayEffectSpecHandle DebuffSpec;
	const UStatusEffectDefinition* StatusEffect = nullptr;
};

/**
 * 투사체 충돌 규칙. 무엇에 맞았다고 볼지, 피해를 누구에게 줄지, 어디에 박힐지를 정한다.
 * 투사체의 비행·충돌 상태는 바꾸지 않고 판정과 피해 적용만 맡는다.
 */
namespace PdSkillProjectileHit
{
	/** 지형·물리 물체·피격 몸체에만 막히고 Pawn 캡슐은 통과하는 충돌 설정. */
	LABPROJECT_API void ApplyCollisionProfile(UPrimitiveComponent* Collision);
	LABPROJECT_API void DisableCollision(UPrimitiveComponent* Collision);
	/** 투사체 자신과 시전자, 시전자에 붙은 액터에는 이동 중에 막히지 않게 한다. */
	LABPROJECT_API void IgnoreSourceActors(UPrimitiveComponent* Collision, AActor& Projectile);

	/** 시전자와 그 소유물, 같은 팀 캐릭터는 맞은 것으로 치지 않는다. */
	LABPROJECT_API bool IsIgnoredImpactActor(const AActor& Projectile, const AActor* OtherActor);
	/** 캐릭터에 맞았으면 피해를 받을 캐릭터 본체를, 아니면 맞은 액터를 피해 대상으로 본다. */
	LABPROJECT_API AActor* ResolveDamageTarget(AActor* OtherActor, const UPrimitiveComponent* OtherComponent);
	/** 보고된 충돌 위치를 우선 쓴다. 박히는 투사체는 표면 지점(ImpactPoint)을 쓰고, 보고가 없으면 FallbackLocation. */
	LABPROJECT_API FVector ResolveImpactLocation(const FHitResult& Hit, bool bUseSurfacePoint, const FVector& FallbackLocation);
	/** 맞은 뼈가 없으면 충돌 지점에서 가장 가까운 뼈를 쓴다. */
	LABPROJECT_API FName ResolveImpactBoneName(
		const UPrimitiveComponent* ImpactComponent,
		const FHitResult& Hit,
		const FVector& ImpactLocation);

	/** 서버에서 피해를 적용하고, 들어갔으면 상태 이상도 건다. */
	LABPROJECT_API bool ApplyDamageToTarget(const AActor& Projectile, AActor* TargetActor, const FSkillProjectileDamage& Damage);
	/** 반경 안의 캐릭터마다 한 번씩 피해를 적용한다. */
	LABPROJECT_API bool ApplyDamageInArea(
		const AActor& Projectile,
		const FVector& Center,
		float Radius,
		const FSkillProjectileDamage& Damage);
}
