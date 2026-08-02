#include "AbilitySystem/TargetingActors/TargetActor_GroundTrace_Decal.h"

#include "Abilities/GameplayAbilityWorldReticle.h"
#include "AbilitySystemComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Map/MapLayerTrigger.h"
#include "Map/OutOfBoundsRespawnVolume.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TargetActor_GroundTrace_Decal)

namespace
{
	constexpr float TargetingDecalSurfaceOffset = 2.0f;
	constexpr float TargetingGroundMinNormalZ = 0.35f;
	const FRotator TargetingDecalWorldRotation(-90.0f, 0.0f, 0.0f);

	bool IsIgnoredTargetingGroundActor(const AActor* Actor)
	{
		return Actor
			&& (Actor->IsA<AOutOfBoundsRespawnVolume>()
				|| Actor->IsA<AMapLayerTrigger>());
	}

	bool IsGroundTraceActorOwnedBy(const AActor* Actor, const AActor* OwnerCandidate)
	{
		if (!Actor || !OwnerCandidate)
		{
			return false;
		}

		for (const AActor* CurrentActor = Actor; CurrentActor; CurrentActor = CurrentActor->GetOwner())
		{
			if (CurrentActor == OwnerCandidate)
			{
				return true;
			}
		}

		return false;
	}

	bool IsSourceRelatedActor(const AActor* Actor, const AActor* SourceActor)
	{
		if (!Actor || !SourceActor)
		{
			return false;
		}

		const APawn* SourcePawn = Cast<APawn>(SourceActor);
		return Actor == SourceActor
			|| IsGroundTraceActorOwnedBy(Actor, SourceActor)
			|| (SourcePawn && Actor->GetInstigator() == SourcePawn);
	}

	bool IsPawnRelatedTargetingActor(const AActor* Actor)
	{
		const AActor* CurrentActor = Actor;
		for (int32 Depth = 0; Depth < 16 && IsValid(CurrentActor); ++Depth)
		{
			if (CurrentActor->IsA<APawn>() || IsValid(CurrentActor->GetInstigator()))
			{
				return true;
			}

			const AActor* OwnerActor = CurrentActor->GetOwner();
			const AActor* AttachParentActor = CurrentActor->GetAttachParentActor();
			const AActor* NextActor = OwnerActor ? OwnerActor : AttachParentActor;
			if (!IsValid(NextActor) || NextActor == CurrentActor)
			{
				break;
			}

			CurrentActor = NextActor;
		}

		return false;
	}

	bool IsValidTargetingGroundHit(const FHitResult& Hit, const AActor* SourceActor)
	{
		return Hit.bBlockingHit
			&& Hit.ImpactNormal.Z >= TargetingGroundMinNormalZ
			&& !IsPawnRelatedTargetingActor(Hit.GetActor())
			&& !IsIgnoredTargetingGroundActor(Hit.GetActor())
			&& !IsSourceRelatedActor(Hit.GetActor(), SourceActor);
	}

	void AddSourceAndAttachmentsToIgnore(
		FCollisionQueryParams& QueryParams,
		AActor* SourceActor)
	{
		if (!IsValid(SourceActor))
		{
			return;
		}

		QueryParams.AddIgnoredActor(SourceActor);

		TArray<AActor*> AttachedActors;
		SourceActor->GetAttachedActors(AttachedActors, true, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (IsValid(AttachedActor))
			{
				QueryParams.AddIgnoredActor(AttachedActor);
			}
		}
	}

	bool TryTraceTargetingWorldSurface(
		UWorld* World,
		const FVector& TraceStart,
		const FVector& TraceEnd,
		const AActor* SourceActor,
		const bool bRequireGroundSurface,
		FCollisionQueryParams QueryParams,
		FHitResult& OutHit)
	{
		if (!World)
		{
			return false;
		}

		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		FVector CurrentTraceStart = TraceStart;
		const FVector TraceDirection = (TraceEnd - TraceStart).GetSafeNormal();
		constexpr int32 MaxIgnoredHitCount = 32;
		for (int32 TraceIndex = 0; TraceIndex < MaxIgnoredHitCount; ++TraceIndex)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(
				Hit,
				CurrentTraceStart,
				TraceEnd,
				ObjectParams,
				QueryParams))
			{
				return false;
			}

