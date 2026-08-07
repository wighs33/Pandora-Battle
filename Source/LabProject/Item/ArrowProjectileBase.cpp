#include "Item/ArrowProjectileBase.h"

#include "Character/CharacterBase.h"
#include "Character/CharacterHitValidation.h"
#include "Common/CollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Map/TransientActorRegistrySubsystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(ArrowProjectileBase)

namespace
{
void ConfigureArrowCollision(UPrimitiveComponent* CollisionComponent)
{
	if (!CollisionComponent)
	{
		return;
	}

	CollisionComponent->SetCollisionObjectType(LabCollisionChannels::Projectile());
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Overlap);
	CollisionComponent->SetGenerateOverlapEvents(false);
	CollisionComponent->SetNotifyRigidBodyCollision(true);
	CollisionComponent->SetCanEverAffectNavigation(false);
	CollisionComponent->SetHiddenInGame(true, false);
	CollisionComponent->SetVisibility(false, false);
	if (UBoxComponent* BoxComponent = Cast<UBoxComponent>(CollisionComponent))
	{
		BoxComponent->bDrawOnlyIfSelected = true;
	}
}

TArray<TEnumAsByte<EObjectTypeQuery>> MakeArrowImpactTraceObjectTypes()
{
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(LabCollisionChannels::HitableBody()));
	return ObjectTypes;
}
}

AArrowProjectileBase::AArrowProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetBoxExtent(FVector(38.0f, 2.0f, 1.0f));
	ConfigureArrowCollision(CollisionBox);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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

	if (UWorld* World = GetWorld())
	{
		if (UTransientActorRegistrySubsystem* Registry =
			World->GetSubsystem<UTransientActorRegistrySubsystem>())
		{
			Registry->RegisterTransientActor(this, GetOwningCharacter());
		}
	}

	bHasImpacted = false;
	bImpactTraceActive = false;
	PreviousImpactTraceLocation = GetActorLocation();
	SetActorTickEnabled(false);

	if (CollisionBox)
	{
		ConfigureArrowCollision(CollisionBox);
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

void AArrowProjectileBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
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
		ConfigureArrowCollision(CollisionComponent);
		CollisionComponent->SetGenerateOverlapEvents(true);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PreviousImpactTraceLocation = GetActorLocation();
		bImpactTraceActive = true;
		SetActorTickEnabled(true);
	}
	return true;
}

void AArrowProjectileBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	static_cast<void>(DeltaSeconds);
	PerformImpactTrace();
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

ACharacterBase* AArrowProjectileBase::GetOwningCharacter() const
{
	if (ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner()))
	{
		return OwnerCharacter;
	}

	return Cast<ACharacterBase>(GetInstigator());
}

AWeaponBase* AArrowProjectileBase::GetOwningWeapon() const
{
	const ACharacterBase* OwnerCharacter = GetOwningCharacter();
	const UEquipmentComponent* EquipmentComponent = OwnerCharacter ? OwnerCharacter->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

bool AArrowProjectileBase::IsIgnoredImpactActor(const AActor* OtherActor) const
{
	if (!IsValid(OtherActor))
	{
		return true;
	}

	const AActor* OwningActor = GetOwner();
	const APawn* InstigatorPawn = GetInstigator();
	const ACharacterBase* OwningCharacter = GetOwningCharacter();

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

		if (OwningCharacter)
		{
			if (const ACharacterBase* OtherCharacter = Cast<ACharacterBase>(CurrentActor))
			{
				if (!OwningCharacter->CanDamageCharacterByTeam(OtherCharacter))
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

void AArrowProjectileBase::PerformImpactTrace()
{
	if (!HasAuthority() || !bImpactTraceActive || bHasImpacted)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	if (PreviousImpactTraceLocation.IsNearlyZero())
	{
		PreviousImpactTraceLocation = CurrentLocation;
		return;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);
	if (AActor* OwnerActor = GetOwner())
	{
		ActorsToIgnore.Add(OwnerActor);
	}
	if (APawn* InstigatorPawn = GetInstigator())
	{
		ActorsToIgnore.Add(InstigatorPawn);
	}
	if (ACharacterBase* OwningCharacter = GetOwningCharacter())
	{
		ActorsToIgnore.Add(OwningCharacter);
	}

	const UBoxComponent* TraceBox = Cast<UBoxComponent>(GetCollisionComponent());
	if (!TraceBox)
	{
		return;
	}

	// Match the authored collision box exactly; a rotated world AABB is too broad for impact correction.
	TArray<FHitResult> HitResults;
	const bool bHit = UKismetSystemLibrary::BoxTraceMultiForObjects(
		this,
		PreviousImpactTraceLocation,
		CurrentLocation,
		TraceBox->GetScaledBoxExtent(),
		TraceBox->GetComponentRotation(),
		MakeArrowImpactTraceObjectTypes(),
		false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResults,
		true);

	PreviousImpactTraceLocation = CurrentLocation;

	if (!bHit)
	{
		return;
	}

	for (const FHitResult& HitResult : HitResults)
	{
		if (TryHandleImpact(HitResult.GetActor(), HitResult.GetComponent()))
		{
			break;
		}
	}
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

	bImpactTraceActive = false;
	SetActorTickEnabled(false);
}

bool AArrowProjectileBase::TryHandleImpact(AActor* OtherActor, UPrimitiveComponent* OtherComp)
{
	if (!HasAuthority() || bHasImpacted || !OtherComp || IsIgnoredImpactActor(OtherActor))
	{
		return false;
	}

	if (PdCharacterHitValidation::IsCharacterRelatedNonWeaponDamageHit(OtherActor, OtherComp))
	{
		return false;
	}

	ACharacterBase* DamageTargetCharacter =
		PdCharacterHitValidation::ResolveWeaponDamageHit(OtherActor, OtherComp);
	bHasImpacted = true;

	if (DamageTargetCharacter)
	{
		if (AWeaponBase* OwningWeapon = GetOwningWeapon())
		{
			OwningWeapon->ApplyDamageFromAuthoritativeProjectileImpact(
				OtherActor,
				OtherComp,
				this);
		}
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
	if (UPrimitiveComponent* CollisionComponent = GetCollisionComponent())
	{
		CollisionComponent->SetHiddenInGame(true, false);
		CollisionComponent->SetVisibility(false, false);
	}

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());
	}

	SetLifeSpan(ImpactLifeSpan);
	return true;
}
