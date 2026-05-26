#include "AbilitySystem/Projectiles/ProjectileBase.h"

#include "AbilitySystem/EffectActors/EffectAreaBase.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/PdCharacterBase.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayCueFunctionLibrary.h"
#include "GameplayEffect.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProjectileBase)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraProjectile, Log, All);

AProjectileBase::AProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);

	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SetRootComponent(SphereCollision);
	SphereCollision->InitSphereRadius(12.0f);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SphereCollision->SetCollisionObjectType(ECC_GameTraceChannel2);
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
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
}

void AProjectileBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(AProjectileBase, TargetLocation, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AProjectileBase, Speed, Params);
}

void AProjectileBase::InitializeProjectile(
	const FVector& InTargetLocation,
	float InSpeed,
	const FGameplayEffectSpecHandle& InDamageEffectSpecHandle)
{
	TargetLocation = InTargetLocation;
	Speed = FMath::Max(InSpeed, 0.0f);
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AProjectileBase, TargetLocation, this);
		MARK_PROPERTY_DIRTY_FROM_NAME(AProjectileBase, Speed, this);
	}
	DamageEffectSpecHandle = InDamageEffectSpecHandle;
	ConfigureIgnoredActors();
	UE_LOG(LogPandoraProjectile, Log,
		TEXT("InitializeProjectile: projectile=%s authority=%s target=%s speed=%.1f damageSpecValid=%s instigator=%s owner=%s"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*FVector(TargetLocation).ToCompactString(),
		Speed,
		DamageEffectSpecHandle.IsValid() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetInstigator()),
		*GetNameSafe(GetOwner()));
}

void AProjectileBase::SetDamageEffectSpecHandle(const FGameplayEffectSpecHandle& InDamageEffectSpecHandle)
{
	DamageEffectSpecHandle = InDamageEffectSpecHandle;
	UE_LOG(LogPandoraProjectile, Log,
		TEXT("SetDamageEffectSpecHandle: projectile=%s valid=%s"),
		*GetNameSafe(this),
		DamageEffectSpecHandle.IsValid() ? TEXT("true") : TEXT("false"));
}

void AProjectileBase::SetImpactEffectAreaSpawnConfigs(
	const TArray<FProjectileImpactEffectAreaSpawnConfig>& InImpactEffectAreaSpawnConfigs,
	const int32 InSourceSkillLevel)
{
	ImpactEffectAreaSpawnConfigs = InImpactEffectAreaSpawnConfigs;
	SourceSkillLevel = FMath::Max(InSourceSkillLevel, 1);

	UE_LOG(LogPandoraProjectile, Log,
		TEXT("SetImpactEffectAreaSpawnConfigs: projectile=%s configs=%d sourceLevel=%d"),
		*GetNameSafe(this),
		ImpactEffectAreaSpawnConfigs.Num(),
		SourceSkillLevel);
}

void AProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	if (SphereCollision)
	{
		ConfigureCollision();
		ConfigureIgnoredActors();
		SphereCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleSphereBeginOverlap);
		SphereCollision->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleSphereHit);
	}

	UE_LOG(LogPandoraProjectile, Log,
		TEXT("BeginPlay: projectile=%s role=%d authority=%s location=%s target=%s speed=%.1f instigator=%s owner=%s spawnCue=%s impactCue=%s"),
		*GetNameSafe(this),
		static_cast<int32>(GetLocalRole()),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetActorLocation().ToCompactString(),
		*FVector(TargetLocation).ToCompactString(),
		Speed,
		*GetNameSafe(GetInstigator()),
		*GetNameSafe(GetOwner()),
		*SpawnGameplayCueTag.ToString(),
		*ImpactGameplayCueTag.ToString());

	StartProjectileMovement();
	ExecuteSpawnGameplayCue();
}

