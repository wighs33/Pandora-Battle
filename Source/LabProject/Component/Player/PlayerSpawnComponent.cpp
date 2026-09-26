#include "Component/Player/PlayerSpawnComponent.h"

#include "Character/CharacterBase.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerSpawnComponent)

UPlayerSpawnComponent::UPlayerSpawnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerSpawnComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	StopRespawning();
	OnPlayerRespawned.Clear();
	UsedPlayerStarts.Reset();
	AssignedPlayerStartsByController.Reset();
	InitialPlayerSpawnTransforms.Reset();
	LastRandomRespawnPlayerStartNames.Reset();

	Super::EndPlay(EndPlayReason);
}

AActor* UPlayerSpawnComponent::ChooseConfiguredPlayerStart(
	AController* Player, FName SpawnIndexTagPrefix)
{
	if (Player)
	{
		if (const TWeakObjectPtr<AActor>* AssignedStart = AssignedPlayerStartsByController.Find(TObjectKey<AController>(Player));
			AssignedStart && AssignedStart->IsValid())
		{
			return AssignedStart->Get();
		}
	}
	const APdPlayerState* PlayerState = Player ? Player->GetPlayerState<APdPlayerState>() : nullptr;
	const UPlayerMatchComponent* MatchComponent = PlayerState ? PlayerState->GetPlayerMatchComponent() : nullptr;
	const int32 SpawnIndex = MatchComponent ? MatchComponent->GetMatchSpawnIndex() : INDEX_NONE;
	if (AActor* TaggedPlayerStart =
		FindPlayerStartBySpawnIndex(
			SpawnIndex,
			SpawnIndexTagPrefix))
	{
		MarkPlayerStartUsed(Player, TaggedPlayerStart);
		return TaggedPlayerStart;
	}

	if (AActor* UnusedPlayerStart = FindFirstUnusedPlayerStart())
	{
		MarkPlayerStartUsed(Player, UnusedPlayerStart);
		return UnusedPlayerStart;
	}

	return nullptr;
}

void UPlayerSpawnComponent::MarkPlayerStartUsed(
	AController* Player,
	AActor* PlayerStart)
{
	if (!PlayerStart)
	{
		return;
	}

	if (!IsPlayerStartUsed(PlayerStart))
	{
		UsedPlayerStarts.Add(PlayerStart);
	}

	if (Player)
	{
		TWeakObjectPtr<AActor>& Previous = AssignedPlayerStartsByController.FindOrAdd(TObjectKey<AController>(Player));
		if (Previous.IsValid() && Previous.Get() != PlayerStart) { UsedPlayerStarts.Remove(Previous.Get()); }
		Previous = PlayerStart;
	}
}

void UPlayerSpawnComponent::RecordInitialSpawn(AController* PlayerController, const FTransform& InitialSpawnTransform)
{
	if (PlayerController)
	{
		// 같은 경기의 리스폰으로 처음 배정된 위치가 덮어써지지 않게 한다.
		InitialPlayerSpawnTransforms.FindOrAdd(TObjectKey<AController>(PlayerController), InitialSpawnTransform);
	}
}

