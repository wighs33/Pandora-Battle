#include "Item/ArrowProjectileBase.h"

#include "Character/PdCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(ArrowProjectileBase)

DEFINE_LOG_CATEGORY_STATIC(LogArrowProjectileBase, Log, All);

AArrowProjectileBase::AArrowProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetBoxExtent(FVector(38.0f, 2.0f, 1.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionBox->SetGenerateOverlapEvents(false);
	CollisionBox->SetCanEverAffectNavigation(false);

	ArrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
	ArrowMesh->SetupAttachment(CollisionBox);
	ArrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArrowMesh->SetCollisionResponseToAllChannels(ECR_Ignore);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionBox);
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->Deactivate();
}

void AArrowProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	bHasImpacted = false;

	if (CollisionBox)
	{
		CollisionBox->SetGenerateOverlapEvents(false);
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AArrowProjectileBase::HandleCollisionOverlap);
		CollisionBox->OnComponentHit.AddDynamic(this, &AArrowProjectileBase::HandleCollisionHit);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	SetActorHiddenInGame(false);
	if (ArrowMesh)
	{
		ArrowMesh->SetHiddenInGame(false);
		ArrowMesh->SetVisibility(true, true);
	}
}

bool AArrowProjectileBase::LaunchArrowActor(const FVector& Direction)
{
	if (bHasImpacted)
	{
		return false;
	}

	SetActorHiddenInGame(false);
	if (ArrowMesh)
	{
		ArrowMesh->SetHiddenInGame(false);
		ArrowMesh->SetVisibility(true, true);
	}

	UPrimitiveComponent* CollisionComponent = GetCollisionComponent();
	if (!CollisionComponent)
	{
		return false;
	}

	if (LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}

	if (TrailSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrailSystem,
			CollisionComponent,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			true);
	}

	UProjectileMovementComponent* ProjectileMovementComponent = GetProjectileMovementComponent();
	if (!ProjectileMovementComponent)
	{
		return false;
	}

	ProjectileMovementComponent->SetUpdatedComponent(CollisionComponent);
	ProjectileMovementComponent->MaxSpeed = LaunchSpeed;
	ProjectileMovementComponent->Velocity = Direction.GetSafeNormal() * LaunchSpeed;
	ProjectileMovementComponent->ProjectileGravityScale = LaunchGravityScale;
	ProjectileMovementComponent->Activate(true);
	ProjectileMovementComponent->UpdateComponentVelocity();
	if (HasAuthority())
	{
		CollisionComponent->SetGenerateOverlapEvents(true);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	return true;
}

void AArrowProjectileBase::HandleCollisionOverlap(
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

	TryHandleImpact(OtherActor, OtherComp);
}

void AArrowProjectileBase::HandleCollisionHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	static_cast<void>(HitComponent);
	static_cast<void>(NormalImpulse);
	static_cast<void>(Hit);

	TryHandleImpact(OtherActor, OtherComp);
}

UPrimitiveComponent* AArrowProjectileBase::GetCollisionComponent() const
{
	if (CollisionBox)
	{
		return CollisionBox;
	}

	return Cast<UPrimitiveComponent>(GetRootComponent());
}

UProjectileMovementComponent* AArrowProjectileBase::GetProjectileMovementComponent() const
{
	if (ProjectileMovement)
	{
		return ProjectileMovement;
	}

	return FindComponentByClass<UProjectileMovementComponent>();
}

APdCharacterBase* AArrowProjectileBase::GetOwningCharacter() const
{
	if (APdCharacterBase* OwnerCharacter = Cast<APdCharacterBase>(GetOwner()))
	{
		return OwnerCharacter;
	}

	return Cast<APdCharacterBase>(GetInstigator());
}

AWeaponBase* AArrowProjectileBase::GetOwningWeapon() const
{
	const APdCharacterBase* OwnerCharacter = GetOwningCharacter();
	const UEquipmentComponent* EquipmentComponent = OwnerCharacter ? OwnerCharacter->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

AActor* AArrowProjectileBase::ResolveDamageTargetActor(AActor* OtherActor) const
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

bool AArrowProjectileBase::IsIgnoredImpactActor(const AActor* OtherActor) const
{
	if (!IsValid(OtherActor))
	{
		return true;
	}

	const AActor* OwningActor = GetOwner();
	const APawn* InstigatorPawn = GetInstigator();
	const APdCharacterBase* OwningCharacter = GetOwningCharacter();

	const AActor* CurrentActor = OtherActor;
	for (int32 Depth = 0; Depth < 8 && IsValid(CurrentActor); ++Depth)
	{
		if (CurrentActor == this || CurrentActor == OwningActor || CurrentActor == InstigatorPawn || CurrentActor == OwningCharacter)
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

void AArrowProjectileBase::StopProjectileMotion()
{
	if (UProjectileMovementComponent* ProjectileMovementComponent = GetProjectileMovementComponent())
	{
		ProjectileMovementComponent->StopMovementImmediately();
		ProjectileMovementComponent->Velocity = FVector::ZeroVector;
		ProjectileMovementComponent->Deactivate();
		ProjectileMovementComponent->UpdateComponentVelocity();
	}
}

bool AArrowProjectileBase::TryHandleImpact(AActor* OtherActor, UPrimitiveComponent* OtherComp)
{
	if (!HasAuthority() || bHasImpacted || !OtherComp || IsIgnoredImpactActor(OtherActor))
	{
		return false;
	}

	AActor* DamageTargetActor = ResolveDamageTargetActor(OtherActor);
	if (IsIgnoredImpactActor(DamageTargetActor))
	{
		return false;
	}

	bHasImpacted = true;

	if (AWeaponBase* OwningWeapon = GetOwningWeapon())
	{
		OwningWeapon->RequestServerApplyDamage(DamageTargetActor);
	}
	else
	{
		UE_LOG(
			LogArrowProjectileBase,
			Warning,
			TEXT("%s impact could not resolve owning weapon. Owner=%s Instigator=%s Target=%s"),
			*GetName(),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(GetInstigator()),
			*GetNameSafe(DamageTargetActor));
	}

	StopProjectileMotion();

	if (UPrimitiveComponent* CollisionComponent = GetCollisionComponent())
	{
		CollisionComponent->SetGenerateOverlapEvents(false);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	AttachToComponent(
		OtherComp,
		FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true));

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}

	SetLifeSpan(ImpactLifeSpan);
	return true;
}
