#include "Skill/Actions/SkillFieldTriggerDamage.h"

#include "Character/CharacterBase.h"
#include "Common/CollisionChannels.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillFieldTriggerDamage)

void USkillFieldTriggerDamage::Configure(
	const FName InTriggerComponentName,
	const bool bInRepeatWhileOverlapping,
	const double InRepeatInterval,
	FSkillFieldTriggerHit InOnHit)
{
	TriggerComponentName = InTriggerComponentName;
	bRepeatWhileOverlapping = bInRepeatWhileOverlapping;
	RepeatInterval = InRepeatInterval;
	OnHit = MoveTemp(InOnHit);
}

void USkillFieldTriggerDamage::Bind(AActor* FieldActor, const bool bDamageExistingOverlaps)
{
	UPrimitiveComponent* TriggerComponent = FindTriggerComponent(FieldActor);
	if (!TriggerComponent)
	{
		return;
	}

	// Actor field trigger volumes are gameplay-only overlap queries. Keeping them
	// as WorldDynamic lets weapon object traces hit the volume and then resolve its
	// owning character as the damage target.
	TriggerComponent->SetCollisionProfileName(TEXT("Custom"));
	TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerComponent->SetCollisionObjectType(LabCollisionChannels::OverlapBox());
	TriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerComponent->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Overlap);

	// Hide only the gameplay collision primitive. Propagating this state from a
	// root trigger also hides attached particle/Niagara components.
	TriggerComponent->SetHiddenInGame(true, false);
	TriggerComponent->SetGenerateOverlapEvents(true);
	TriggerComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleBeginOverlap);
	TriggerComponent->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleEndOverlap);
	TriggerComponents.AddUnique(TriggerComponent);
	TriggerComponent->UpdateOverlaps();

	TArray<AActor*> OverlappingActors;
	TriggerComponent->GetOverlappingActors(OverlappingActors, ACharacterBase::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		Track(FieldActor, OverlappingActor);
		if (bDamageExistingOverlaps)
		{
			Hit(FieldActor, OverlappingActor, false);
		}
	}

	StartRepeatTickIfNeeded();
}

void USkillFieldTriggerDamage::Reset()
{
	for (UPrimitiveComponent* TriggerComponent : TriggerComponents)
	{
		if (TriggerComponent)
		{
			TriggerComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleBeginOverlap);
			TriggerComponent->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleEndOverlap);
			TriggerComponent->SetGenerateOverlapEvents(false);
			TriggerComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RepeatTimerHandle);
	}
	RepeatTimerHandle.Invalidate();
	TriggerComponents.Reset();
	DamagedActorsBySource.Reset();
	DamageSourceActorsByKey.Reset();
	OverlappingActorsBySource.Reset();
}

bool USkillFieldTriggerDamage::IsBound(const AActor* FieldActor) const
{
	return FieldActor && TriggerComponents.ContainsByPredicate([FieldActor](const UPrimitiveComponent* TriggerComponent)
	{
		return TriggerComponent && TriggerComponent->GetOwner() == FieldActor;
	});
}

void USkillFieldTriggerDamage::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	AActor* DamageSourceActor = OverlappedComponent ? OverlappedComponent->GetOwner() : nullptr;
	Track(DamageSourceActor, OtherActor);
	Hit(DamageSourceActor, OtherActor, false);
}

void USkillFieldTriggerDamage::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const int32 OtherBodyIndex)
{
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);

	Untrack(OverlappedComponent ? OverlappedComponent->GetOwner() : nullptr, OtherActor);
}

// 겹쳐 있는 대상을 소스별로 다시 맞힌다. 사라진 소스와 대상은 이때 정리한다.
void USkillFieldTriggerDamage::HandleRepeatTick()
{
	if (!bRepeatWhileOverlapping)
	{
		return;
	}

	TArray<FObjectKey> DamageSourceKeys;
	OverlappingActorsBySource.GetKeys(DamageSourceKeys);

	for (const FObjectKey& SourceKey : DamageSourceKeys)
	{
		TWeakObjectPtr<AActor>* DamageSourcePtr = DamageSourceActorsByKey.Find(SourceKey);
		AActor* DamageSourceActor = DamageSourcePtr ? DamageSourcePtr->Get() : nullptr;
		if (!IsValid(DamageSourceActor))
		{
			DamageSourceActorsByKey.Remove(SourceKey);
			OverlappingActorsBySource.Remove(SourceKey);
			continue;
		}

		TArray<TWeakObjectPtr<AActor>>* OverlappingActors = OverlappingActorsBySource.Find(SourceKey);
		if (!OverlappingActors)
		{
			DamageSourceActorsByKey.Remove(SourceKey);
			continue;
		}

		TArray<TWeakObjectPtr<AActor>> DamageTargets;
		DamageTargets.Reserve(OverlappingActors->Num());
		for (int32 ActorIndex = OverlappingActors->Num() - 1; ActorIndex >= 0; --ActorIndex)
		{
			AActor* OverlappingActor = (*OverlappingActors)[ActorIndex].Get();
			if (!IsValid(OverlappingActor))
			{
				OverlappingActors->RemoveAtSwap(ActorIndex);
				continue;
			}

			DamageTargets.Add(OverlappingActor);
		}

		if (OverlappingActors->IsEmpty())
		{
			DamageSourceActorsByKey.Remove(SourceKey);
			OverlappingActorsBySource.Remove(SourceKey);
			continue;
		}

		// 피해 처리 중에 겹침이 끝난 대상은 건너뛴다.
		for (const TWeakObjectPtr<AActor>& TargetPtr : DamageTargets)
		{
			AActor* OverlappingActor = TargetPtr.Get();
			if (!IsValid(DamageSourceActor) || !IsValid(OverlappingActor))
			{
				continue;
			}

			const TArray<TWeakObjectPtr<AActor>>* CurrentOverlappingActors = OverlappingActorsBySource.Find(SourceKey);
			if (!CurrentOverlappingActors
				|| !CurrentOverlappingActors->ContainsByPredicate(
					[OverlappingActor](const TWeakObjectPtr<AActor>& ExistingActor)
					{
						return ExistingActor.Get() == OverlappingActor;
					}))
			{
				continue;
			}

			Hit(DamageSourceActor, OverlappingActor, true);
		}
	}
}

