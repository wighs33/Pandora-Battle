#include "Skill/Actors/SkillProjectileHit.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Common/CollisionChannels.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ObjectKey.h"

namespace
{
	void AddWithAttachedActors(TArray<AActor*>& OutActors, AActor* Actor)
	{
		if (!Actor)
		{
			return;
		}

		OutActors.AddUnique(Actor);
		TArray<AActor*> AttachedActors;
		Actor->GetAttachedActors(AttachedActors, true, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			OutActors.AddUnique(AttachedActor);
		}
	}

	bool TryApplyDebuffToTarget(
		const AActor& Projectile,
		AActor* TargetActor,
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		const FSkillProjectileDamage& Damage)
	{
		if (!Projectile.HasAuthority()
			|| !Damage.DebuffSpec.IsValid()
			|| !Damage.DebuffSpec.Data.IsValid()
			|| !IsValid(TargetActor)
			|| !SourceASC
			|| !TargetASC
			|| !Damage.StatusEffect
			|| !Damage.StatusEffect->CanStack(TargetASC))
		{
			return false;
		}

		const FActiveGameplayEffectHandle AppliedHandle =
			SourceASC->ApplyGameplayEffectSpecToTarget(*Damage.DebuffSpec.Data.Get(), TargetASC);
		if (AppliedHandle.WasSuccessfullyApplied())
		{
			if (UStatusEffectReplicationComponent* ReplicationComponent =
				TargetActor->FindComponentByClass<UStatusEffectReplicationComponent>())
			{
				ReplicationComponent->TrackAppliedStatusEffect(Damage.StatusEffect, AppliedHandle);
			}
		}

		return AppliedHandle.WasSuccessfullyApplied();
	}
}

void PdSkillProjectileHit::ApplyCollisionProfile(UPrimitiveComponent* Collision)
{
	if (!Collision)
	{
		return;
	}

	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(LabCollisionChannels::Projectile());
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Block);
	Collision->SetCollisionResponseToChannel(LabCollisionChannels::OverlapBox(), ECR_Ignore);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetNotifyRigidBodyCollision(true);
}

void PdSkillProjectileHit::DisableCollision(UPrimitiveComponent* Collision)
{
	if (!Collision)
	{
		return;
	}

	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetNotifyRigidBodyCollision(false);
}

