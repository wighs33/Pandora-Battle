#include "Skill/Actors/SkillProjectile.h"

#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Skill/Actors/SkillProjectileFlight.h"
#include "Skill/Actors/SkillProjectileHit.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillProjectile)

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
	PdSkillProjectileHit::ApplyCollisionProfile(SphereCollision);
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
	DOREPLIFETIME_WITH_PARAMS_FAST(ASkillProjectile, ReadiedGrowth, Params);
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
	ResetImpactState();
	PdSkillProjectileHit::IgnoreSourceActors(SphereCollision, *this);
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
	PdSkillProjectileHit::DisableCollision(SphereCollision);
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
	ResetImpactState();
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
	PdSkillProjectileHit::DisableCollision(SphereCollision);
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
		// 자랄 것이 없으면 최종 크기만 바로 적용한다.
		StopReadiedScaleGrowth();
		ReadiedGrowth.StartScale = InStartScale;
		ReadiedGrowth.TargetScale = InTargetScale;
		ReadiedGrowth.NiagaraVector2DParameterName = InNiagaraVector2DParameterName;
		ReadiedGrowth.NiagaraStartSize = InNiagaraStartSize;
		ReadiedGrowth.NiagaraTargetSize = InNiagaraTargetSize;
		ApplyReadiedGrowthValue(1.0f);
		MarkReadiedScaleGrowthDirty();
		if (HasAuthority() && GetIsReplicated())
		{
			ForceNetUpdate();
		}
		return;
	}

	ReadiedGrowth.StartScale = InStartScale;
	ReadiedGrowth.TargetScale = InTargetScale;
	ReadiedGrowth.Duration = ClampedDuration;
	ReadiedGrowth.ServerStartTime = GetSyncedWorldTimeSeconds();
	ReadiedGrowth.NiagaraVector2DParameterName = InNiagaraVector2DParameterName;
	ReadiedGrowth.NiagaraStartSize = InNiagaraStartSize;
	ReadiedGrowth.NiagaraTargetSize = InNiagaraTargetSize;
	ReadiedGrowth.bActive = true;
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
	return ReadiedGrowth.GetAlpha(GetSyncedWorldTimeSeconds());
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
	ResetImpactState();
	PdSkillProjectileHit::ApplyCollisionProfile(SphereCollision);
	PdSkillProjectileHit::IgnoreSourceActors(SphereCollision, *this);
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

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddUniqueDynamic(this, &ThisClass::HandleProjectileStopped);
	}

	if (SphereCollision)
	{
		if (!bCosmeticOnly && Speed > 0.0f)
		{
			PdSkillProjectileHit::ApplyCollisionProfile(SphereCollision);
		}
		else
		{
			PdSkillProjectileHit::DisableCollision(SphereCollision);
		}
		PdSkillProjectileHit::IgnoreSourceActors(SphereCollision, *this);
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
		PdSkillProjectilePresentation::ExecuteCue(*this, SpawnGameplayCueTag, GetActorLocation());
	}
}

void ASkillProjectile::Destroyed()
{
	if (bHasImpacted && !bImpactCueExecuted)
	{
		ExecuteImpactGameplayCueAtLocation(GetActorLocation());
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
		PdSkillProjectileHit::DisableCollision(SphereCollision);
		if (Speed > 0.0f)
		{
			StartProjectileMovement();
		}
		ApplyProjectileLoopVisual();
		return;
	}

	if (Speed > 0.0f)
	{
		PdSkillProjectileHit::ApplyCollisionProfile(SphereCollision);
		StartProjectileMovement();
	}
	else
	{
		PdSkillProjectileHit::DisableCollision(SphereCollision);
	}
	ApplyProjectileLoopVisual();
}

void ASkillProjectile::OnRep_ProjectileVisuals()
{
	ApplyProjectileLoopVisual();
}