void AProjectileBase::Destroyed()
{
	if (bHasImpacted && !bImpactCueExecuted)
	{
		ExecuteImpactGameplayCue();
	}

	UE_LOG(LogPandoraProjectile, Log,
		TEXT("Destroyed: projectile=%s impacted=%s impactCueExecuted=%s location=%s authority=%s"),
		*GetNameSafe(this),
		bHasImpacted ? TEXT("true") : TEXT("false"),
		bImpactCueExecuted ? TEXT("true") : TEXT("false"),
		*GetActorLocation().ToCompactString(),
		HasAuthority() ? TEXT("true") : TEXT("false"));

	Super::Destroyed();
}

void AProjectileBase::OnRep_ProjectileFlightData()
{
	UE_LOG(LogPandoraProjectile, Log,
		TEXT("OnRep_ProjectileFlightData: projectile=%s target=%s speed=%.1f location=%s"),
		*GetNameSafe(this),
		*FVector(TargetLocation).ToCompactString(),
		Speed,
		*GetActorLocation().ToCompactString());
	StartProjectileMovement();
}

void AProjectileBase::HandleSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	HandleImpact(OtherActor, OtherComp);
}

void AProjectileBase::HandleSphereHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	static_cast<void>(HitComponent);
	static_cast<void>(NormalImpulse);
	static_cast<void>(Hit);

	HandleImpact(OtherActor, OtherComp);
}

void AProjectileBase::StartProjectileMovement() const
{
	if (!ProjectileMovement || Speed <= 0.0f)
	{
		UE_LOG(LogPandoraProjectile, Warning,
			TEXT("StartProjectileMovement skipped: projectile=%s movement=%s speed=%.1f target=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ProjectileMovement),
			Speed,
			*FVector(TargetLocation).ToCompactString());
		return;
	}

	FVector Direction = (FVector(TargetLocation) - GetActorLocation()).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		UE_LOG(LogPandoraProjectile, Warning,
			TEXT("StartProjectileMovement direction is zero, using actor forward. projectile=%s location=%s target=%s"),
			*GetNameSafe(this),
			*GetActorLocation().ToCompactString(),
			*FVector(TargetLocation).ToCompactString());
		Direction = GetActorForwardVector();
	}

	ProjectileMovement->SetUpdatedComponent(SphereCollision);
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = Direction * Speed;
	ProjectileMovement->Activate(true);
	ProjectileMovement->UpdateComponentVelocity();
	UE_LOG(LogPandoraProjectile, Log,
		TEXT("StartProjectileMovement: projectile=%s location=%s target=%s direction=%s velocity=%s active=%s"),
		*GetNameSafe(this),
		*GetActorLocation().ToCompactString(),
		*FVector(TargetLocation).ToCompactString(),
		*Direction.ToCompactString(),
		*ProjectileMovement->Velocity.ToCompactString(),
		ProjectileMovement->IsActive() ? TEXT("true") : TEXT("false"));
}

void AProjectileBase::ConfigureCollision() const
{
	if (!SphereCollision)
	{
		return;
	}

	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SphereCollision->SetCollisionObjectType(ECC_GameTraceChannel2);
	SphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	SphereCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SphereCollision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	SphereCollision->SetGenerateOverlapEvents(true);
	SphereCollision->SetNotifyRigidBodyCollision(true);
	UE_LOG(LogPandoraProjectile, Log,
		TEXT("Projectile collision configured: projectile=%s objectType=ArrowProjectile pawn=Overlap hitableBody=Overlap"),
		*GetNameSafe(this));
}

void AProjectileBase::ConfigureIgnoredActors() const
{
	if (!SphereCollision)
	{
		return;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<AProjectileBase*>(this));

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
		UE_LOG(LogPandoraProjectile, Log,
			TEXT("Projectile movement ignoring actor: projectile=%s ignored=%s"),
			*GetNameSafe(this),
			*GetNameSafe(IgnoredActor));
	}
}