			const AActor* HitActor = Hit.GetActor();
			const bool bValidHit = bRequireGroundSurface
				? IsValidTargetingGroundHit(Hit, SourceActor)
				: Hit.bBlockingHit
					&& !IsPawnRelatedTargetingActor(HitActor)
					&& !IsIgnoredTargetingGroundActor(HitActor)
					&& !IsSourceRelatedActor(HitActor, SourceActor);
			if (bValidHit)
			{
				OutHit = Hit;
				return true;
			}

			if (IsValid(HitActor))
			{
				QueryParams.AddIgnoredActor(HitActor);
			}
			else if (const UPrimitiveComponent* HitComponent = Hit.GetComponent())
			{
				QueryParams.AddIgnoredComponent(HitComponent);
			}

			if (TraceDirection.IsNearlyZero())
			{
				return false;
			}

			CurrentTraceStart = Hit.ImpactPoint + TraceDirection;
		}

		return false;
	}

	float ResolveTargetingDecalDepth(const double ConfiguredDepth)
	{
		return FMath::Clamp(static_cast<float>(ConfiguredDepth), 1.0f, 8.0f);
	}
}

ATargetActor_GroundTrace_Decal::ATargetActor_GroundTrace_Decal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;
}

void ATargetActor_GroundTrace_Decal::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyCachedGroundHitToDecal();
	UpdateDecalGrowth();
}

void ATargetActor_GroundTrace_Decal::ConfigureDecalGrowth(
	const double InStartSize,
	const double InTargetSize,
	const double InDuration)
{
	DecalGrowthStartSize = FMath::Max(InStartSize, 0.0);
	DecalGrowthTargetSize = FMath::Max(InTargetSize, 0.0);
	DecalGrowthDuration = FMath::Max(InDuration, 0.0);
	DecalGrowthStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bDecalGrowthActive = DecalGrowthDuration > UE_SMALL_NUMBER
		&& !FMath::IsNearlyEqual(DecalGrowthStartSize, DecalGrowthTargetSize);

	ApplyDecalSize(bDecalGrowthActive ? DecalGrowthStartSize : DecalGrowthTargetSize);
}

void ATargetActor_GroundTrace_Decal::ConfigureGroundProjection(
	const double InTraceStartHeight,
	const double InTraceDepth)
{
	GroundProjectionTraceStartHeight = FMath::Max(InTraceStartHeight, 0.0);
	GroundProjectionTraceDepth = FMath::Max(InTraceDepth, 100.0);
}

void ATargetActor_GroundTrace_Decal::BeginPlay()
{
	Super::BeginPlay();

	if (!Decal || !DefaultSceneRoot)
	{
		return;
	}

	DestroySpawnedDecal();

	const double InitialSize = bDecalGrowthActive ? DecalGrowthStartSize : DecalSize;
	SpawnedDecalComponent = UGameplayStatics::SpawnDecalAttached(
		Decal,
		FVector(ResolveTargetingDecalDepth(DecalDepth), InitialSize, InitialSize),
		DefaultSceneRoot,
		NAME_None,
		FVector::ZeroVector,
		TargetingDecalWorldRotation,
		EAttachLocation::KeepRelativeOffset,
		0.0f);

	if (SpawnedDecalComponent)
	{
		SpawnedDecalComponent->SetAbsolute(true, true, false);
		SpawnedDecalComponent->SetWorldRotation(TargetingDecalWorldRotation);
		SpawnedDecalComponent->SetFadeScreenSize(0.0f);
		if (bOverrideDecalColor)
		{
			SpawnedDecalComponent->SetDecalColor(DecalColor);
		}
		SpawnedDecalComponent->SetVisibility(false);
		ApplyDecalSize(InitialSize);
	}
}

