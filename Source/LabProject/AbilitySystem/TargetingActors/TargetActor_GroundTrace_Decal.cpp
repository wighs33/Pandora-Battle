#include "AbilitySystem/TargetingActors/TargetActor_GroundTrace_Decal.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TargetActor_GroundTrace_Decal)

ATargetActor_GroundTrace_Decal::ATargetActor_GroundTrace_Decal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
	RootComponent = DefaultSceneRoot;
}

void ATargetActor_GroundTrace_Decal::BeginPlay()
{
	Super::BeginPlay();

	if (!Decal || !DefaultSceneRoot)
	{
		return;
	}

	DestroySpawnedDecal();

	SpawnedDecalComponent = UGameplayStatics::SpawnDecalAttached(
		Decal,
		FVector(50.0, DecalSize, DecalSize),
		DefaultSceneRoot,
		NAME_None,
		FVector::ZeroVector,
		FRotator(-90.0, 0.0, 0.0),
		EAttachLocation::KeepRelativeOffset,
		0.0f);

	if (SpawnedDecalComponent)
	{
		SpawnedDecalComponent->SetDecalColor(DecalColor);
	}
}

void ATargetActor_GroundTrace_Decal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroySpawnedDecal();
	Super::EndPlay(EndPlayReason);
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
