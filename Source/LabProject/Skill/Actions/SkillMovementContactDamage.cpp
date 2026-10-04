#include "Skill/Actions/SkillMovementContactDamage.h"

#include "AbilitySystem/Ability/SkillAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Engine/World.h"

namespace
{
	constexpr float ContactDamageTickInterval = 1.0f / 30.0f;
	constexpr float ContactDamageCapsuleInflation = 15.0f;
}

void FSkillMovementContactDamage::Start(USkillAbility& SkillAbility, UObject& TimerOwner)
{
	Stop();

	const USkillDefinition* SkillDataAsset = SkillAbility.GetSourceSkillDataAsset();
	const AActor* AvatarActor = SkillAbility.GetAvatarActorFromActorInfo();
	const ACharacterBase* Character = SkillAbility.GetPdCharacterFromActorInfo();
	if (!SkillDataAsset || !SkillDataAsset->Movement.bDamageEnemiesOnContact || !AvatarActor || !AvatarActor->HasAuthority()
		|| !Character || !Character->GetCapsuleComponent())
	{
		return;
	}

	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	if (!DamageConfig.GameplayEffectClass || SkillAbility.CalculateDamageMagnitude(DamageConfig) <= 0.0f)
	{
		return;
	}
	Ability = &SkillAbility;
	bActive = true;
	OverlappingActors.Reset();
	PreviousLocation = Character->GetCapsuleComponent()->GetComponentLocation();

	Tick();
	// 첫 피해를 주는 중에 능력이 끝났다면 타이머를 걸지 않는다.
	if (!bActive)
	{
		return;
	}

	if (UWorld* World = SkillAbility.GetWorld())
	{
		TimerWorld = World;
		World->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(&TimerOwner, [this]()
		{
			Tick();
		}), ContactDamageTickInterval, true);
	}
}

void FSkillMovementContactDamage::Stop()
{
	if (UWorld* World = TimerWorld.Get())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}

	TimerHandle.Invalidate();
	TimerWorld.Reset();
	Ability.Reset();
	bActive = false;
	PreviousLocation = FVector::ZeroVector;
	OverlappingActors.Reset();
	CurrentActors.Reset();
	SweepHits.Reset();
	OverlapResults.Reset();
}

void FSkillMovementContactDamage::Tick()
{
	if (!bActive)
	{
		return;
	}

	const USkillAbility* SkillAbility = Ability.Get();
	ACharacterBase* Character = SkillAbility ? SkillAbility->GetPdCharacterFromActorInfo() : nullptr;
	UCapsuleComponent* CapsuleComponent = Character ? Character->GetCapsuleComponent() : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !CapsuleComponent || !World)
	{
		Stop();
		return;
	}

	const FVector CurrentLocation = CapsuleComponent->GetComponentLocation();
	const FVector SweepStart = PreviousLocation.IsNearlyZero() ? CurrentLocation : PreviousLocation;
	const FQuat CapsuleRotation = CapsuleComponent->GetComponentQuat();
	const FCollisionShape ContactShape = FCollisionShape::MakeCapsule(CapsuleComponent->GetScaledCapsuleRadius() + ContactDamageCapsuleInflation,
		CapsuleComponent->GetScaledCapsuleHalfHeight() + ContactDamageCapsuleInflation);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MovementContactDamage), false, Character);
	QueryParams.AddIgnoredActor(Character);

	CurrentActors.Reset();
	SweepHits.Reset();
	OverlapResults.Reset();

	TArray<TWeakObjectPtr<AActor>> ContactDamageTargets;
	const auto ProcessContactActor = [this, &ContactDamageTargets](AActor* ContactActor)
	{
		if (!IsValid(ContactActor))
		{
			return;
		}

		const FObjectKey ContactActorKey(ContactActor);
		const bool bAlreadySeenThisTick = CurrentActors.Contains(ContactActorKey);
		CurrentActors.Add(ContactActorKey);
		if (bAlreadySeenThisTick || OverlappingActors.Contains(ContactActorKey))
		{
			return;
		}

		ContactDamageTargets.Add(ContactActor);
	};

	if (!SweepStart.Equals(CurrentLocation, UE_KINDA_SMALL_NUMBER))
	{
		World->SweepMultiByObjectType(SweepHits, SweepStart, CurrentLocation, CapsuleRotation, ObjectQueryParams, ContactShape, QueryParams);
		for (const FHitResult& SweepHit : SweepHits)
		{
			ProcessContactActor(SweepHit.GetActor());
		}
	}

	World->OverlapMultiByObjectType(OverlapResults, CurrentLocation, CapsuleRotation, ObjectQueryParams, ContactShape, QueryParams);
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		ProcessContactActor(OverlapResult.GetActor());
	}

	OverlappingActors = MoveTemp(CurrentActors);
	PreviousLocation = CurrentLocation;

	for (const TWeakObjectPtr<AActor>& ContactActorPtr : ContactDamageTargets)
	{
		if (!bActive)
		{
			break;
		}

		if (AActor* ContactActor = ContactActorPtr.Get())
		{
			ApplyDamageTo(ContactActor);
		}
	}
}

void FSkillMovementContactDamage::ApplyDamageTo(AActor* HitActor)
{
	const USkillAbility* SkillAbility = Ability.Get();
	AActor* SourceActor = SkillAbility ? SkillAbility->GetAvatarActorFromActorInfo() : nullptr;
	if (!bActive || !SkillAbility || !IsValid(HitActor) || HitActor == SourceActor)
	{
		return;
	}

	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(SourceActor);
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(HitActor);
	if (!SourceCharacter || !TargetCharacter)
	{
		return;
	}

	UAbilitySystemComponent* SourceAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
	UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	if (!SourceAbilitySystemComponent || !TargetAbilitySystemComponent)
	{
		return;
	}

	if (TargetCharacter->IsDead() || !SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = SkillAbility->GetSourceSkillDataAsset();
	if (!SkillDataAsset)
	{
		return;
	}
	const FSkillGameplayEffectConfig DamageConfig = SkillDataAsset->GetResolvedDamageConfig();
	const float DamageMagnitude = SkillAbility->CalculateDamageMagnitude(DamageConfig);
	if (!DamageConfig.GameplayEffectClass || DamageMagnitude <= 0.0f)
	{
		return;
	}
	FGameplayEffectSpecHandle DamageSpecHandle = SkillAbility->MakeConfiguredDamageEffectSpec(DamageConfig, DamageMagnitude);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetAbilitySystemComponent);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		SkillAbility->ApplyConfiguredStatusEffectToTarget(SkillDataAsset, TargetAbilitySystemComponent);
	}
}
