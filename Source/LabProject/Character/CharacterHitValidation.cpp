#include "Character/CharacterHitValidation.h"

#include "Character/CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace PdCharacterHitValidation
{
	ACharacterBase* ResolveRelatedCharacter(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent)
	{
		if (const UPrimitiveComponent* Component = HitComponent)
		{
			if (ACharacterBase* ComponentOwnerCharacter = Cast<ACharacterBase>(Component->GetOwner()))
			{
				return ComponentOwnerCharacter;
			}
		}

		AActor* CurrentActor = HitActor;
		for (int32 Depth = 0; Depth < 8 && IsValid(CurrentActor); ++Depth)
		{
			if (ACharacterBase* Character = Cast<ACharacterBase>(CurrentActor))
			{
				return Character;
			}

			AActor* OwnerActor = CurrentActor->GetOwner();
			AActor* AttachParentActor = CurrentActor->GetAttachParentActor();
			AActor* NextActor = OwnerActor ? OwnerActor : AttachParentActor;
			if (!IsValid(NextActor) || NextActor == CurrentActor)
			{
				break;
			}

			CurrentActor = NextActor;
		}

		return nullptr;
	}

	ACharacterBase* ResolveDirectMeshHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent)
	{
		ACharacterBase* Character = ResolveRelatedCharacter(HitActor, HitComponent);
		return Character && HitComponent && HitComponent == Character->GetMesh()
			? Character
			: nullptr;
	}

	ACharacterBase* ResolveMeleeWeaponDamageHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent)
	{
		ACharacterBase* Character = ResolveRelatedCharacter(HitActor, HitComponent);
		return Character
			&& HitComponent
			&& (HitComponent == Character->GetMesh()
				|| HitComponent == Character->GetCapsuleComponent())
				? Character
				: nullptr;
	}

	ACharacterBase* ResolveWeaponDamageHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent)
	{
		return ResolveDirectMeshHit(HitActor, HitComponent);
	}

	bool IsCharacterRelatedNonMeshHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent)
	{
		return ResolveRelatedCharacter(HitActor, HitComponent) != nullptr
			&& ResolveDirectMeshHit(HitActor, HitComponent) == nullptr;
	}

	bool IsCharacterRelatedNonWeaponDamageHit(
		AActor* HitActor,
		const UPrimitiveComponent* HitComponent)
	{
		return ResolveRelatedCharacter(HitActor, HitComponent) != nullptr
			&& ResolveWeaponDamageHit(HitActor, HitComponent) == nullptr;
	}
}
