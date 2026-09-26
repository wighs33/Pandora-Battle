#include "Map/OutOfBoundsVolume.h"

#include "Character/CharacterBase.h"
#include "Common/CollisionChannels.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Component/Player/PlayerSpawnComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OutOfBoundsVolume)

AOutOfBoundsVolume::AOutOfBoundsVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	BoundarySphere = CreateDefaultSubobject<USphereComponent>(TEXT("BoundarySphere"));
	SetRootComponent(BoundarySphere);

	BoundarySphere->InitSphereRadius(10000.0f);
	ConfigureBoundaryCollision();
}

void AOutOfBoundsVolume::BeginPlay()
{
	Super::BeginPlay();

	ConfigureBoundaryCollision();
	BoundarySphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleBoundaryEndOverlap);
}

void AOutOfBoundsVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;

	BoundarySphere->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleBoundaryEndOverlap);

	Super::EndPlay(EndPlayReason);
}

void AOutOfBoundsVolume::HandleBoundaryEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (!HasAuthority() || ShouldIgnoreEndOverlap(OtherActor))
	{
		return;
	}

	ACharacterBase* PlayerCharacter = Cast<ACharacterBase>(OtherActor);
	if (PlayerCharacter)
	{
		if (bRespawnPlayersOnEndOverlap && TryRespawnPlayer(PlayerCharacter))
		{
			BP_OnPlayerRespawnedFromOutOfBounds(PlayerCharacter);
		}
		return;
	}

	TryDestroyNonPlayerActor(OtherActor);
}

bool AOutOfBoundsVolume::TryRespawnPlayer(ACharacterBase* PlayerCharacter)
{
	AController* Controller = PlayerCharacter->GetController();
	if (!Controller)
	{
		return false;
	}

	AGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	UPlayerSpawnComponent* Spawn = GameMode ? GameMode->FindComponentByClass<UPlayerSpawnComponent>() : nullptr;
	if (Spawn)
	{
		Spawn->RequestPlayerRespawn(Controller, PlayerCharacter);
		return true;
	}

	return false;
}

bool AOutOfBoundsVolume::TryDestroyNonPlayerActor(AActor* Actor) const
{
	if (!bDestroyNonPlayerActorsOnEndOverlap || !IsValid(Actor) || Actor == this || Actor->IsNetStartupActor())
	{
		return false;
	}

	if (Actor->IsA<ACharacterBase>() || Actor->IsA<AController>() || Actor->IsA<APlayerState>()
		|| Actor->IsA<AGameModeBase>() || Actor->IsA<AGameStateBase>())
	{
		return false;
	}

	return Actor->Destroy();
}

bool AOutOfBoundsVolume::ShouldIgnoreEndOverlap(const AActor* OtherActor) const
{
	const UWorld* World = GetWorld();
	if (!World || bEndingPlay || IsActorBeingDestroyed() || World->bIsTearingDown || !World->IsGameWorld())
	{
		return true;
	}

	return !IsValid(OtherActor)
		|| OtherActor->IsActorBeingDestroyed()
		|| !OtherActor->HasActorBegunPlay();
}

void AOutOfBoundsVolume::ConfigureBoundaryCollision() const
{
	BoundarySphere->SetCollisionProfileName(TEXT("Custom"));
	BoundarySphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoundarySphere->SetCollisionObjectType(LabCollisionChannels::OverlapBox());
	BoundarySphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundarySphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BoundarySphere->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Overlap);
	BoundarySphere->SetGenerateOverlapEvents(true);
}