void UPlayerSpawnComponent::RequestPlayerRespawn(
	AController* PlayerController,
	APawn* DeadPawn)
{
	AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !PlayerController
		|| !PlayerController->IsPlayerController())
	{
		return;
	}

	if (!bRespawningEnabled)
	{
		return;
	}

	const TObjectKey<AController> ControllerKey(PlayerController);
	if (PendingPlayerRespawnTimers.Contains(ControllerKey))
	{
		return;
	}

	TWeakObjectPtr<AController> WeakPlayerController(PlayerController);
	TWeakObjectPtr<APawn> WeakDeadPawn(DeadPawn);
	const float RespawnDelay = FMath::Max(MatchRules->PlayerRespawnDelay, 0.0f);
	if (ACharacterBase* DeadCharacter = Cast<ACharacterBase>(DeadPawn))
	{
		DeadCharacter->ClearCharacterOverlayMaterial();
		if (RespawnDelay > 0.0f)
		{
			DeadCharacter->StartDeathDissolve(RespawnDelay);
		}
	}

	if (APdPlayerController* PdPlayerController =
		Cast<APdPlayerController>(PlayerController))
	{
		PdPlayerController->Client_StartRespawnDelayCountdown(
			RespawnDelay);
	}

	if (RespawnDelay <= 0.0f)
	{
		FinishPlayerRespawn(WeakPlayerController, WeakDeadPawn);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerHandle RespawnTimerHandle;
	World->GetTimerManager().SetTimer(
		RespawnTimerHandle,
		FTimerDelegate::CreateWeakLambda(
			this,
			[this, WeakPlayerController, WeakDeadPawn, ControllerKey]()
			{
				PendingPlayerRespawnTimers.Remove(ControllerKey);
				FinishPlayerRespawn(
					WeakPlayerController,
					WeakDeadPawn);
			}),
		RespawnDelay,
		false);
	PendingPlayerRespawnTimers.Add(
		ControllerKey,
		RespawnTimerHandle);
}

bool UPlayerSpawnComponent::TryGetPlayerInitialSpawnTransform(
	AController* PlayerController,
	FTransform& OutSpawnTransform) const
{
	if (PlayerController)
	{
		if (const FTransform* CachedSpawnTransform =
			InitialPlayerSpawnTransforms.Find(
				TObjectKey<AController>(PlayerController)))
		{
			OutSpawnTransform = *CachedSpawnTransform;
			return true;
		}

		if (const TWeakObjectPtr<AActor>* AssignedPlayerStart =
			AssignedPlayerStartsByController.Find(
				TObjectKey<AController>(PlayerController)))
		{
			if (const AActor* PlayerStart = AssignedPlayerStart->Get())
			{
				OutSpawnTransform = PlayerStart->GetActorTransform();
				return true;
			}
		}
	}

	return false;
}

TArray<APlayerController*> UPlayerSpawnComponent::MovePlayersToInitialSpawns()
{
	TArray<APlayerController*> MovedPlayers;
	const AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	UWorld* World = GetWorld();
	if (!GameMode || !GameMode->HasAuthority() || !World)
	{
		return MovedPlayers;
	}

	for (FConstPlayerControllerIterator Iterator =
			World->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		APawn* Pawn =
			PlayerController ? PlayerController->GetPawn() : nullptr;
		if (!PlayerController || !Pawn)
		{
			continue;
		}

		FTransform InitialSpawnTransform;
		if (!TryGetPlayerInitialSpawnTransform(
			PlayerController,
			InitialSpawnTransform))
		{
			continue;
		}

		Pawn->SetActorTransform(
			InitialSpawnTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		PlayerController->SetControlRotation(
			InitialSpawnTransform.GetRotation().Rotator());
		MovedPlayers.Add(PlayerController);
	}
	return MovedPlayers;
}

void UPlayerSpawnComponent::ClearRuntimeStateForController(
	AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	const TObjectKey<AController> ControllerKey(Controller);
	if (UWorld* World = GetWorld())
	{
		if (FTimerHandle* RespawnTimerHandle =
			PendingPlayerRespawnTimers.Find(ControllerKey))
		{
			World->GetTimerManager().ClearTimer(*RespawnTimerHandle);
		}
	}

	PendingPlayerRespawnTimers.Remove(ControllerKey);
	InitialPlayerSpawnTransforms.Remove(ControllerKey);
	LastRandomRespawnPlayerStartNames.Remove(ControllerKey);

	if (TWeakObjectPtr<AActor>* AssignedPlayerStart =
		AssignedPlayerStartsByController.Find(ControllerKey))
	{
		if (AActor* PlayerStart = AssignedPlayerStart->Get())
		{
			UsedPlayerStarts.Remove(PlayerStart);
		}
	}
	AssignedPlayerStartsByController.Remove(ControllerKey);
}

AActor* UPlayerSpawnComponent::FindPlayerStartBySpawnIndex(
	const int32 SpawnIndex,
	const FName PlayerStartTagPrefix) const
{
	if (SpawnIndex == INDEX_NONE)
	{
		return nullptr;
	}

	const FString Prefix = PlayerStartTagPrefix.ToString();
	const FName DesiredPlayerStartTag(
		*FString::Printf(TEXT("%s%d"), *Prefix, SpawnIndex));

	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		APlayerStart::StaticClass(),
		PlayerStarts);
	PlayerStarts.Sort(
		[](const AActor& A, const AActor& B)
		{
			return A.GetName() < B.GetName();
		});

	for (AActor* PlayerStartActor : PlayerStarts)
	{
		const APlayerStart* PlayerStart =
			Cast<APlayerStart>(PlayerStartActor);
		if (PlayerStart
			&& PlayerStart->PlayerStartTag == DesiredPlayerStartTag
			&& !IsPlayerStartUsed(PlayerStartActor))
		{
			return PlayerStartActor;
		}
	}

	return nullptr;
}

