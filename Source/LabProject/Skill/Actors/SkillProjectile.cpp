#include "Skill/Actors/SkillProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Common/CollisionChannels.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/OverlapResult.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayCueFunctionLibrary.h"
#include "GameplayEffect.h"
#include "Map/TransientActorRegistrySubsystem.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ObjectKey.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillProjectile)

namespace
{
	constexpr float HitNiagaraGroundTraceStartHeight = 150.0f;
	constexpr float HitNiagaraGroundTraceDepth = 5000.0f;
}

ASkillProjectile::ASkillProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bNetUseOwnerRelevancy = true;
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);

	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SetRootComponent(SphereCollision);
	SphereCollision->InitSphereRadius(12.0f);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SphereCollision->SetCollisionObjectType(LabCollisionChannels::Projectile());
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(LabCollisionChannels::OverlapBox(), ECR_Ignore);
	SphereCollision->SetGenerateOverlapEvents(true);
	SphereCollision->SetNotifyRigidBodyCollision(true);
	SphereCollision->SetCanEverAffectNavigation(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(SphereCollision);
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 0.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->Deactivate();

	ProjectileEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ProjectileEffect"));
	ProjectileEffect->SetupAttachment(SphereCollision);
	ProjectileEffect->SetAutoActivate(false);
}

void ASkillProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, TargetLocation, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, Speed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, bUseArcTrajectory, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ArcHeight, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ArcGravityScale, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, MuzzleFX, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ProjectileFX, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, HitFX, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, bSpawnHitNiagaraOnGround, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, SpawnGameplayCueTag, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ImpactGameplayCueTag, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ReadiedScaleGrowthStartScale, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ReadiedScaleGrowthTargetScale, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ReadiedScaleGrowthDuration, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ReadiedScaleGrowthServerStartTime, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ReadiedScaleGrowthNiagaraVector2DParameterName, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ReadiedScaleGrowthNiagaraStartSize, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ReadiedScaleGrowthNiagaraTargetSize, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, bReadiedScaleGrowthActive, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, bHasImpacted, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, bKeepProjectileVisualAfterImpact, Params);
}

void ASkillProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateReadiedScaleGrowth();
}

void ASkillProjectile::InitializeProjectile(
	const FVector& InTargetLocation,
	float InSpeed,
	const FGameplayEffectSpecHandle& InDamageEffectSpecHandle)
{
	bCosmeticOnly = false;
	LaunchProjectile(InTargetLocation, InSpeed, InDamageEffectSpecHandle);
}

void ASkillProjectile::PrepareProjectile(const FGameplayEffectSpecHandle& InDamageEffectSpecHandle)
{
	bCosmeticOnly = false;
	TargetLocation = GetActorLocation();
	Speed = 0.0f;
	MarkProjectileFlightDataDirty();

	DamageEffectSpecHandle = InDamageEffectSpecHandle;
	bHasImpacted = false;
	bKeepProjectileVisualAfterImpact = false;
	bImpactCueExecuted = false;
	bImpactNiagaraExecuted = false;
	ConfigureIgnoredActors();
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
	DisableProjectileCollision();
	ApplyProjectileLoopVisual();

}

void ASkillProjectile::PrepareCosmeticReadiedProjectile(const float InLifeSpan)
{
	bCosmeticOnly = true;
	TargetLocation = GetActorLocation();
	Speed = 0.0f;
	MarkProjectileFlightDataDirty();

	DamageEffectSpecHandle = FGameplayEffectSpecHandle();
	DebuffEffectSpecHandle = FGameplayEffectSpecHandle();
	StatusEffectDefinition = nullptr;
	bHasImpacted = false;
	bKeepProjectileVisualAfterImpact = false;
	bImpactCueExecuted = false;
	bImpactNiagaraExecuted = false;
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
	DisableProjectileCollision();
	ApplyProjectileLoopVisual();

	if (InLifeSpan > 0.0f)
	{
		SetLifeSpan(InLifeSpan);
	}
}

