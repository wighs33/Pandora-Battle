#include "Skill/Actions/SkillTriggerDamage.h"

#include "Character/CharacterBase.h"
#include "Components/PrimitiveComponent.h"
#include "Definition/AbilitySystem/SkillEffectSettings.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillTriggerDamage)

void USkillTriggerDamage::Configure(const FSkillTopLevelDamageConfig& DamageConfig, FSkillTriggerHit InOnHit)
{
	bRepeatWhileOverlapping = DamageConfig.bRepeatTriggerDamageWhileOverlapping;
	RepeatInterval = DamageConfig.TriggerDamageInterval;
	OnHit = MoveTemp(InOnHit);
}

UPrimitiveComponent* USkillTriggerDamage::FindTriggerComponent(AActor* Actor, const FName ComponentName,
	const bool bUseAnyPrimitiveAsFallback)
{
	if (!Actor)
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	if (PrimitiveComponents.IsEmpty())
	{
		return nullptr;
	}

	if (!ComponentName.IsNone())
	{
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->GetFName() == ComponentName)
			{
				return PrimitiveComponent;
			}
		}

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->ComponentHasTag(ComponentName))
			{
				return PrimitiveComponent;
			}
		}

		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->GetName().Contains(ComponentName.ToString()))
			{
				return PrimitiveComponent;
			}
		}
	}

	if (!bUseAnyPrimitiveAsFallback)
	{
		return nullptr;
	}

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->GetGenerateOverlapEvents()
			&& PrimitiveComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			return PrimitiveComponent;
		}
	}

	return PrimitiveComponents[0];
}

void USkillTriggerDamage::Bind(UPrimitiveComponent* TriggerComponent, const bool bHitExistingOverlaps)
{
	if (!TriggerComponent)
	{
		return;
	}

	TriggerComponent->SetGenerateOverlapEvents(true);
	TriggerComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleBeginOverlap);
	TriggerComponent->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleEndOverlap);
	TriggerComponents.AddUnique(TriggerComponent);
	TrackExistingOverlaps(TriggerComponent, bHitExistingOverlaps);
	StartRepeatTickIfNeeded();
}

void USkillTriggerDamage::SetDamageActive(const bool bActive)
{
	bDamageActive = bActive;
	if (!bActive)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RepeatTimerHandle);
		}
		RepeatTimerHandle.Invalidate();
		return;
	}

	for (UPrimitiveComponent* TriggerComponent : TriggerComponents)
	{
		TrackExistingOverlaps(TriggerComponent, true);
	}
	StartRepeatTickIfNeeded();
}

void USkillTriggerDamage::Reset()
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
	bDamageActive = true;
	TriggerComponents.Reset();
	DamagedActorsBySource.Reset();
	DamageSourceActorsByKey.Reset();
	OverlappingActorsBySource.Reset();
}

bool USkillTriggerDamage::IsBound(const AActor* TriggerOwner) const
{
	return TriggerOwner && TriggerComponents.ContainsByPredicate([TriggerOwner](const UPrimitiveComponent* TriggerComponent)
	{
		return TriggerComponent && TriggerComponent->GetOwner() == TriggerOwner;
	});
}

void USkillTriggerDamage::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, const int32 OtherBodyIndex, const bool bFromSweep, const FHitResult& SweepResult)
{
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	AActor* DamageSourceActor = OverlappedComponent ? OverlappedComponent->GetOwner() : nullptr;
	Track(DamageSourceActor, OtherActor);
	Hit(DamageSourceActor, OtherActor, false);
}

void USkillTriggerDamage::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, const int32 OtherBodyIndex)
{
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);

	Untrack(OverlappedComponent ? OverlappedComponent->GetOwner() : nullptr, OtherActor);
}

// 겹쳐 있는 대상을 소스별로 다시 맞힌다. 사라진 소스와 대상은 이때 정리한다.
void USkillTriggerDamage::HandleRepeatTick()
{
	if (!bRepeatWhileOverlapping || !bDamageActive)
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
			if (!CurrentOverlappingActors || !CurrentOverlappingActors->ContainsByPredicate(
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

void USkillTriggerDamage::TrackExistingOverlaps(UPrimitiveComponent* TriggerComponent, const bool bHitExistingOverlaps)
{
	AActor* DamageSourceActor = TriggerComponent ? TriggerComponent->GetOwner() : nullptr;
	if (!DamageSourceActor)
	{
		return;
	}

	TriggerComponent->UpdateOverlaps();

	TArray<AActor*> OverlappingActors;
	TriggerComponent->GetOverlappingActors(OverlappingActors, ACharacterBase::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		Track(DamageSourceActor, OverlappingActor);
		if (bHitExistingOverlaps)
		{
			Hit(DamageSourceActor, OverlappingActor, false);
		}
	}
}

void USkillTriggerDamage::StartRepeatTickIfNeeded()
{
	UWorld* World = GetWorld();
	if (!bRepeatWhileOverlapping || !bDamageActive || RepeatTimerHandle.IsValid() || !World)
	{
		return;
	}

	// 0 이하의 설정도 타이머 생성 시 최소 0.05초로 보정한다.
	const float DamageInterval = static_cast<float>(FMath::Max(RepeatInterval, 0.05));
	World->GetTimerManager().SetTimer(RepeatTimerHandle, this, &ThisClass::HandleRepeatTick, DamageInterval, true);
}

void USkillTriggerDamage::Track(AActor* DamageSourceActor, AActor* OtherActor)
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

void USkillTriggerDamage::Untrack(AActor* DamageSourceActor, AActor* OtherActor)
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

	OverlappingActors->RemoveAllSwap([OtherActor](const TWeakObjectPtr<AActor>& ExistingActor)
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
void USkillTriggerDamage::Hit(AActor* DamageSourceActor, AActor* HitActor, const bool bAllowRepeatedDamage)
{
	if (!bDamageActive || !IsValid(DamageSourceActor) || !IsValid(HitActor) || !OnHit.IsBound())
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