void PdSkillProjectileHit::IgnoreSourceActors(UPrimitiveComponent* Collision, AActor& Projectile)
{
	if (!Collision)
	{
		return;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(&Projectile);
	AddWithAttachedActors(ActorsToIgnore, Projectile.GetOwner());
	AddWithAttachedActors(ActorsToIgnore, Projectile.GetInstigator());

	for (AActor* IgnoredActor : ActorsToIgnore)
	{
		if (IsValid(IgnoredActor))
		{
			Collision->IgnoreActorWhenMoving(IgnoredActor, true);
		}
	}
}

bool PdSkillProjectileHit::IsIgnoredImpactActor(const AActor& Projectile, const AActor* OtherActor)
{
	if (!IsValid(OtherActor))
	{
		return true;
	}

	const AActor* OwningActor = Projectile.GetOwner();
	const APawn* InstigatorPawn = Projectile.GetInstigator();
	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(InstigatorPawn);

	// 무기·소환물처럼 시전자에게 소유되거나 붙은 액터도 시전자 쪽으로 본다.
	const AActor* CurrentActor = OtherActor;
	for (int32 Depth = 0; Depth < 8 && IsValid(CurrentActor); ++Depth)
	{
		if (CurrentActor == &Projectile || CurrentActor == OwningActor || CurrentActor == InstigatorPawn)
		{
			return true;
		}

		if (InstigatorPawn && CurrentActor->GetInstigator() == InstigatorPawn)
		{
			return true;
		}

		if (SourceCharacter)
		{
			if (const ACharacterBase* OtherCharacter = Cast<ACharacterBase>(CurrentActor))
			{
				if (!SourceCharacter->CanDamageCharacterByTeam(OtherCharacter))
				{
					return true;
				}
			}
		}

		const AActor* OwnerActor = CurrentActor->GetOwner();
		const AActor* AttachParentActor = CurrentActor->GetAttachParentActor();
		const AActor* NextActor = OwnerActor ? OwnerActor : AttachParentActor;
		if (NextActor == CurrentActor)
		{
			break;
		}

		CurrentActor = NextActor;
	}

	return false;
}

AActor* PdSkillProjectileHit::ResolveDamageTarget(AActor* OtherActor, const UPrimitiveComponent* OtherComponent)
{
	if (!IsValid(OtherActor))
	{
		return nullptr;
	}

	if (PdCharacterHitValidation::ResolveRelatedCharacter(OtherActor, OtherComponent))
	{
		return PdCharacterHitValidation::ResolveDirectMeshHit(OtherActor, OtherComponent);
	}

	return OtherActor;
}

FVector PdSkillProjectileHit::ResolveImpactLocation(
	const FHitResult& Hit,
	const bool bUseSurfacePoint,
	const FVector& FallbackLocation)
{
	const bool bHasReportedHit = Hit.GetActor() != nullptr || Hit.GetComponent() != nullptr;
	if (bUseSurfacePoint && bHasReportedHit && !Hit.ImpactPoint.ContainsNaN())
	{
		return FVector(Hit.ImpactPoint);
	}

	return bHasReportedHit && !Hit.Location.ContainsNaN() ? FVector(Hit.Location) : FallbackLocation;
}

FName PdSkillProjectileHit::ResolveImpactBoneName(
	const UPrimitiveComponent* ImpactComponent,
	const FHitResult& Hit,
	const FVector& ImpactLocation)
{
	if (!IsValid(ImpactComponent))
	{
		return NAME_None;
	}

	if (!Hit.BoneName.IsNone() && ImpactComponent->DoesSocketExist(Hit.BoneName))
	{
		return Hit.BoneName;
	}

	if (const USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(ImpactComponent))
	{
		const FName ClosestBoneName = SkeletalMesh->FindClosestBone(ImpactLocation);
		if (!ClosestBoneName.IsNone() && SkeletalMesh->DoesSocketExist(ClosestBoneName))
		{
			return ClosestBoneName;
		}
	}

	return NAME_None;
}

bool PdSkillProjectileHit::ApplyDamageToTarget(
	const AActor& Projectile,
	AActor* TargetActor,
	const FSkillProjectileDamage& Damage)
{
	if (!Projectile.HasAuthority() || !IsValid(TargetActor) || !Damage.DamageSpec.IsValid())
	{
		return false;
	}

	APawn* InstigatorPawn = Projectile.GetInstigator();
	if (const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(InstigatorPawn))
	{
		if (const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor))
		{
			if (!SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
			{
				return false;
			}
		}
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorPawn);
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!SourceASC || !TargetASC || !Damage.DamageSpec.Data.IsValid())
	{
		return false;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*Damage.DamageSpec.Data.Get(), TargetASC);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		TryApplyDebuffToTarget(Projectile, TargetActor, SourceASC, TargetASC, Damage);
	}

	return AppliedHandle.WasSuccessfullyApplied();
}

bool PdSkillProjectileHit::ApplyDamageInArea(
	const AActor& Projectile,
	const FVector& Center,
	const float Radius,
	const FSkillProjectileDamage& Damage)
{
	UWorld* World = Projectile.GetWorld();
	if (!Projectile.HasAuthority()
		|| !World
		|| Radius <= UE_SMALL_NUMBER
		|| !Damage.DamageSpec.IsValid()
		|| !Damage.DamageSpec.Data.IsValid())
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(LabCollisionChannels::HitableBody());

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectileImpactAreaDamage), false);
	if (AActor* OwningActor = Projectile.GetOwner())
	{
		QueryParams.AddIgnoredActor(OwningActor);
	}
	if (APawn* InstigatorPawn = Projectile.GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorPawn);
	}

	TArray<FOverlapResult> OverlapResults;
	if (!World->OverlapMultiByObjectType(
		OverlapResults,
		Center,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(Radius),
		QueryParams))
	{
		return false;
	}

	// 캡슐과 메시가 함께 겹쳐도 캐릭터마다 한 번만 맞힌다.
	TSet<FObjectKey> ProcessedTargets;
	bool bAppliedAnyDamage = false;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		ACharacterBase* TargetCharacter = Cast<ACharacterBase>(
			ResolveDamageTarget(OverlapResult.GetActor(), OverlapResult.GetComponent()));
		if (!IsValid(TargetCharacter))
		{
			continue;
		}

		const FObjectKey TargetKey(TargetCharacter);
		if (ProcessedTargets.Contains(TargetKey))
		{
			continue;
		}
		ProcessedTargets.Add(TargetKey);

		bAppliedAnyDamage |= ApplyDamageToTarget(Projectile, TargetCharacter, Damage);
	}

	return bAppliedAnyDamage;
}
