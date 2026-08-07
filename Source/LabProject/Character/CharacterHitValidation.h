#pragma once

#include "CoreMinimal.h"

class AActor;
class ACharacterBase;
class UPrimitiveComponent;

namespace PdCharacterHitValidation
{
	/** Resolves a character directly or indirectly related to the hit actor/component. */
	LABPROJECT_API ACharacterBase* ResolveRelatedCharacter(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/** Returns a character only when the hit component is that character's primary skeletal mesh. */
	LABPROJECT_API ACharacterBase* ResolveDirectMeshHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/** Returns a character when a melee weapon touches its primary mesh or movement capsule. */
	LABPROJECT_API ACharacterBase* ResolveMeleeWeaponDamageHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/**
	 * Resolves a valid ranged weapon or projectile damage hit.
	 * A character is accepted only when the hit component is its primary skeletal mesh.
	 */
	LABPROJECT_API ACharacterBase* ResolveWeaponDamageHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/** True when the hit belongs to a character but did not touch its primary skeletal mesh. */
	LABPROJECT_API bool IsCharacterRelatedNonMeshHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);

	/** True when the hit belongs to a character but is not a component accepted for weapon damage. */
	LABPROJECT_API bool IsCharacterRelatedNonWeaponDamageHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent);
}