AActor* UPlayerSpawnComponent::FindFirstUnusedPlayerStart() const
{
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		APlayerStart::StaticClass(),
		PlayerStarts);
	PlayerStarts.Sort(
		[](const AActor& A, const AActor& B)
		{
			return A.GetName() < B.GetName();
		});

	for (AActor* PlayerStart : PlayerStarts)
	{
		if (PlayerStart && !IsPlayerStartUsed(PlayerStart))
		{
			return PlayerStart;
		}
	}

	return nullptr;
}

bool UPlayerSpawnComponent::IsPlayerStartUsed(
	const AActor* PlayerStart) const
{
	return PlayerStart
		&& UsedPlayerStarts.ContainsByPredicate(
			[PlayerStart](const TObjectPtr<AActor>& UsedPlayerStart)
			{
				return UsedPlayerStart.Get() == PlayerStart;
			});
}

void UPlayerSpawnComponent::FinishPlayerRespawn(TWeakObjectPtr<AController> WeakPlayerController, TWeakObjectPtr<APawn> WeakDeadPawn)
{
	AGameModeBase* GameMode = Cast<AGameModeBase>(GetOwner());
	AController* Controller = WeakPlayerController.Get();
	if (!GameMode || !GameMode->HasAuthority() || !Controller || !bRespawningEnabled)
	{
		return;
	}
	PendingPlayerRespawnTimers.Remove(TObjectKey<AController>(Controller));
	APdPlayerController* PdController = Cast<APdPlayerController>(Controller);
	FTransform Transform;
	if (!TryGetPlayerRespawnTransform(Controller, Transform))
	{
		UE_LOG(LogTemp, Error, TEXT("No respawn PlayerStart for %s."), *GetNameSafe(Controller));
		if (PdController) { PdController->Client_HideRespawnDelayCountdown(); }
		return;
	}

	if (APdPlayerState* State = Controller->GetPlayerState<APdPlayerState>())
	{
		if (UPdAbilitySystemComponent* ASC = Cast<UPdAbilitySystemComponent>(State->GetAbilitySystemComponent()))
		{
			ASC->ResetRuntimeStateForRespawn();
		}
	}
	APawn* Pawn = Controller->GetPawn();
	if (!IsValid(Pawn))
	{
		Pawn = WeakDeadPawn.Get();
		if (IsValid(Pawn)) { Controller->Possess(Pawn); }
	}
	const bool bCreatedPawn = !IsValid(Pawn);
	if (bCreatedPawn)
	{
		GameMode->RestartPlayerAtTransform(Controller, Transform);
		Pawn = Controller->GetPawn();
	}
	if (IsValid(Pawn))
	{
		if (ACharacterBase* Character = Cast<ACharacterBase>(Pawn))
		{
			Character->ResetDeathStateForRespawnAtTransform(Transform);
		}
		else
		{
			Pawn->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->ForceNetUpdate();
		}
		Controller->SetControlRotation(Transform.GetRotation().Rotator());
		if (PdController) { PdController->Client_ResetRespawnedPawnStateAtTransform(Pawn, Transform); }
	}
	if (PdController) { PdController->Client_HideRespawnDelayCountdown(); }
	if (IsValid(Pawn)) { OnPlayerRespawned.Broadcast(Cast<APlayerController>(Controller), bCreatedPawn); }
}

