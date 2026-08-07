#include "Map/OutOfBoundsRespawnVolume.h"

#include "AbilitySystem/EffectActors/EffectAreaBase.h"
#include "AbilitySystem/Projectiles/ProjectileBase.h"
#include "Character/CharacterBase.h"
#include "Common/CollisionChannels.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Item/ArrowProjectileBase.h"
#include "Lobby/Contents/LobbyGameMode.h"
#include "Map/TransientActorRegistrySubsystem.h"
#include "Mode/ExperienceGameMode.h"
#include "UI/DamageIndicatorActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OutOfBoundsRespawnVolume)

AOutOfBoundsRespawnVolume::AOutOfBoundsRespawnVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	BoundarySphere = CreateDefaultSubobject<USphereComponent>(TEXT("BoundarySphere"));
	SetRootComponent(BoundarySphere);

	BoundarySphere->InitSphereRadius(10000.0f);
	ConfigureBoundaryCollision();

	CleanupActorClasses.Add(AProjectileBase::StaticClass());
	CleanupActorClasses.Add(AArrowProjectileBase::StaticClass());
	CleanupActorClasses.Add(AEffectAreaBase::StaticClass());
	CleanupActorClasses.Add(ADamageIndicatorActor::StaticClass());
}

void AOutOfBoundsRespawnVolume::BeginPlay()
{
	Super::BeginPlay();

	if (BoundarySphere)
	{
		ConfigureBoundaryCollision();
		BoundarySphere->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleBoundaryEndOverlap);

	}
}

void AOutOfBoundsRespawnVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;

	if (BoundarySphere)
	{
		BoundarySphere->OnComponentEndOverlap.RemoveDynamic(this, &ThisClass::HandleBoundaryEndOverlap);
	}

	Super::EndPlay(EndPlayReason);
}

void AOutOfBoundsRespawnVolume::HandleBoundaryEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (ShouldIgnoreEndOverlap(OtherActor))
	{
		return;
	}

	if (!HasAuthority())
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

bool AOutOfBoundsRespawnVolume::TryRespawnPlayer(ACharacterBase* PlayerCharacter)
{
	if (!PlayerCharacter)
	{
		return false;
	}

	AController* Controller = PlayerCharacter->GetController();
	if (!Controller)
	{

		return false;
	}

	const int32 CleanedActorCount = bCleanupTransientActorsOnPlayerExit
		? CleanupTransientActors(PlayerCharacter)
		: 0;

	AExperienceGameMode* ExperienceGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AExperienceGameMode>() : nullptr;
	if (ExperienceGameMode)
	{
		ExperienceGameMode->RequestPlayerRespawn(Controller, PlayerCharacter);

		return true;
	}

	ALobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ALobbyGameMode>() : nullptr;
	if (LobbyGameMode)
	{
		LobbyGameMode->RequestLobbyPlayerRespawn(Controller, PlayerCharacter);

		return true;
	}

	return false;
}

bool AOutOfBoundsRespawnVolume::TryDestroyNonPlayerActor(AActor* Actor) const
{
	if (!bDestroyNonPlayerActorsOnEndOverlap || !CanCleanupActor(Actor, nullptr))
	{
		return false;
	}

Actor->Destroy();
	return true;
}

int32 AOutOfBoundsRespawnVolume::CleanupTransientActors(ACharacterBase* TriggeringPlayer) const
{
	UWorld* World = GetWorld();
	UTransientActorRegistrySubsystem* Registry =
		World ? World->GetSubsystem<UTransientActorRegistrySubsystem>() : nullptr;
	if (!Registry || !IsValid(TriggeringPlayer))
	{
		return 0;
	}

	TArray<AActor*> RegisteredActors;
	Registry->GetTransientActorsForSource(TriggeringPlayer, RegisteredActors);

	int32 DestroyedCount = 0;
	for (AActor* Actor : RegisteredActors)
	{
		if (!IsConfiguredCleanupClass(Actor)
			|| !CanCleanupActor(Actor, TriggeringPlayer))
		{
			continue;
		}

		if (Actor->Destroy())
		{
			++DestroyedCount;
		}
	}

	return DestroyedCount;
}

bool AOutOfBoundsRespawnVolume::CanCleanupActor(const AActor* Actor, const ACharacterBase* TriggeringPlayer) const
{
	if (!IsValid(Actor) || Actor == this || Actor == TriggeringPlayer)
	{
		return false;
	}

	if (Actor->IsA<ACharacterBase>()
		|| Actor->IsA<AController>()
		|| Actor->IsA<APlayerState>()
		|| Actor->IsA<AGameModeBase>()
		|| Actor->IsA<AGameStateBase>())
	{
		return false;
	}

	return bAllowCleanupOfNetStartupActors || !Actor->IsNetStartupActor();
}

bool AOutOfBoundsRespawnVolume::IsConfiguredCleanupClass(const AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return false;
	}

	for (const TSubclassOf<AActor>& CleanupClass : CleanupActorClasses)
	{
		if (CleanupClass && Actor->IsA(CleanupClass))
		{
			return true;
		}
	}

	return false;
}

bool AOutOfBoundsRespawnVolume::ShouldIgnoreEndOverlap(const AActor* OtherActor) const
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

void AOutOfBoundsRespawnVolume::ConfigureBoundaryCollision() const
{
	if (!BoundarySphere)
	{
		return;
	}

	BoundarySphere->SetCollisionProfileName(TEXT("Custom"));
	BoundarySphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoundarySphere->SetCollisionObjectType(LabCollisionChannels::OverlapBox());
	BoundarySphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundarySphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	BoundarySphere->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Overlap);
	BoundarySphere->SetGenerateOverlapEvents(true);
}
