#pragma once

#include "CoreMinimal.h"

class AActor;
class ACharacterBase;
class UPrimitiveComponent;

namespace PdCharacterHitValidation
{
	/** 맞은 액터·컴포넌트와 직접 또는 간접으로 연결된 캐릭터를 찾는다. */
	LABPROJECT_API ACharacterBase* ResolveRelatedCharacter(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/** 맞은 컴포넌트가 그 캐릭터의 주 스켈레탈 메시일 때만 캐릭터를 돌려준다. */
	LABPROJECT_API ACharacterBase* ResolveDirectMeshHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/** 근접 무기가 주 메시나 이동 캡슐에 닿았을 때 캐릭터를 돌려준다. */
	LABPROJECT_API ACharacterBase* ResolveMeleeWeaponDamageHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/**
	 * 원거리 무기나 투사체 피해로 인정할 맞음을 찾는다.
	 * 캐릭터는 맞은 컴포넌트가 주 스켈레탈 메시일 때만 인정한다.
	 */
	LABPROJECT_API ACharacterBase* ResolveWeaponDamageHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/** 캐릭터에 속한 맞음이지만 주 스켈레탈 메시에 닿지 않았으면 true. */
	LABPROJECT_API bool IsCharacterRelatedNonMeshHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/** 캐릭터에 속한 맞음이지만 무기 피해로 인정하는 컴포넌트가 아니면 true. */
	LABPROJECT_API bool IsCharacterRelatedNonWeaponDamageHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);
}