void ASkillProjectile::StartReadiedScaleGrowth(
	FVector InStartScale,
	FVector InTargetScale,
	const float InDuration,
	const FName InNiagaraVector2DParameterName,
	FVector2D InNiagaraStartSize,
	FVector2D InNiagaraTargetSize)
{
	const float ClampedDuration = FMath::Max(InDuration, 0.0f);
	InStartScale = InStartScale.ComponentMax(FVector::ZeroVector);
	InTargetScale = InTargetScale.ComponentMax(FVector::ZeroVector);
	InNiagaraStartSize.X = FMath::Max(InNiagaraStartSize.X, 0.0f);
	InNiagaraStartSize.Y = FMath::Max(InNiagaraStartSize.Y, 0.0f);
	InNiagaraTargetSize.X = FMath::Max(InNiagaraTargetSize.X, 0.0f);
	InNiagaraTargetSize.Y = FMath::Max(InNiagaraTargetSize.Y, 0.0f);

	const bool bHasNiagaraSizeGrowth = !InNiagaraVector2DParameterName.IsNone() && !InNiagaraStartSize.Equals(InNiagaraTargetSize);
	if (ClampedDuration <= 0.0f || (InStartScale.Equals(InTargetScale) && !bHasNiagaraSizeGrowth))
	{
		StopReadiedScaleGrowth();
		ReadiedScaleGrowthStartScale = InStartScale;
		ReadiedScaleGrowthTargetScale = InTargetScale;
		ReadiedScaleGrowthNiagaraVector2DParameterName = InNiagaraVector2DParameterName;
		ReadiedScaleGrowthNiagaraStartSize = InNiagaraStartSize;
		ReadiedScaleGrowthNiagaraTargetSize = InNiagaraTargetSize;
		ApplyReadiedGrowthValue(1.0f);
		MarkReadiedScaleGrowthDirty();
		if (HasAuthority() && GetIsReplicated())
		{
			ForceNetUpdate();
		}
		return;
	}

	ReadiedScaleGrowthStartScale = InStartScale;
	ReadiedScaleGrowthTargetScale = InTargetScale;
	ReadiedScaleGrowthDuration = ClampedDuration;
	ReadiedScaleGrowthServerStartTime = GetSyncedWorldTimeSeconds();
	ReadiedScaleGrowthNiagaraVector2DParameterName = InNiagaraVector2DParameterName;
	ReadiedScaleGrowthNiagaraStartSize = InNiagaraStartSize;
	ReadiedScaleGrowthNiagaraTargetSize = InNiagaraTargetSize;
	bReadiedScaleGrowthActive = true;
	MarkReadiedScaleGrowthDirty();

	ApplyReadiedGrowthValue(0.0f);
	SetActorTickEnabled(true);
	if (HasAuthority() && GetIsReplicated())
	{
		ForceNetUpdate();
	}

}

float ASkillProjectile::GetReadiedScaleGrowthAlpha() const
{
	if (ReadiedScaleGrowthDuration <= 0.0f)
	{
		return 1.0f;
	}

	const float ElapsedTime = FMath::Max(GetSyncedWorldTimeSeconds() - ReadiedScaleGrowthServerStartTime, 0.0f);
	return FMath::Clamp(ElapsedTime / ReadiedScaleGrowthDuration, 0.0f, 1.0f);
}

void ASkillProjectile::LaunchProjectile(
	const FVector& InTargetLocation,
	float InSpeed,
	const FGameplayEffectSpecHandle& InDamageEffectSpecHandle)
{
	bCosmeticOnly = false;
	StopReadiedScaleGrowth();
	TargetLocation = InTargetLocation;
	Speed = FMath::Max(InSpeed, 0.0f);
	MarkProjectileFlightDataDirty();
	DamageEffectSpecHandle = InDamageEffectSpecHandle;
	bHasImpacted = false;
	bKeepProjectileVisualAfterImpact = false;
	bImpactCueExecuted = false;
	bImpactNiagaraExecuted = false;
	ConfigureCollision();
	ConfigureIgnoredActors();
	ApplyProjectileLoopVisual();

	if (HasActorBegunPlay())
	{
		StartProjectileMovement();
	}
	if (HasAuthority() && GetIsReplicated() && HasActorBegunPlay())
	{
		ForceNetUpdate();
	}
}

void ASkillProjectile::ConfigureArcTrajectory(
	const bool bInUseArcTrajectory,
	const float InArcHeight,
	const float InArcGravityScale)
{
	bUseArcTrajectory = bInUseArcTrajectory;
	ArcHeight = FMath::Max(InArcHeight, 0.0f);
	ArcGravityScale = FMath::Max(InArcGravityScale, 0.0f);
	MarkProjectileFlightDataDirty();

}

void ASkillProjectile::SetDebuffEffectSpecHandle(
	const FGameplayEffectSpecHandle& InDebuffEffectSpecHandle,
	UStatusEffectDefinition* InStatusEffectDefinition)
{
	DebuffEffectSpecHandle = InDebuffEffectSpecHandle;
	StatusEffectDefinition = InStatusEffectDefinition;

}

void ASkillProjectile::SetImpactAreaDamageRadius(const float InImpactAreaDamageRadius)
{
	ImpactAreaDamageRadius = FMath::Max(InImpactAreaDamageRadius, 0.0f);
}