// 이름이 같은 컴포넌트, 같은 태그, 이름을 포함하는 컴포넌트 순으로 찾고, 없으면 겹침을 받는 첫 충돌체를 쓴다.
UPrimitiveComponent* USkillFieldTriggerDamage::FindTriggerComponent(AActor* FieldActor) const
{
	if (!FieldActor)
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	FieldActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	if (PrimitiveComponents.IsEmpty())
	{
		return nullptr;
	}

	if (!TriggerComponentName.IsNone())
	{
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->GetFName() == TriggerComponentName)
			{
				return PrimitiveComponent;
			}
		}

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->ComponentHasTag(TriggerComponentName))
			{
				return PrimitiveComponent;
			}
		}

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->GetName().Contains(TriggerComponentName.ToString()))
			{
				return PrimitiveComponent;
			}
		}
	}

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent
			&& PrimitiveComponent->GetGenerateOverlapEvents()
			&& PrimitiveComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			return PrimitiveComponent;
		}
	}

	return PrimitiveComponents[0];
}

void USkillFieldTriggerDamage::StartRepeatTickIfNeeded()
{
	UWorld* World = GetWorld();
	if (!bRepeatWhileOverlapping || RepeatTimerHandle.IsValid() || !World)
	{
		return;
	}

	// 소환 액터와 동일하게, 0 이하의 설정도 타이머 생성 시 최소 0.05초로 보정한다.
	const float DamageInterval = static_cast<float>(FMath::Max(RepeatInterval, 0.05));
	World->GetTimerManager().SetTimer(
		RepeatTimerHandle,
		this,
		&ThisClass::HandleRepeatTick,
		DamageInterval,
		true);
}

void USkillFieldTriggerDamage::Track(AActor* DamageSourceActor, AActor* OtherActor)
{
	if (!IsValid(DamageSourceActor) || !IsValid(OtherActor) || !OtherActor->IsA<ACharacterBase>())
	{
		return;
	}

	const FObjectKey SourceKey(DamageSourceActor);
	DamageSourceActorsByKey.FindOrAdd(SourceKey) = DamageSourceActor;

	TArray<TWeakObjectPtr<AActor>>& OverlappingActors = OverlappingActorsBySource.FindOrAdd(SourceKey);
	for (const TWeakObjectPtr<AActor>& ExistingActor : OverlappingActors)
	{
		if (ExistingActor.Get() == OtherActor)
		{
			return;
		}
	}

	OverlappingActors.Add(OtherActor);
}

void USkillFieldTriggerDamage::Untrack(AActor* DamageSourceActor, AActor* OtherActor)
{
	if (!DamageSourceActor || !OtherActor)
	{
		return;
	}

	const FObjectKey SourceKey(DamageSourceActor);
	TArray<TWeakObjectPtr<AActor>>* OverlappingActors = OverlappingActorsBySource.Find(SourceKey);
	if (!OverlappingActors)
	{
		return;
	}

	OverlappingActors->RemoveAllSwap(
		[OtherActor](const TWeakObjectPtr<AActor>& ExistingActor)
		{
			return !ExistingActor.IsValid() || ExistingActor.Get() == OtherActor;
		});

	if (OverlappingActors->IsEmpty())
	{
		OverlappingActorsBySource.Remove(SourceKey);
		DamageSourceActorsByKey.Remove(SourceKey);
	}
}

// 반복 피해가 아니면 같은 소스가 같은 대상을 한 번만 맞힌다.
void USkillFieldTriggerDamage::Hit(AActor* DamageSourceActor, AActor* HitActor, const bool bAllowRepeatedDamage)
{
	if (!IsValid(DamageSourceActor) || !IsValid(HitActor) || !OnHit.IsBound())
	{
		return;
	}

	const FObjectKey SourceKey(DamageSourceActor);
	const FObjectKey HitActorKey(HitActor);
	if (!bAllowRepeatedDamage && DamagedActorsBySource.FindOrAdd(SourceKey).Contains(HitActorKey))
	{
		return;
	}

	if (OnHit.Execute(DamageSourceActor, HitActor) && !bAllowRepeatedDamage)
	{
		DamagedActorsBySource.FindOrAdd(SourceKey).Add(HitActorKey);
	}
}
