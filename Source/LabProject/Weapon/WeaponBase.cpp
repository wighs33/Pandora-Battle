#include "Weapon/WeaponBase.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponBase)

DEFINE_LOG_CATEGORY(WeaponBaseLog);

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TrailStartPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TrailStartPoint"));
	TrailStartPoint->SetupAttachment(WeaponMesh);

	TrailEndPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TrailEndPoint"));
	TrailEndPoint->SetupAttachment(WeaponMesh);
}