void ASkillProjectile::ConfigureImpactPersistence(
	const bool bInStickOnImpact,
	const float InPostImpactLifeSpan)
{
	PostImpactLifeSpan = FMath::Max(InPostImpactLifeSpan, 0.0f);
	bStickOnImpact = bInStickOnImpact && PostImpactLifeSpan > UE_SMALL_NUMBER;
}

void ASkillProjectile::ConfigureProjectileVisuals(
	UNiagaraSystem* InMuzzleFX,
	UNiagaraSystem* InProjectileFX,
	UNiagaraSystem* InHitFX,
	const bool bInSpawnHitNiagaraOnGround,
	const FGameplayTag InSpawnGameplayCueTag,
	const FGameplayTag InImpactGameplayCueTag)
{
	MuzzleFX = InMuzzleFX;
	ProjectileFX = InProjectileFX;
	HitFX = InHitFX;
	bSpawnHitNiagaraOnGround = bInSpawnHitNiagaraOnGround;
	SpawnGameplayCueTag = InSpawnGameplayCueTag;
	ImpactGameplayCueTag = InImpactGameplayCueTag;

	MarkProjectileVisualsDirty();
	if (HasAuthority() && GetIsReplicated())
	{
		ForceNetUpdate();
	}

}

void ASkillProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UTransientActorRegistrySubsystem* Registry =
			World->GetSubsystem<UTransientActorRegistrySubsystem>())
		{
			Registry->RegisterTransientActor(this, GetOwner());
		}
	}

	if (SphereCollision)
	{
		if (bCosmeticOnly)
		{
			DisableProjectileCollision();
		}
		else if (Speed > 0.0f)
		{
			ConfigureCollision();
		}
		else
		{
			DisableProjectileCollision();
		}
		ConfigureIgnoredActors();
		SphereCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleSphereBeginOverlap);
		SphereCollision->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleSphereHit);
	}

	if (bHasImpacted)
	{
		OnRep_ImpactState();
	}
	else
	{
		if (Speed > 0.0f)
		{
			StartProjectileMovement();
		}
		ApplyProjectileLoopVisual();
		ExecuteSpawnGameplayCue();
	}
}

void ASkillProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UTransientActorRegistrySubsystem* Registry =
			World->GetSubsystem<UTransientActorRegistrySubsystem>())
		{
			Registry->UnregisterTransientActor(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ASkillProjectile::Destroyed()
{
	if (bHasImpacted && !bImpactCueExecuted)
	{
		ExecuteImpactGameplayCue();
	}
	if (bHasImpacted && !bImpactNiagaraExecuted)
	{
		ExecuteImpactNiagaraAtLocation(GetActorLocation());
	}

Super::Destroyed();
}

void ASkillProjectile::OnRep_ProjectileFlightData()
{
	if (bHasImpacted)
	{
		OnRep_ImpactState();
		return;
	}

	if (bCosmeticOnly)
	{
		DisableProjectileCollision();
		if (Speed > 0.0f)
		{
			StartProjectileMovement();
		}
		ApplyProjectileLoopVisual();
		return;
	}

	if (Speed > 0.0f)
	{
		ConfigureCollision();
		StartProjectileMovement();
	}
	else
	{
		DisableProjectileCollision();
	}
	ApplyProjectileLoopVisual();
}

void ASkillProjectile::OnRep_ProjectileVisuals()
{
	ApplyProjectileLoopVisual();
}

void ASkillProjectile::OnRep_ReadiedScaleGrowth()
{
	if (bReadiedScaleGrowthActive)
	{
		SetActorTickEnabled(true);
		UpdateReadiedScaleGrowth();
		return;
	}

	SetActorTickEnabled(false);
}

void ASkillProjectile::OnRep_ImpactState()
{
	if (!bHasImpacted)
	{
		return;
	}

	StopAtImpact(GetActorLocation());
	ApplyProjectileLoopVisual();
	ExecuteImpactGameplayCueAtLocation(GetActorLocation());
	ExecuteImpactNiagaraAtLocation(GetActorLocation());
}

void ASkillProjectile::HandleSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherBodyIndex);

	const FHitResult EmptyHit;
	HandleImpact(OtherActor, OtherComp, bFromSweep ? SweepResult : EmptyHit);
}

void ASkillProjectile::HandleSphereHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	static_cast<void>(HitComponent);
	static_cast<void>(NormalImpulse);

	HandleImpact(OtherActor, OtherComp, Hit);
}