void ATargetActor_GroundTrace_Decal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GenericDelegateBoundASC)
	{
		GenericDelegateBoundASC->GenericLocalConfirmCallbacks.RemoveDynamic(
			this,
			&AGameplayAbilityTargetActor::ConfirmTargeting);
		GenericDelegateBoundASC->GenericLocalCancelCallbacks.RemoveDynamic(
			this,
			&AGameplayAbilityTargetActor::CancelTargeting);
		GenericDelegateBoundASC = nullptr;
	}

	DestroySpawnedDecal();
	Super::EndPlay(EndPlayReason);
}

FHitResult ATargetActor_GroundTrace_Decal::PerformTrace(AActor* InSourceActor)
{
	UWorld* World = InSourceActor ? InSourceActor->GetWorld() : GetWorld();
	if (!World || !IsValid(InSourceActor))
	{
		MarkGroundTraceFailure();
		return FHitResult();
	}

	const FVector TargetingTraceStart = StartLocation.GetTargetingTransform().GetLocation();
	const float SafeMaxRange = FMath::Max(MaxRange, 0.0f);
	FVector TargetingTraceEnd =
		TargetingTraceStart + InSourceActor->GetActorForwardVector() * SafeMaxRange;
	if (PrimaryPC)
	{
		FVector ViewStart = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		PrimaryPC->GetPlayerViewPoint(ViewStart, ViewRotation);

		const FVector ViewDirection = ViewRotation.Vector().GetSafeNormal();
		FVector ViewEnd = ViewStart + ViewDirection * SafeMaxRange;
		ClipCameraRayToAbilityRange(
			ViewStart,
			ViewDirection,
			TargetingTraceStart,
			SafeMaxRange,
			ViewEnd);

		FVector AimDirection = (ViewEnd - TargetingTraceStart).GetSafeNormal();
		if (AimDirection.IsNearlyZero())
		{
			AimDirection = ViewDirection;
		}
		TargetingTraceEnd = TargetingTraceStart + AimDirection * SafeMaxRange;
	}

	FCollisionQueryParams TargetingQueryParams(
		SCENE_QUERY_STAT(TargetActorGroundDecalAimTrace),
		false);
	TargetingQueryParams.bReturnPhysicalMaterial = true;
	AddSourceAndAttachmentsToIgnore(TargetingQueryParams, InSourceActor);

	FHitResult AimSurfaceHit;
	const bool bHitAimSurface = TryTraceTargetingWorldSurface(
		World,
		TargetingTraceStart,
		TargetingTraceEnd,
		InSourceActor,
		false,
		TargetingQueryParams,
		AimSurfaceHit);
	const FVector AimLocation = bHitAimSurface
		? AimSurfaceHit.ImpactPoint
		: TargetingTraceEnd;

	const FVector GroundTraceStart = AimLocation
		+ FVector::UpVector * static_cast<float>(FMath::Max(GroundProjectionTraceStartHeight, 0.0));
	const FVector GroundTraceEnd = AimLocation
		- FVector::UpVector * static_cast<float>(FMath::Max(GroundProjectionTraceDepth, 100.0));

	FCollisionQueryParams GroundQueryParams(
		SCENE_QUERY_STAT(TargetActorGroundDecalFloorTrace),
		false);
	GroundQueryParams.bReturnPhysicalMaterial = true;
	AddSourceAndAttachmentsToIgnore(GroundQueryParams, InSourceActor);

	FHitResult GroundHit;
	if (TryTraceTargetingWorldSurface(
		World,
		GroundTraceStart,
		GroundTraceEnd,
		InSourceActor,
		true,
		GroundQueryParams,
		GroundHit))
	{
		bLastTraceWasGood = true;
		ApplyGroundHitToDecal(GroundHit);
		MarkGroundTraceSuccess();
		return GroundHit;
	}

	bLastTraceWasGood = false;
	if (AGameplayAbilityWorldReticle* LocalReticleActor = ReticleActor.Get())
	{
		LocalReticleActor->SetIsTargetValid(false);
	}
	SetTargetingDecalVisible(false);
	MarkGroundTraceFailure();

	FHitResult FailedTrace;
	FailedTrace.TraceStart = TargetingTraceStart;
	FailedTrace.TraceEnd = TargetingTraceEnd;
	FailedTrace.Location = AimLocation;
	return FailedTrace;
}