bool UPlayerSpawnComponent::TryGetPlayerRespawnTransform(AController* Controller, FTransform& OutTransform)
{
	if (RespawnLocation == EPlayerRespawnLocation::InitialSpawn)
	{
		return TryGetPlayerInitialSpawnTransform(Controller, OutTransform);
	}
	AActor* Start = nullptr;
	if (RespawnLocation == EPlayerRespawnLocation::RandomPlayerStart)
	{
		Start = FindRandomRespawnPlayerStart(Controller, *MatchRules);
		if (Start) { LastRandomRespawnPlayerStartNames.FindOrAdd(TObjectKey<AController>(Controller)) = Start->GetFName(); }
	}
	if (!Start) { return false; }
	OutTransform = Start->GetActorTransform();
	return true;
}

AActor* UPlayerSpawnComponent::FindRandomRespawnPlayerStart(
	AController* PlayerController,
	const UMatchRuleDefinition& Rules) const
{
	UWorld* World = GetWorld();
	if (!World || Rules.RandomRespawnPlayerStartTags.IsEmpty())
	{
		return nullptr;
	}

	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(
		World,
		APlayerStart::StaticClass(),
		PlayerStarts);
	PlayerStarts.Sort(
		[](const AActor& A, const AActor& B)
		{
			return A.GetName() < B.GetName();
		});

	TArray<AActor*> Candidates;
	for (AActor* PlayerStartActor : PlayerStarts)
	{
		const APlayerStart* PlayerStart =
			Cast<APlayerStart>(PlayerStartActor);
		if (PlayerStart
			&& DoesPlayerStartMatchRandomRespawnTags(
				PlayerStart,
				Rules))
		{
			Candidates.Add(PlayerStartActor);
		}
	}

	if (Candidates.IsEmpty())
	{
		return nullptr;
	}

	if (Rules.bAvoidLastRandomRespawnPlayerStart
		&& Candidates.Num() > 1
		&& PlayerController)
	{
		const FName* LastPlayerStartName =
			LastRandomRespawnPlayerStartNames.Find(
				TObjectKey<AController>(PlayerController));
		if (LastPlayerStartName && !LastPlayerStartName->IsNone())
		{
			Candidates.RemoveAll(
				[LastPlayerStartName](const AActor* Candidate)
				{
					return Candidate
						&& Candidate->GetFName() == *LastPlayerStartName;
				});
		}
	}

	return Candidates.IsEmpty()
		? nullptr
		: Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

bool UPlayerSpawnComponent::DoesPlayerStartMatchRandomRespawnTags(
	const APlayerStart* PlayerStart,
	const UMatchRuleDefinition& Rules) const
{
	if (!PlayerStart)
	{
		return false;
	}

	for (const FName& RespawnTag :
		Rules.RandomRespawnPlayerStartTags)
	{
		if (!RespawnTag.IsNone()
			&& (PlayerStart->PlayerStartTag == RespawnTag
				|| PlayerStart->ActorHasTag(RespawnTag)))
		{
			return true;
		}
	}

	return false;
}

void UPlayerSpawnComponent::Initialize(const UMatchRuleDefinition* InMatchRules)
{
	check(InMatchRules);
	MatchRules = InMatchRules;
	RespawnLocation = MatchRules->bUseRandomPlayerStartRespawns
		? EPlayerRespawnLocation::RandomPlayerStart : EPlayerRespawnLocation::InitialSpawn;
	bRespawningEnabled = true;
}

void UPlayerSpawnComponent::StopRespawning()
{
	bRespawningEnabled = false;
	if (UWorld* World = GetWorld())
	{
		for (auto& Pair : PendingPlayerRespawnTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
			if (APdPlayerController* Controller = Cast<APdPlayerController>(Pair.Key.ResolveObjectPtr()))
			{
				Controller->Client_HideRespawnDelayCountdown();
			}
		}
	}
	PendingPlayerRespawnTimers.Reset();
}