void ASkillProjectile::StartProjectileMovement() const
{
	if (!ProjectileMovement || Speed <= 0.0f)
	{

		return;
	}

	FVector Direction = (FVector(TargetLocation) - GetActorLocation()).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{

		Direction = GetActorForwardVector();
	}

	ProjectileMovement->SetUpdatedComponent(SphereCollision);
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = Direction * Speed;
	if (bUseArcTrajectory)
	{
		const FVector ArcVelocity = CalculateArcLaunchVelocity();
		if (!ArcVelocity.IsNearlyZero())
		{
			ProjectileMovement->ProjectileGravityScale = ArcGravityScale;
			ProjectileMovement->InitialSpeed = ArcVelocity.Size();
			ProjectileMovement->MaxSpeed = FMath::Max(Speed, ArcVelocity.Size()) * 2.0f;
			ProjectileMovement->Velocity = ArcVelocity;
			Direction = ArcVelocity.GetSafeNormal();
		}
	}
	ProjectileMovement->Activate(true);
	ProjectileMovement->UpdateComponentVelocity();

}

FVector ASkillProjectile::CalculateArcLaunchVelocity() const
{
	const UWorld* World = GetWorld();
	const float WorldGravityZ = World ? World->GetGravityZ() : -980.0f;
	const float GravityScale = FMath::Max(ArcGravityScale, UE_SMALL_NUMBER);
	const float GravityZ = WorldGravityZ * GravityScale;
	const float GravityMagnitude = FMath::Abs(GravityZ);
	if (GravityMagnitude <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const FVector StartLocation = GetActorLocation();
	const FVector EndLocation = FVector(TargetLocation);
	const FVector Delta = EndLocation - StartLocation;
	const FVector HorizontalDelta(Delta.X, Delta.Y, 0.0);
	const float HorizontalDistance = HorizontalDelta.Size();

	float TravelTime = 0.0f;
	if (ArcHeight > UE_SMALL_NUMBER)
	{
		TravelTime = FMath::Sqrt((8.0f * ArcHeight) / GravityMagnitude);
	}
	else if (Speed > UE_SMALL_NUMBER && HorizontalDistance > UE_SMALL_NUMBER)
	{
		TravelTime = HorizontalDistance / Speed;
	}

	if (TravelTime <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const FVector HorizontalVelocity = HorizontalDistance > UE_SMALL_NUMBER
		? HorizontalDelta / TravelTime
		: FVector::ZeroVector;
	const float VerticalVelocity = (Delta.Z - (0.5f * GravityZ * FMath::Square(TravelTime))) / TravelTime;
	const FVector LaunchVelocity = HorizontalVelocity + FVector::UpVector * VerticalVelocity;

	return LaunchVelocity;
}

void ASkillProjectile::ConfigureCollision() const
{
	if (!SphereCollision)
	{
		return;
	}

	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SphereCollision->SetCollisionObjectType(LabCollisionChannels::Projectile());
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(LabCollisionChannels::OverlapBox(), ECR_Ignore);
	SphereCollision->SetGenerateOverlapEvents(true);
	SphereCollision->SetNotifyRigidBodyCollision(true);

}

void ASkillProjectile::DisableProjectileCollision() const
{
	if (!SphereCollision)
	{
		return;
	}

	SphereCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereCollision->SetGenerateOverlapEvents(false);
	SphereCollision->SetNotifyRigidBodyCollision(false);

}

void ASkillProjectile::ConfigureIgnoredActors() const
{
	if (!SphereCollision)
	{
		return;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<ASkillProjectile*>(this));

	if (AActor* OwningActor = GetOwner())
	{
		ActorsToIgnore.AddUnique(OwningActor);

		TArray<AActor*> AttachedActors;
		OwningActor->GetAttachedActors(AttachedActors, true, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			ActorsToIgnore.AddUnique(AttachedActor);
		}
	}

	if (APawn* InstigatorPawn = GetInstigator())
	{
		ActorsToIgnore.AddUnique(InstigatorPawn);

		TArray<AActor*> AttachedActors;
		InstigatorPawn->GetAttachedActors(AttachedActors, true, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			ActorsToIgnore.AddUnique(AttachedActor);
		}
	}

	for (AActor* IgnoredActor : ActorsToIgnore)
	{
		if (!IsValid(IgnoredActor))
		{
			continue;
		}

		SphereCollision->IgnoreActorWhenMoving(IgnoredActor, true);

	}
}

void ASkillProjectile::HandleImpact(
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const FHitResult& Hit)
{
	if (bCosmeticOnly)
	{
		return;
	}

	if (bHasImpacted || !OtherComp || IsIgnoredImpactActor(OtherActor))
	{

		return;
	}

	// A character can own several query primitives (capsule, interaction sensor,
	// equipment and presentation volumes). None of those may consume a skill
	// projectile. Only the character's primary skeletal mesh is damage geometry.
	if (PdCharacterHitValidation::IsCharacterRelatedNonMeshHit(OtherActor, OtherComp))
	{
		return;
	}

	AActor* DamageTargetActor = ResolveDamageTargetActor(OtherActor, OtherComp);
	if (IsIgnoredImpactActor(DamageTargetActor))
	{

		return;
	}

	if (HasAuthority())
	{
		const bool bKeepProjectileAfterImpact =
			bStickOnImpact && PostImpactLifeSpan > UE_SMALL_NUMBER;
		ACharacterBase* StuckCharacter = nullptr;
		FName StuckBoneName = NAME_None;
		bHasImpacted = true;
		bKeepProjectileVisualAfterImpact = bKeepProjectileAfterImpact;
		MarkImpactStateDirty();

		const bool bHasReportedImpactPoint =
			(Hit.GetActor() != nullptr || Hit.GetComponent() != nullptr)
			&& !Hit.ImpactPoint.ContainsNaN();
		const bool bHasReportedImpactLocation =
			(Hit.GetActor() != nullptr || Hit.GetComponent() != nullptr)
			&& !Hit.Location.ContainsNaN();
		const FVector ImpactLocation = bKeepProjectileAfterImpact && bHasReportedImpactPoint
			? FVector(Hit.ImpactPoint)
			: (bHasReportedImpactLocation ? FVector(Hit.Location) : GetActorLocation());
		StopAtImpact(ImpactLocation);

		if (ImpactAreaDamageRadius > UE_SMALL_NUMBER)
		{
			TryApplyDamageInImpactArea(ImpactLocation);
		}
		else
		{
			TryApplyDamageToTarget(DamageTargetActor);
		}

		if (bKeepProjectileAfterImpact)
		{
			StuckCharacter = PdCharacterHitValidation::ResolveDirectMeshHit(
				OtherActor,
				OtherComp);

			if (StuckCharacter && IsValid(StuckCharacter->GetMesh()))
			{
				// Character impacts always attach to the authoritative primary mesh.
				// Hit.BoneName is normally populated by the mesh physics asset; use
				// the nearest bone as a safe fallback so animation keeps the icicle
				// embedded in the struck body part.
				StuckBoneName = ResolveImpactBoneName(
					StuckCharacter->GetMesh(),
					Hit,
					ImpactLocation);
			}
			else
			{
				const FName ImpactBoneName = ResolveImpactBoneName(
					OtherComp,
					Hit,
					ImpactLocation);
				AttachToImpactComponent(OtherComp, ImpactBoneName);
			}
		}
		else if (ProjectileEffect)
		{
			ProjectileEffect->Deactivate();
		}

		MulticastExecuteImpactGameplayCue(
			FVector_NetQuantize(ImpactLocation),
			HitFX.Get(),
			bSpawnHitNiagaraOnGround,
			ImpactGameplayCueTag,
			bKeepProjectileAfterImpact,
			StuckCharacter,
			StuckBoneName);

		FHitResult SkillHit = Hit;
		SkillHit.ImpactPoint = ImpactLocation;
		OnSkillImpact.Broadcast(DamageTargetActor, SkillHit);

		if (bKeepProjectileAfterImpact)
		{
			SetLifeSpan(PostImpactLifeSpan);
			ForceNetUpdate();
		}
		else
		{
			Destroy();
		}
	}
}

void ASkillProjectile::StopAtImpact(const FVector& ImpactLocation)
{
	DisableProjectileCollision();
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	if (!ImpactLocation.ContainsNaN())
	{
		SetActorLocation(
			ImpactLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
}

FName ASkillProjectile::ResolveImpactBoneName(
	const UPrimitiveComponent* ImpactComponent,
	const FHitResult& Hit,
	const FVector& ImpactLocation) const
{
	if (!IsValid(ImpactComponent))
	{
		return NAME_None;
	}

	if (!Hit.BoneName.IsNone() && ImpactComponent->DoesSocketExist(Hit.BoneName))
	{
		return Hit.BoneName;
	}

	if (const USkeletalMeshComponent* SkeletalMesh =
		Cast<USkeletalMeshComponent>(ImpactComponent))
	{
		const FName ClosestBoneName = SkeletalMesh->FindClosestBone(ImpactLocation);
		if (!ClosestBoneName.IsNone()
			&& SkeletalMesh->DoesSocketExist(ClosestBoneName))
		{
			return ClosestBoneName;
		}
	}

	return NAME_None;
}

void ASkillProjectile::AttachToImpactComponent(
	UPrimitiveComponent* OtherComp,
	const FName ImpactBoneName)
{
	if (!IsValid(OtherComp)
		|| OtherComp == SphereCollision
		|| !IsValid(OtherComp->GetOwner()))
	{
		return;
	}

	FName AttachSocketName = ImpactBoneName;
	if (!AttachSocketName.IsNone() && !OtherComp->DoesSocketExist(AttachSocketName))
	{
		AttachSocketName = NAME_None;
	}

	AttachToComponent(
		OtherComp,
		FAttachmentTransformRules::KeepWorldTransform,
		AttachSocketName);
}

bool ASkillProjectile::TryApplyDamageToTarget(AActor* TargetActor)
{
	if (!HasAuthority() || !IsValid(TargetActor) || !DamageEffectSpecHandle.IsValid())
	{

		return false;
	}

	if (const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(GetInstigator()))
	{
		if (const ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor))
		{
			if (!SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
			{

				return false;
			}
		}
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetInstigator());
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!SourceASC || !TargetASC || !DamageEffectSpecHandle.Data.IsValid())
	{

		return false;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageEffectSpecHandle.Data.Get(), TargetASC);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		TryApplyDebuffToTarget(TargetActor, SourceASC, TargetASC);
	}

	return AppliedHandle.WasSuccessfullyApplied();
}

bool ASkillProjectile::TryApplyDamageInImpactArea(const FVector& ImpactLocation)
{
	UWorld* World = GetWorld();
	if (!HasAuthority()
		|| !World
		|| ImpactAreaDamageRadius <= UE_SMALL_NUMBER
		|| !DamageEffectSpecHandle.IsValid()
		|| !DamageEffectSpecHandle.Data.IsValid())
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(LabCollisionChannels::HitableBody());

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(ProjectileImpactAreaDamage),
		false);
	if (AActor* OwningActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwningActor);
	}
	if (APawn* InstigatorPawn = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorPawn);
	}

	TArray<FOverlapResult> OverlapResults;
	if (!World->OverlapMultiByObjectType(
		OverlapResults,
		ImpactLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(ImpactAreaDamageRadius),
		QueryParams))
	{
		return false;
	}

	TSet<FObjectKey> ProcessedTargets;
	bool bAppliedAnyDamage = false;
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* TargetActor = ResolveDamageTargetActor(
			OverlapResult.GetActor(),
			OverlapResult.GetComponent());
		ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor);
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

		bAppliedAnyDamage |= TryApplyDamageToTarget(TargetCharacter);
	}

	return bAppliedAnyDamage;
}