void ATargetActor_GroundTrace_Decal::DestroySpawnedDecal()
{
	if (!IsValid(SpawnedDecalComponent))
	{
		SpawnedDecalComponent = nullptr;
		return;
	}

	SpawnedDecalComponent->DestroyComponent();
	SpawnedDecalComponent = nullptr;
}

void ATargetActor_GroundTrace_Decal::ApplyDecalSize(const double InDecalSize) const
{
	if (!IsValid(SpawnedDecalComponent))
	{
		return;
	}

	const double ClampedSize = FMath::Max(InDecalSize, 0.0);
	SpawnedDecalComponent->DecalSize = FVector(ResolveTargetingDecalDepth(DecalDepth), ClampedSize, ClampedSize);
	SpawnedDecalComponent->MarkRenderStateDirty();
}

void ATargetActor_GroundTrace_Decal::ApplyGroundHitToDecal(const FHitResult& GroundHit)
{
	const FVector GroundNormal = GroundHit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : GroundHit.ImpactNormal;
	const FVector DecalLocation = GroundHit.ImpactPoint + GroundNormal * TargetingDecalSurfaceOffset;
	LastGroundDecalLocation = DecalLocation;
	bHasLastGroundDecalLocation = true;

	if (AGameplayAbilityWorldReticle* LocalReticleActor = ReticleActor.Get())
	{
		LocalReticleActor->SetIsTargetValid(true);
		LocalReticleActor->SetActorLocation(DecalLocation);
	}
}

void ATargetActor_GroundTrace_Decal::ApplyCachedGroundHitToDecal() const
{
	if (!bHasLastGroundDecalLocation)
	{
		return;
	}

	if (IsValid(SpawnedDecalComponent))
	{
		SpawnedDecalComponent->SetVisibility(true);
		SpawnedDecalComponent->SetHiddenInGame(false);
		SpawnedDecalComponent->SetWorldLocation(LastGroundDecalLocation);
		SpawnedDecalComponent->SetWorldRotation(TargetingDecalWorldRotation);
	}
}

void ATargetActor_GroundTrace_Decal::SetTargetingDecalVisible(const bool bVisible) const
{
	if (IsValid(SpawnedDecalComponent))
	{
		SpawnedDecalComponent->SetVisibility(bVisible);
		SpawnedDecalComponent->SetHiddenInGame(!bVisible);
	}
}

void ATargetActor_GroundTrace_Decal::UpdateDecalGrowth()
{
	if (!bDecalGrowthActive)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double ElapsedTime = World ? World->GetTimeSeconds() - DecalGrowthStartTime : DecalGrowthDuration;
	const double Alpha = DecalGrowthDuration > UE_SMALL_NUMBER
		? FMath::Clamp(ElapsedTime / DecalGrowthDuration, 0.0, 1.0)
		: 1.0;
	const double CurrentSize = FMath::Lerp(DecalGrowthStartSize, DecalGrowthTargetSize, Alpha);
	ApplyDecalSize(CurrentSize);

	if (Alpha >= 1.0)
	{
		bDecalGrowthActive = false;
		ApplyDecalSize(DecalGrowthTargetSize);
	}
}

void ATargetActor_GroundTrace_Decal::MarkGroundTraceFailure()
{
	bLastGroundTraceSucceeded = false;
	bHasLastGroundDecalLocation = false;
}

void ATargetActor_GroundTrace_Decal::MarkGroundTraceSuccess()
{
	if (bLastGroundTraceSucceeded)
	{
		return;
	}

	bLastGroundTraceSucceeded = true;
}