void ASkillProjectile::OnRep_ReadiedScaleGrowth()
{
	if (ReadiedGrowth.bActive)
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

void ASkillProjectile::HandleProjectileStopped(const FHitResult& Hit)
{
	AActor* OtherActor = Hit.GetActor();
	UPrimitiveComponent* OtherComponent = Hit.GetComponent();
	if (!bCosmeticOnly && !bHasImpacted && SphereCollision && IsValid(OtherComponent)
		&& (PdSkillProjectileHit::IsIgnoredImpactActor(*this, OtherActor)
			|| PdCharacterHitValidation::IsCharacterRelatedNonMeshHit(OtherActor, OtherComponent)))
	{
		// Ignoring damage alone does not prevent ProjectileMovement from stopping on a blocker.
		SphereCollision->IgnoreComponentWhenMoving(OtherComponent, true);
		StartProjectileMovement();
		return;
	}

	// Initial penetration can stop projectile movement without a component hit notification.
	// HandleImpact guards against processing an already reported hit twice.
	HandleImpact(OtherActor, OtherComponent, Hit);
}

void ASkillProjectile::HandleImpact(
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	const FHitResult& Hit)
{
	if (bCosmeticOnly || bHasImpacted || !OtherComp || PdSkillProjectileHit::IsIgnoredImpactActor(*this, OtherActor))
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

	AActor* DamageTargetActor = PdSkillProjectileHit::ResolveDamageTarget(OtherActor, OtherComp);
	if (PdSkillProjectileHit::IsIgnoredImpactActor(*this, DamageTargetActor) || !HasAuthority())
	{
		return;
	}

	const bool bKeepProjectileAfterImpact = bStickOnImpact && PostImpactLifeSpan > UE_SMALL_NUMBER;
	bHasImpacted = true;
	bKeepProjectileVisualAfterImpact = bKeepProjectileAfterImpact;
	MarkImpactStateDirty();

	const FVector ImpactLocation =
		PdSkillProjectileHit::ResolveImpactLocation(Hit, bKeepProjectileAfterImpact, GetActorLocation());
	StopAtImpact(ImpactLocation);

	FSkillProjectileDamage Damage;
	Damage.DamageSpec = DamageEffectSpecHandle;
	Damage.DebuffSpec = DebuffEffectSpecHandle;
	Damage.StatusEffect = StatusEffectDefinition;
	if (ImpactAreaDamageRadius > UE_SMALL_NUMBER)
	{
		PdSkillProjectileHit::ApplyDamageInArea(*this, ImpactLocation, ImpactAreaDamageRadius, Damage);
	}
	else
	{
		PdSkillProjectileHit::ApplyDamageToTarget(*this, DamageTargetActor, Damage);
	}

	ACharacterBase* StuckCharacter = nullptr;
	FName StuckBoneName = NAME_None;
	if (bKeepProjectileAfterImpact)
	{
		StuckCharacter = PdCharacterHitValidation::ResolveDirectMeshHit(OtherActor, OtherComp);
		if (StuckCharacter && IsValid(StuckCharacter->GetMesh()))
		{
			// Character impacts always attach to the authoritative primary mesh.
			// Hit.BoneName is normally populated by the mesh physics asset; use
			// the nearest bone as a safe fallback so animation keeps the icicle
			// embedded in the struck body part.
			StuckBoneName = PdSkillProjectileHit::ResolveImpactBoneName(StuckCharacter->GetMesh(), Hit, ImpactLocation);
		}
		else
		{
			AttachToImpactComponent(OtherComp, PdSkillProjectileHit::ResolveImpactBoneName(OtherComp, Hit, ImpactLocation));
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

void ASkillProjectile::ResetImpactState()
{
	bHasImpacted = false;
	bKeepProjectileVisualAfterImpact = false;
	bImpactCueExecuted = false;
	bImpactNiagaraExecuted = false;
}

void ASkillProjectile::StartProjectileMovement() const
{
	if (!ProjectileMovement)
	{
		return;
	}

	PdSkillProjectileFlight::FLaunchParams Params;
	Params.StartLocation = GetActorLocation();
	Params.TargetLocation = FVector(TargetLocation);
	Params.FallbackDirection = GetActorForwardVector();
	Params.Speed = Speed;
	Params.bUseArcTrajectory = bUseArcTrajectory;
	Params.ArcHeight = ArcHeight;
	Params.ArcGravityScale = ArcGravityScale;
	if (const UWorld* World = GetWorld())
	{
		Params.WorldGravityZ = World->GetGravityZ();
	}
	PdSkillProjectileFlight::Launch(*ProjectileMovement, SphereCollision, Params);
}

void ASkillProjectile::StopAtImpact(const FVector& ImpactLocation)
{
	PdSkillProjectileHit::DisableCollision(SphereCollision);
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

void ASkillProjectile::ExecuteImpactGameplayCueAtLocation(const FVector& CueLocation)
{
	if (!ImpactGameplayCueTag.IsValid() || bImpactCueExecuted)
	{
		return;
	}

	bImpactCueExecuted = true;
	PdSkillProjectilePresentation::ExecuteCue(*this, ImpactGameplayCueTag, CueLocation);
}

// 날아가는 동안은 비행 이펙트, 대기 중에는 총구 이펙트를 쓰고, 맞은 뒤에는 박힌 투사체만 이펙트를 유지한다.
void ASkillProjectile::ApplyProjectileLoopVisual() const
{
	UNiagaraSystem* DesiredSystem = nullptr;
	if (!bHasImpacted || bKeepProjectileVisualAfterImpact)
	{
		DesiredSystem = Speed > 0.0f ? ProjectileFX.Get() : MuzzleFX.Get();
	}

	PdSkillProjectilePresentation::ApplyLoopEffect(ProjectileEffect, DesiredSystem);
}

void ASkillProjectile::ExecuteImpactNiagaraAtLocation(const FVector& CueLocation)
{
	if (!HitFX || bImpactNiagaraExecuted)
	{
		return;
	}

	bImpactNiagaraExecuted = true;
	PdSkillProjectilePresentation::SpawnImpactEffect(*this, HitFX.Get(), CueLocation, bSpawnHitNiagaraOnGround);
}

void ASkillProjectile::StopReadiedScaleGrowth()
{
	if (!ReadiedGrowth.bActive)
	{
		return;
	}

	UpdateReadiedScaleGrowth();
	ReadiedGrowth.bActive = false;
	MarkReadiedScaleGrowthDirty();
	SetActorTickEnabled(false);
	ForceNetUpdate();
}

void ASkillProjectile::UpdateReadiedScaleGrowth()
{
	if (!ReadiedGrowth.bActive)
	{
		return;
	}

	const float Alpha = ReadiedGrowth.Duration <= UE_SMALL_NUMBER ? 1.0f : GetReadiedScaleGrowthAlpha();
	ApplyReadiedGrowthValue(Alpha);
	if (Alpha < 1.0f)
	{
		return;
	}

	ReadiedGrowth.bActive = false;
	MarkReadiedScaleGrowthDirty();
	SetActorTickEnabled(false);
	if (HasAuthority())
	{
		ForceNetUpdate();
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
	SetActorScale3D(ReadiedGrowth.GetScale(Alpha));
	PdSkillProjectilePresentation::SetNiagaraVector2D(
		*this,
		ReadiedGrowth.GetNiagaraParameterName(),
		ReadiedGrowth.GetNiagaraSize(Alpha));
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

	MARK_PROPERTY_DIRTY_FROM_NAME(ASkillProjectile, ReadiedGrowth, this);
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

	// 복제 속성보다 먼저 도착한 클라이언트도 같은 충돌 연출을 쓰도록 비어 있는 값만 채운다.
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