bool ASkillProjectile::TryApplyDebuffToTarget(AActor* TargetActor, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC) const
{
	if (!HasAuthority()
		|| !DebuffEffectSpecHandle.IsValid()
		|| !DebuffEffectSpecHandle.Data.IsValid()
		|| !IsValid(TargetActor)
		|| !SourceASC
		|| !TargetASC
		|| !StatusEffectDefinition
		|| !StatusEffectDefinition->CanStack(TargetASC))
	{
		return false;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*DebuffEffectSpecHandle.Data.Get(), TargetASC);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		if (UStatusEffectReplicationComponent* ReplicationComponent =
			TargetActor->FindComponentByClass<UStatusEffectReplicationComponent>())
		{
			ReplicationComponent->TrackAppliedStatusEffect(
				StatusEffectDefinition,
				AppliedHandle);
		}
	}

	return AppliedHandle.WasSuccessfullyApplied();
}

AActor* ASkillProjectile::ResolveDamageTargetActor(
	AActor* OtherActor,
	const UPrimitiveComponent* OtherComponent) const
{
	if (!IsValid(OtherActor))
	{
		return nullptr;
	}

	if (PdCharacterHitValidation::ResolveRelatedCharacter(OtherActor, OtherComponent))
	{
		return PdCharacterHitValidation::ResolveDirectMeshHit(
			OtherActor,
			OtherComponent);
	}

	return OtherActor;
}

