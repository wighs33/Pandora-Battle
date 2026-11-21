#include "Animation/AnimNotify_SpawnAreaStrikes.h"

#include "AI/Monster/MonsterAreaStrike.h"
#include "AI/Monster/MonsterCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_SpawnAreaStrikes)

namespace
{
	/** 둘레에 놓을 자리에서 바닥을 찾는 높이 범위. 경기장 계단과 언덕을 넘는 정도다. */
	constexpr float FloorSearchUp = 500.0f;
	constexpr float FloorSearchDown = 1500.0f;

	FVector GetFeetLocation(const AActor& Actor)
	{
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Actor.GetSimpleCollisionCylinder(Radius, HalfHeight);
		return Actor.GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
	}

	TOptional<FVector> FindFloor(const UWorld& World, const FVector& Location, const AActor& IgnoredActor)
	{
		FHitResult Hit;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(AreaStrikeFloor), false, &IgnoredActor);
		if (World.LineTraceSingleByObjectType(Hit, Location + FVector(0.0f, 0.0f, FloorSearchUp), Location - FVector(0.0f, 0.0f, FloorSearchDown),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return Hit.ImpactPoint;
		}
		return {};
	}
}

void UAnimNotify_SpawnAreaStrikes::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AMonsterCharacter* Monster = MeshComp ? Cast<AMonsterCharacter>(MeshComp->GetOwner()) : nullptr;
	UWorld* World = Monster ? Monster->GetWorld() : nullptr;
	if (!World || !Monster->HasAuthority() || !StrikeClass)
	{
		return;
	}

	TArray<FVector, TInlineAllocator<16>> StrikeLocations;
	const FVector MonsterFeet = GetFeetLocation(*Monster);
	for (TActorIterator<ACharacterBase> It(World); It; ++It)
	{
		if (Monster->IsValidMonsterDamageTarget(*It) && FVector::Dist2D(It->GetActorLocation(), MonsterFeet) <= PlayerSearchRadius)
		{
			StrikeLocations.Add(GetFeetLocation(**It));
		}
	}
	for (int32 Index = 0; Index < ExtraStrikeCount; ++Index)
	{
		const FVector Offset = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector()
			* FMath::FRandRange(ExtraStrikeMinDistance, FMath::Max(ExtraStrikeMinDistance, ExtraStrikeRadius));
		if (const TOptional<FVector> Floor = FindFloor(*World, MonsterFeet + Offset, *Monster))
		{
			StrikeLocations.Add(Floor.GetValue());
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Monster;
	SpawnParameters.Instigator = Monster;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FVector& Location : StrikeLocations)
	{
		World->SpawnActor<AMonsterAreaStrike>(StrikeClass, Location, FRotator::ZeroRotator, SpawnParameters);
	}
}

FString UAnimNotify_SpawnAreaStrikes::GetNotifyName_Implementation() const
{
	return TEXT("Spawn Area Strikes");
}