void AProjectileBase::HandleImpact(AActor* OtherActor, UPrimitiveComponent* OtherComp)
{
	if (bHasImpacted || !OtherComp || IsIgnoredImpactActor(OtherActor))
	{
		UE_LOG(LogPandoraProjectile, Log,
			TEXT("Impact ignored: projectile=%s other=%s otherComp=%s impacted=%s ignored=%s authority=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OtherActor),
			*GetNameSafe(OtherComp),
			bHasImpacted ? TEXT("true") : TEXT("false"),
			IsIgnoredImpactActor(OtherActor) ? TEXT("true") : TEXT("false"),
			HasAuthority() ? TEXT("true") : TEXT("false"));
		return;
	}

	AActor* DamageTargetActor = ResolveDamageTargetActor(OtherActor);
	if (IsIgnoredImpactActor(DamageTargetActor))
	{
		UE_LOG(LogPandoraProjectile, Log,
			TEXT("Impact ignored after target resolution: projectile=%s other=%s resolvedTarget=%s authority=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OtherActor),
			*GetNameSafe(DamageTargetActor),
			HasAuthority() ? TEXT("true") : TEXT("false"));
		return;
	}

	UE_LOG(LogPandoraProjectile, Log,
		TEXT("Impact: projectile=%s other=%s resolvedTarget=%s otherComp=%s authority=%s location=%s"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		*GetNameSafe(DamageTargetActor),
		*GetNameSafe(OtherComp),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetActorLocation().ToCompactString());

	if (HasAuthority())
	{
		const bool bAppliedDamage = TryApplyDamageToTarget(DamageTargetActor);
		TrySpawnImpactEffectAreas(DamageTargetActor, bAppliedDamage);
		UE_LOG(LogPandoraProjectile, Log,
			TEXT("Impact authority result: projectile=%s target=%s damageApplied=%s"),
			*GetNameSafe(this),
			*GetNameSafe(DamageTargetActor),
			bAppliedDamage ? TEXT("true") : TEXT("false"));
		bHasImpacted = true;
		MulticastExecuteImpactGameplayCue(FVector_NetQuantize(GetActorLocation()));
		Destroy();
	}
	else
	{
		UE_LOG(LogPandoraProjectile, Log,
			TEXT("Impact on remote copy: projectile=%s target=%s damage skipped."),
			*GetNameSafe(this),
			*GetNameSafe(DamageTargetActor));
	}
}

bool AProjectileBase::TryApplyDamageToTarget(AActor* TargetActor)
{
	if (!HasAuthority() || !IsValid(TargetActor) || !DamageEffectSpecHandle.IsValid())
	{
		UE_LOG(LogPandoraProjectile, Warning,
			TEXT("TryApplyDamageToTarget failed early: projectile=%s authority=%s target=%s damageSpecValid=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(TargetActor),
			DamageEffectSpecHandle.IsValid() ? TEXT("true") : TEXT("false"));
		return false;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetInstigator());
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!SourceASC || !TargetASC || !DamageEffectSpecHandle.Data.IsValid())
	{
		UE_LOG(LogPandoraProjectile, Warning,
			TEXT("TryApplyDamageToTarget failed ASC/spec check: projectile=%s instigator=%s sourceASC=%s target=%s targetASC=%s specDataValid=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetInstigator()),
			*GetNameSafe(SourceASC),
			*GetNameSafe(TargetActor),
			*GetNameSafe(TargetASC),
			DamageEffectSpecHandle.Data.IsValid() ? TEXT("true") : TEXT("false"));
		return false;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*DamageEffectSpecHandle.Data.Get(), TargetASC);
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		TryApplyDebuffToTarget(TargetActor, SourceASC, TargetASC);
	}

	UE_LOG(LogPandoraProjectile, Log,
		TEXT("TryApplyDamageToTarget: projectile=%s sourceASC=%s target=%s targetASC=%s applied=%s"),
		*GetNameSafe(this),
		*GetNameSafe(SourceASC),
		*GetNameSafe(TargetActor),
		*GetNameSafe(TargetASC),
		AppliedHandle.WasSuccessfullyApplied() ? TEXT("true") : TEXT("false"));
	return AppliedHandle.WasSuccessfullyApplied();
}

void AProjectileBase::TrySpawnImpactEffectAreas(AActor* TargetActor, const bool bDamageApplied)
{
	if (!HasAuthority() || ImpactEffectAreaSpawnConfigs.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !IsValid(TargetActor))
	{
		UE_LOG(LogPandoraProjectile, Warning,
			TEXT("TrySpawnImpactEffectAreas skipped: projectile=%s world=%s target=%s configs=%d"),
			*GetNameSafe(this),
			World ? TEXT("valid") : TEXT("null"),
			*GetNameSafe(TargetActor),
			ImpactEffectAreaSpawnConfigs.Num());
		return;
	}

	for (const FProjectileImpactEffectAreaSpawnConfig& SpawnConfig : ImpactEffectAreaSpawnConfigs)
	{
		if (!SpawnConfig.EffectAreaClass)
		{
			UE_LOG(LogPandoraProjectile, Warning,
				TEXT("Impact effect area skipped: projectile=%s target=%s reason=MissingClass sourceLevel=%d"),
				*GetNameSafe(this),
				*GetNameSafe(TargetActor),
				SourceSkillLevel);
			continue;
		}

		if (SpawnConfig.bRequireSuccessfulDamageApplication && !bDamageApplied)
		{
			UE_LOG(LogPandoraProjectile, Log,
				TEXT("Impact effect area skipped: projectile=%s target=%s area=%s damageApplied=false"),
				*GetNameSafe(this),
				*GetNameSafe(TargetActor),
				*GetNameSafe(SpawnConfig.EffectAreaClass.Get()));
			continue;
		}

		FTransform SpawnTransform;
		if (!ResolveImpactEffectAreaSpawnTransform(SpawnConfig, TargetActor, SpawnTransform))
		{
			UE_LOG(LogPandoraProjectile, Warning,
				TEXT("Impact effect area skipped: projectile=%s target=%s area=%s reason=ResolveSpawnTransformFailed"),
				*GetNameSafe(this),
				*GetNameSafe(TargetActor),
				*GetNameSafe(SpawnConfig.EffectAreaClass.Get()));
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = GetInstigator();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AEffectAreaBase* SpawnedArea = World->SpawnActor<AEffectAreaBase>(
			SpawnConfig.EffectAreaClass,
			SpawnTransform,
			SpawnParams);
		if (SpawnedArea && SpawnConfig.LifeSpan > 0.0)
		{
			SpawnedArea->SetLifeSpan(static_cast<float>(SpawnConfig.LifeSpan));
		}

		UE_LOG(LogPandoraProjectile, Log,
			TEXT("Impact effect area spawn: projectile=%s target=%s areaClass=%s spawned=%s sourceLevel=%d lifeSpan=%.2f location=%s rotation=%s"),
			*GetNameSafe(this),
			*GetNameSafe(TargetActor),
			*GetNameSafe(SpawnConfig.EffectAreaClass.Get()),
			*GetNameSafe(SpawnedArea),
			SourceSkillLevel,
			SpawnConfig.LifeSpan,
			*SpawnTransform.GetLocation().ToCompactString(),
			*SpawnTransform.GetRotation().Rotator().ToCompactString());
	}
}

bool AProjectileBase::ResolveImpactEffectAreaSpawnTransform(
	const FProjectileImpactEffectAreaSpawnConfig& SpawnConfig,
	AActor* TargetActor,
	FTransform& OutSpawnTransform) const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const FVector BaseLocation = SpawnConfig.bSpawnAtTargetFeet ? TargetActor->GetActorLocation() : GetActorLocation();
	FVector SpawnLocation = BaseLocation;
	FVector GroundNormal = FVector::UpVector;

	if (SpawnConfig.bSpawnAtTargetFeet)
	{
		const FVector TraceStart = BaseLocation + FVector(0.0, 0.0, FMath::Max(SpawnConfig.GroundTraceStartHeight, 0.0));
		const FVector TraceEnd = BaseLocation - FVector(0.0, 0.0, FMath::Max(SpawnConfig.GroundTraceDepth, 100.0));

		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Add(const_cast<AProjectileBase*>(this));
		ActorsToIgnore.Add(TargetActor);
		if (AActor* OwningActor = GetOwner())
		{
			ActorsToIgnore.AddUnique(OwningActor);
		}
		if (APawn* InstigatorPawn = GetInstigator())
		{
			ActorsToIgnore.AddUnique(InstigatorPawn);
		}

		FHitResult GroundHit;
		const bool bHit = UKismetSystemLibrary::LineTraceSingle(
			this,
			TraceStart,
			TraceEnd,
			SpawnConfig.GroundTraceChannel,
			false,
			ActorsToIgnore,
			EDrawDebugTrace::None,
			GroundHit,
			true);

		if (bHit && GroundHit.bBlockingHit)
		{
			SpawnLocation = GroundHit.Location;
			GroundNormal = GroundHit.Normal.GetSafeNormal();
			UE_LOG(LogPandoraProjectile, Log,
				TEXT("Impact effect area ground resolved: projectile=%s target=%s start=%s end=%s ground=%s groundActor=%s"),
				*GetNameSafe(this),
				*GetNameSafe(TargetActor),
				*TraceStart.ToCompactString(),
				*TraceEnd.ToCompactString(),
				*SpawnLocation.ToCompactString(),
				*GetNameSafe(GroundHit.GetActor()));
		}
		else
		{
			UE_LOG(LogPandoraProjectile, Warning,
				TEXT("Impact effect area ground trace missed: projectile=%s target=%s start=%s end=%s, using target location."),
				*GetNameSafe(this),
				*GetNameSafe(TargetActor),
				*TraceStart.ToCompactString(),
				*TraceEnd.ToCompactString());
		}
	}

	SpawnLocation += SpawnConfig.SpawnOffset;

	const FRotator SpawnRotation = SpawnConfig.bAlignToGroundNormal && !GroundNormal.IsNearlyZero()
		? FRotationMatrix::MakeFromZ(GroundNormal).Rotator()
		: FRotator(0.0, GetActorRotation().Yaw, 0.0);
	OutSpawnTransform = FTransform(SpawnRotation, SpawnLocation);
	return true;
}

bool AProjectileBase::TryApplyDebuffToTarget(AActor* TargetActor, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC) const
{
	if (!HasAuthority() || !DebuffEffectClass || !IsValid(TargetActor) || !SourceASC || !TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddInstigator(GetInstigator(), const_cast<AProjectileBase*>(this));
	EffectContext.AddSourceObject(const_cast<AProjectileBase*>(this));

	FGameplayEffectSpecHandle DebuffSpecHandle = SourceASC->MakeOutgoingSpec(
		DebuffEffectClass,
		FMath::Max(DebuffEffectLevel, 1.0f),
		EffectContext);

	if (!DebuffSpecHandle.IsValid() || !DebuffSpecHandle.Data.IsValid())
	{
		UE_LOG(LogPandoraProjectile, Warning,
			TEXT("TryApplyDebuffToTarget failed: invalid spec. projectile=%s debuff=%s target=%s"),
			*GetNameSafe(this),
			*GetNameSafe(DebuffEffectClass.Get()),
			*GetNameSafe(TargetActor));
		return false;
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*DebuffSpecHandle.Data.Get(), TargetASC);

	UE_LOG(LogPandoraProjectile, Log,
		TEXT("TryApplyDebuffToTarget: projectile=%s debuff=%s target=%s applied=%s"),
		*GetNameSafe(this),
		*GetNameSafe(DebuffEffectClass.Get()),
		*GetNameSafe(TargetActor),
		AppliedHandle.WasSuccessfullyApplied() ? TEXT("true") : TEXT("false"));
	return AppliedHandle.WasSuccessfullyApplied();
}

AActor* AProjectileBase::ResolveDamageTargetActor(AActor* OtherActor) const
{
	if (!IsValid(OtherActor))
	{
		return nullptr;
	}

	if (Cast<APdCharacterBase>(OtherActor))
	{
		return OtherActor;
	}

	AActor* CurrentActor = OtherActor;
	for (int32 Depth = 0; Depth < 8 && IsValid(CurrentActor); ++Depth)
	{
		AActor* OwnerActor = CurrentActor->GetOwner();
		if (Cast<APdCharacterBase>(OwnerActor))
		{
			return OwnerActor;
		}

		AActor* AttachParentActor = CurrentActor->GetAttachParentActor();
		if (Cast<APdCharacterBase>(AttachParentActor))
		{
			return AttachParentActor;
		}

		CurrentActor = OwnerActor ? OwnerActor : AttachParentActor;
	}

	return OtherActor;
}

bool AProjectileBase::IsIgnoredImpactActor(const AActor* OtherActor) const
{
	if (!IsValid(OtherActor))
	{
		return true;
	}

	const AActor* OwningActor = GetOwner();
	const APawn* InstigatorPawn = GetInstigator();

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

void AProjectileBase::ExecuteSpawnGameplayCue() const
{
	if (!SpawnGameplayCueTag.IsValid())
	{
		UE_LOG(LogPandoraProjectile, Log,
			TEXT("Spawn gameplay cue skipped: projectile=%s tag invalid."),
			*GetNameSafe(this));
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = GetActorLocation();
	Parameters.Instigator = GetInstigator();
	Parameters.EffectCauser = const_cast<AProjectileBase*>(this);
	UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor(
		const_cast<AProjectileBase*>(this),
		SpawnGameplayCueTag,
		Parameters);
	UE_LOG(LogPandoraProjectile, Log,
		TEXT("Spawn gameplay cue executed: projectile=%s tag=%s location=%s"),
		*GetNameSafe(this),
		*SpawnGameplayCueTag.ToString(),
		*GetActorLocation().ToCompactString());
}

void AProjectileBase::ExecuteImpactGameplayCue()
{
	ExecuteImpactGameplayCueAtLocation(GetActorLocation());
}

void AProjectileBase::ExecuteImpactGameplayCueAtLocation(const FVector& CueLocation)
{
	if (!ImpactGameplayCueTag.IsValid())
	{
		UE_LOG(LogPandoraProjectile, Log,
			TEXT("Impact gameplay cue skipped: projectile=%s tag invalid."),
			*GetNameSafe(this));
		return;
	}

	if (bImpactCueExecuted)
	{
		UE_LOG(LogPandoraProjectile, Log,
			TEXT("Impact gameplay cue skipped: projectile=%s tag=%s already executed."),
			*GetNameSafe(this),
			*ImpactGameplayCueTag.ToString());
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
	UE_LOG(LogPandoraProjectile, Log,
		TEXT("Impact gameplay cue executed: projectile=%s tag=%s location=%s authority=%s"),
		*GetNameSafe(this),
		*ImpactGameplayCueTag.ToString(),
		*CueLocation.ToCompactString(),
		HasAuthority() ? TEXT("true") : TEXT("false"));
}

void AProjectileBase::MulticastExecuteImpactGameplayCue_Implementation(FVector_NetQuantize CueLocation)
{
	bHasImpacted = true;
	UE_LOG(LogPandoraProjectile, Log,
		TEXT("Impact gameplay cue multicast received: projectile=%s tag=%s location=%s authority=%s"),
		*GetNameSafe(this),
		*ImpactGameplayCueTag.ToString(),
		*FVector(CueLocation).ToCompactString(),
		HasAuthority() ? TEXT("true") : TEXT("false"));
	ExecuteImpactGameplayCueAtLocation(FVector(CueLocation));
}