bool ASkillProjectile::IsIgnoredImpactActor(const AActor* OtherActor) const
{
	if (!IsValid(OtherActor))
	{
		return true;
	}

	const AActor* OwningActor = GetOwner();
	const APawn* InstigatorPawn = GetInstigator();
	const ACharacterBase* SourceCharacter = Cast<ACharacterBase>(InstigatorPawn);

	const AActor* CurrentActor = OtherActor;
	for (int32 Depth = 0; Depth < 8 && IsValid(CurrentActor); ++Depth)
	{
		if (CurrentActor == this || CurrentActor == OwningActor || CurrentActor == InstigatorPawn)
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

void ASkillProjectile::ExecuteSpawnGameplayCue() const
{
	if (!SpawnGameplayCueTag.IsValid())
	{

		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = GetActorLocation();
	Parameters.Instigator = GetInstigator();
	Parameters.EffectCauser = const_cast<ASkillProjectile*>(this);
	UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor(
		const_cast<ASkillProjectile*>(this),
		SpawnGameplayCueTag,
		Parameters);

}

void ASkillProjectile::ExecuteImpactGameplayCue()
{
	ExecuteImpactGameplayCueAtLocation(GetActorLocation());
}

void ASkillProjectile::ExecuteImpactGameplayCueAtLocation(const FVector& CueLocation)
{
	if (!ImpactGameplayCueTag.IsValid())
	{

		return;
	}

	if (bImpactCueExecuted)
	{

		return;
	}

	bImpactCueExecuted = true;

	FGameplayCueParameters Parameters;
	Parameters.Location = CueLocation;
	Parameters.Instigator = GetInstigator();
	Parameters.EffectCauser = this;
	UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor(
		this,
		ImpactGameplayCueTag,
		Parameters);

}

void ASkillProjectile::ApplyProjectileLoopVisual() const
{
	UNiagaraSystem* DesiredSystem = nullptr;
	if (!bHasImpacted || bKeepProjectileVisualAfterImpact)
	{
		DesiredSystem = Speed > 0.0f ? ProjectileFX.Get() : MuzzleFX.Get();
	}

	ApplyProjectileEffectSystem(DesiredSystem);
}

void ASkillProjectile::ApplyProjectileEffectSystem(UNiagaraSystem* DesiredSystem) const
{
	if (!ProjectileEffect)
	{
		return;
	}

	if (!DesiredSystem)
	{
		ProjectileEffect->Deactivate();
		return;
	}

	const bool bAssetChanged = ProjectileEffect->GetAsset() != DesiredSystem;
	if (bAssetChanged)
	{
		ProjectileEffect->DeactivateImmediate();
		ProjectileEffect->SetAsset(DesiredSystem);
		ProjectileEffect->ResetSystem();
	}
	else if (!ProjectileEffect->IsActive())
	{
		ProjectileEffect->ResetSystem();
	}

	ProjectileEffect->Activate(true);

}

void ASkillProjectile::ExecuteImpactNiagaraAtLocation(const FVector& CueLocation)
{
	if (!HitFX || bImpactNiagaraExecuted)
	{
		return;
	}

	bImpactNiagaraExecuted = true;
	const FTransform SpawnTransform = ResolveImpactNiagaraSpawnTransform(CueLocation);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		HitFX.Get(),
		SpawnTransform.GetLocation(),
		SpawnTransform.GetRotation().Rotator());

}

FTransform ASkillProjectile::ResolveImpactNiagaraSpawnTransform(const FVector& CueLocation) const
{
	if (!bSpawnHitNiagaraOnGround)
	{
		return FTransform(GetActorRotation(), CueLocation);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return FTransform(GetActorRotation(), CueLocation);
	}

	const FVector TraceStart = CueLocation + FVector(0.0, 0.0, HitNiagaraGroundTraceStartHeight);
	const FVector TraceEnd = CueLocation - FVector(0.0, 0.0, HitNiagaraGroundTraceDepth);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectileHitNiagaraGroundTrace), false);
	QueryParams.AddIgnoredActor(this);
	if (AActor* OwningActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwningActor);
	}
	if (APawn* InstigatorPawn = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorPawn);
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult GroundHit;
	if (World->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, ObjectParams, QueryParams) && GroundHit.bBlockingHit)
	{
		const FRotator GroundRotation = FRotationMatrix::MakeFromZ(GroundHit.Normal.GetSafeNormal()).Rotator();

		return FTransform(GroundRotation, GroundHit.Location);
	}

	return FTransform(GetActorRotation(), CueLocation);
}

