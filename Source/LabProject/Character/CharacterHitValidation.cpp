#include "Character/CharacterHitValidation.h"

#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Common/PdGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace CharacterHitValidation
{
	ACharacterBase* ResolveRelatedCharacter(AActor* HitActor, const UPrimitiveComponent* HitComponent)
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

	ACharacterBase* ResolveDirectMeshHit(AActor* HitActor, const UPrimitiveComponent* HitComponent)
	{
		ACharacterBase* Character = ResolveRelatedCharacter(HitActor, HitComponent);
		return Character && HitComponent && HitComponent == Character->GetMesh() ? Character : nullptr;
	}

	// 근접 공격은 몸과 캡슐 모두로 맞는다. 다만 몸만 날아오른 몬스터(비행 공격)는 땅에 남은 캡슐이 아니라 몸에 맞아야 한다.
	ACharacterBase* ResolveMeleeWeaponDamageHit(AActor* HitActor, const UPrimitiveComponent* HitComponent)
	{
		ACharacterBase* Character = ResolveRelatedCharacter(HitActor, HitComponent);
		if (!Character || !HitComponent)
		{
			return nullptr;
		}
		if (HitComponent == Character->GetMesh())
		{
			return Character;
		}
		const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
		const bool bBodyInAir = ASC && ASC->HasMatchingGameplayTag(PdGameplayTags::State_Monster_Flying);
		return HitComponent == Character->GetCapsuleComponent() && !bBodyInAir ? Character : nullptr;
	}

	ACharacterBase* ResolveWeaponDamageHit(AActor* HitActor, const UPrimitiveComponent* HitComponent)
	{
		return ResolveDirectMeshHit(HitActor, HitComponent);
	}

	bool IsCharacterRelatedNonMeshHit(AActor* HitActor, const UPrimitiveComponent* HitComponent)
	{
		return ResolveRelatedCharacter(HitActor, HitComponent) != nullptr
			&& ResolveDirectMeshHit(HitActor, HitComponent) == nullptr;
	}

	bool IsCharacterRelatedNonWeaponDamageHit(AActor* HitActor, const UPrimitiveComponent* HitComponent)
	{
		return ResolveRelatedCharacter(HitActor, HitComponent) != nullptr
			&& ResolveWeaponDamageHit(HitActor, HitComponent) == nullptr;
	}
}