void ASkillProjectile::StopReadiedScaleGrowth()
{
	if (!bReadiedScaleGrowthActive)
	{
		return;
	}

	UpdateReadiedScaleGrowth();
	bReadiedScaleGrowthActive = false;
	MarkReadiedScaleGrowthDirty();
	SetActorTickEnabled(false);
	ForceNetUpdate();
}

void ASkillProjectile::UpdateReadiedScaleGrowth()
{
	if (!bReadiedScaleGrowthActive)
	{
		return;
	}

	if (ReadiedScaleGrowthDuration <= UE_SMALL_NUMBER)
	{
		ApplyReadiedGrowthValue(1.0f);
		bReadiedScaleGrowthActive = false;
		MarkReadiedScaleGrowthDirty();
		SetActorTickEnabled(false);
		if (HasAuthority())
		{
			ForceNetUpdate();
		}
		return;
	}

	const float Alpha = GetReadiedScaleGrowthAlpha();
	ApplyReadiedGrowthValue(Alpha);

	if (Alpha >= 1.0f)
	{
		bReadiedScaleGrowthActive = false;
		MarkReadiedScaleGrowthDirty();
		SetActorTickEnabled(false);
		if (HasAuthority())
		{
			ForceNetUpdate();
		}
	}
}

float ASkillProjectile::GetSyncedWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

void ASkillProjectile::ApplyReadiedGrowthValue(const float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	SetActorScale3D(FMath::Lerp(ReadiedScaleGrowthStartScale, ReadiedScaleGrowthTargetScale, ClampedAlpha));

	if (!GetNormalizedReadiedNiagaraParameterName().IsNone())
	{
		const FVector2D Value(
			FMath::Lerp(ReadiedScaleGrowthNiagaraStartSize.X, ReadiedScaleGrowthNiagaraTargetSize.X, ClampedAlpha),
			FMath::Lerp(ReadiedScaleGrowthNiagaraStartSize.Y, ReadiedScaleGrowthNiagaraTargetSize.Y, ClampedAlpha));
		SetReadiedNiagaraVector2DParameter(Value);
	}
}

void ASkillProjectile::SetReadiedNiagaraVector2DParameter(const FVector2D Value) const
{
	const FName ParameterName = GetNormalizedReadiedNiagaraParameterName();
	if (ParameterName.IsNone())
	{
		return;
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents(NiagaraComponents);
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!NiagaraComponent)
		{
			continue;
		}

		NiagaraComponent->SetVariableVec2(ParameterName, Value);
	}
}

FName ASkillProjectile::GetNormalizedReadiedNiagaraParameterName() const
{
	if (ReadiedScaleGrowthNiagaraVector2DParameterName.IsNone())
	{
		return NAME_None;
	}

	const FString RawName = ReadiedScaleGrowthNiagaraVector2DParameterName.ToString();
	return RawName.StartsWith(TEXT("User."))
		? ReadiedScaleGrowthNiagaraVector2DParameterName
		: FName(*FString::Printf(TEXT("User.%s"), *RawName));
}

void ASkillProjectile::MarkProjectileFlightDataDirty()
{
	if (!HasAuthority() || !GetIsReplicated())
	{
		return;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, TargetLocation, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, Speed, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, bUseArcTrajectory, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ArcHeight, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ArcGravityScale, this);
}

void ASkillProjectile::MarkProjectileVisualsDirty()
{
	if (!HasAuthority() || !GetIsReplicated())
	{
		return;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, MuzzleFX, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ProjectileFX, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, HitFX, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, bSpawnHitNiagaraOnGround, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, SpawnGameplayCueTag, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ImpactGameplayCueTag, this);
}

void ASkillProjectile::MarkReadiedScaleGrowthDirty()
{
	if (!HasAuthority() || !GetIsReplicated())
	{
		return;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ReadiedScaleGrowthStartScale, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ReadiedScaleGrowthTargetScale, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ReadiedScaleGrowthDuration, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ReadiedScaleGrowthServerStartTime, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ReadiedScaleGrowthNiagaraVector2DParameterName, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ReadiedScaleGrowthNiagaraStartSize, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ReadiedScaleGrowthNiagaraTargetSize, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, bReadiedScaleGrowthActive, this);
}

void ASkillProjectile::MarkImpactStateDirty()
{
	if (!HasAuthority() || !GetIsReplicated())
	{
		return;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, bHasImpacted, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, bKeepProjectileVisualAfterImpact, this);
}

void ASkillProjectile::MulticastExecuteImpactGameplayCue_Implementation(
	FVector_NetQuantize CueLocation,
	UNiagaraSystem* InHitFX,
	const bool bInSpawnHitNiagaraOnGround,
	FGameplayTag InImpactGameplayCueTag,
	const bool bKeepProjectileVisual,
	ACharacterBase* InStuckCharacter,
	const FName InStuckBoneName)
{
	bHasImpacted = true;
	bKeepProjectileVisualAfterImpact = bKeepProjectileVisual;
	StopAtImpact(FVector(CueLocation));
	if (bKeepProjectileVisual
		&& IsValid(InStuckCharacter)
		&& IsValid(InStuckCharacter->GetMesh()))
	{
		AttachToImpactComponent(
			InStuckCharacter->GetMesh(),
			InStuckBoneName);
	}
	bool bVisualsChanged = false;
	if (!HitFX && InHitFX)
	{
		HitFX = InHitFX;
		bVisualsChanged = true;
	}
	if (bSpawnHitNiagaraOnGround != bInSpawnHitNiagaraOnGround)
	{
		bSpawnHitNiagaraOnGround = bInSpawnHitNiagaraOnGround;
		bVisualsChanged = true;
	}
	if (!ImpactGameplayCueTag.IsValid() && InImpactGameplayCueTag.IsValid())
	{
		ImpactGameplayCueTag = InImpactGameplayCueTag;
		bVisualsChanged = true;
	}
	if (bVisualsChanged)
	{
		MarkProjectileVisualsDirty();
	}
	if (ProjectileEffect && !bKeepProjectileVisual)
	{
		ProjectileEffect->Deactivate();
	}

	ExecuteImpactGameplayCueAtLocation(FVector(CueLocation));
	ExecuteImpactNiagaraAtLocation(FVector(CueLocation));
}
