#include "Component/Match/MatchSpawnComponent.h"

#include "Character/CharacterBase.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Match/MatchFlowComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MatchSpawnComponent)

UMatchSpawnComponent::UMatchSpawnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMatchSpawnComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		for (TPair<TObjectKey<AController>, FTimerHandle>& Pair :
			PendingPlayerRespawnTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}

	PendingPlayerRespawnTimers.Reset();
	UsedPlayerStarts.Reset();
	AssignedPlayerStartsByController.Reset();
	InitialPlayerSpawnTransforms.Reset();
	LastRandomRespawnPlayerStartNames.Reset();

	Super::EndPlay(EndPlayReason);
}

AExperienceGameMode*
UMatchSpawnComponent::GetExperienceGameMode() const
{
	return Cast<AExperienceGameMode>(GetOwner());
}

const AExperienceGameMode*
UMatchSpawnComponent::GetExperienceGameModeConst() const
{
	return Cast<AExperienceGameMode>(GetOwner());
}

AActor* UMatchSpawnComponent::ChooseConfiguredPlayerStart(
	AController* Player)
{
	if (!Settings.bUseLobbySpawnIndexPlayerStarts)
	{
		return nullptr;
	}

	const APdPlayerState* PlayerState = Player ? Player->GetPlayerState<APdPlayerState>() : nullptr;
	const UPlayerMatchComponent* MatchComponent = PlayerState ? PlayerState->GetPlayerMatchComponent() : nullptr;
	const int32 SpawnIndex = MatchComponent ? MatchComponent->GetMatchSpawnIndex() : INDEX_NONE;
	if (AActor* TaggedPlayerStart =
		FindPlayerStartByMatchSpawnIndex(
			SpawnIndex,
			Settings.LobbySpawnPlayerStartTagPrefix))
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

void UMatchSpawnComponent::MarkPlayerStartUsed(
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
		AssignedPlayerStartsByController.FindOrAdd(
			TObjectKey<AController>(Player)) = PlayerStart;
	}
}

void UMatchSpawnComponent::RecordInitialSpawn(AController* PlayerController, const FTransform& InitialSpawnTransform)
{
	if (PlayerController)
	{
		// 같은 경기의 리스폰으로 처음 배정된 위치가 덮어써지지 않게 한다.
		InitialPlayerSpawnTransforms.FindOrAdd(TObjectKey<AController>(PlayerController), InitialSpawnTransform);
	}
}

void UMatchSpawnComponent::RequestPlayerRespawn(
	AController* PlayerController,
	APawn* DeadPawn)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode
		|| !GameMode->HasAuthority()
		|| !PlayerController
		|| !PlayerController->IsPlayerController())
	{
		return;
	}

	const UMatchFlowComponent* MatchFlow =
		GameMode->GetMatchFlowComponent();
	if (MatchFlow && MatchFlow->IsGameResultShown())
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
	const float RespawnDelay = GetPlayerRespawnDelay();
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

bool UMatchSpawnComponent::TryGetPlayerInitialSpawnTransform(
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

TArray<APlayerController*> UMatchSpawnComponent::ForceMovePlayersToInitialSpawns()
{
	TArray<APlayerController*> MovedPlayers;
	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	UWorld* World = GetWorld();
	if (!GameMode || !World)
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

void UMatchSpawnComponent::ClearRuntimeStateForController(
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

AActor* UMatchSpawnComponent::FindPlayerStartByMatchSpawnIndex(
	const int32 SpawnIndex,
	const FName PlayerStartTagPrefix) const
{
	if (SpawnIndex == INDEX_NONE)
	{
		return nullptr;
	}

	const FString Prefix = PlayerStartTagPrefix.IsNone()
		? FString(TEXT("Spawn_"))
		: PlayerStartTagPrefix.ToString();
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

AActor* UMatchSpawnComponent::FindFirstUnusedPlayerStart() const
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

bool UMatchSpawnComponent::IsPlayerStartUsed(
	const AActor* PlayerStart) const
{
	return PlayerStart
		&& UsedPlayerStarts.ContainsByPredicate(
			[PlayerStart](const TObjectPtr<AActor>& UsedPlayerStart)
			{
				return UsedPlayerStart.Get() == PlayerStart;
			});
}

void UMatchSpawnComponent::FinishPlayerRespawn(
	TWeakObjectPtr<AController> WeakPlayerController,
	TWeakObjectPtr<APawn> WeakDeadPawn)
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !GameMode->HasAuthority())
	{
		return;
	}

	AController* PlayerController = WeakPlayerController.Get();
	if (!PlayerController)
	{
		return;
	}

	PendingPlayerRespawnTimers.Remove(
		TObjectKey<AController>(PlayerController));

	const UMatchFlowComponent* MatchFlow =
		GameMode->GetMatchFlowComponent();
	if (MatchFlow && MatchFlow->IsGameResultShown())
	{
		if (APdPlayerController* PdPlayerController =
			Cast<APdPlayerController>(PlayerController))
		{
			PdPlayerController->Client_HideRespawnDelayCountdown();
		}
		return;
	}

	APawn* CurrentPawn = PlayerController->GetPawn();
	APawn* DeadPawn = WeakDeadPawn.Get();
	if (!DeadPawn)
	{
		DeadPawn = CurrentPawn;
	}

	if (APdPlayerState* PlayerState = PlayerController->GetPlayerState<APdPlayerState>())
	{
		if (UPdAbilitySystemComponent* AbilitySystem = Cast<UPdAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent()))
		{
			AbilitySystem->ResetRuntimeStateForRespawn();
		}
	}

	FTransform RespawnTransform;
	if (TryGetPlayerRespawnTransform(
		PlayerController,
		RespawnTransform))
	{
		APawn* RespawnPawn = CurrentPawn ? CurrentPawn : DeadPawn;
		if (!CurrentPawn && IsValid(RespawnPawn))
		{
			PlayerController->Possess(RespawnPawn);
		}

		if (IsValid(RespawnPawn))
		{
			if (ACharacterBase* RespawnedCharacter = Cast<ACharacterBase>(RespawnPawn))
			{
				RespawnedCharacter->ResetDeathStateForRespawnAtTransform(RespawnTransform);
			}
			else
			{
				RespawnPawn->SetActorTransform(RespawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
				RespawnPawn->ForceNetUpdate();
			}
			PlayerController->SetControlRotation(RespawnTransform.GetRotation().Rotator());
			if (APdPlayerController* PdPlayerController =
				Cast<APdPlayerController>(PlayerController))
			{
				PdPlayerController
					->Client_ResetRespawnedPawnStateAtTransform(RespawnPawn, RespawnTransform);
				PdPlayerController
					->Client_HideRespawnDelayCountdown();
			}
			return;
		}

		GameMode->RestartPlayerAtTransform(
			PlayerController,
			RespawnTransform);
		PlayerController->SetControlRotation(
			RespawnTransform.GetRotation().Rotator());
		if (ACharacterBase* RespawnedCharacter =
			Cast<ACharacterBase>(PlayerController->GetPawn()))
		{
			RespawnedCharacter->ResetDeathStateForRespawnAtTransform(RespawnTransform);
		}
		if (APdPlayerController* PdPlayerController =
			Cast<APdPlayerController>(PlayerController))
		{
			PdPlayerController
				->Client_ResetRespawnedPawnStateAtTransform(PlayerController->GetPawn(), RespawnTransform);
			PdPlayerController->Client_HideRespawnDelayCountdown();
		}
		return;
	}

	APawn* FallbackRespawnPawn = CurrentPawn ? CurrentPawn : DeadPawn;
	if (!CurrentPawn && IsValid(FallbackRespawnPawn))
	{
		PlayerController->Possess(FallbackRespawnPawn);
	}

	if (IsValid(FallbackRespawnPawn))
	{
		const FTransform FallbackRespawnTransform =
			FallbackRespawnPawn->GetActorTransform();
		PlayerController->SetControlRotation(
			FallbackRespawnTransform.GetRotation().Rotator());
		if (ACharacterBase* RespawnedCharacter =
			Cast<ACharacterBase>(FallbackRespawnPawn))
		{
			RespawnedCharacter->ResetDeathStateForRespawnAtTransform(FallbackRespawnTransform);
		}
		else
		{
			FallbackRespawnPawn->ForceNetUpdate();
		}
		if (APdPlayerController* PdPlayerController =
			Cast<APdPlayerController>(PlayerController))
		{
			PdPlayerController
				->Client_ResetRespawnedPawnStateAtTransform(FallbackRespawnPawn, FallbackRespawnTransform);
			PdPlayerController->Client_HideRespawnDelayCountdown();
		}
		return;
	}

	if (APdPlayerController* PdPlayerController =
		Cast<APdPlayerController>(PlayerController))
	{
		PdPlayerController->Client_HideRespawnDelayCountdown();
	}
}

bool UMatchSpawnComponent::TryGetPlayerRespawnTransform(
	AController* PlayerController,
	FTransform& OutRespawnTransform)
{
	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const UMatchFlowComponent* MatchFlow =
		GameMode ? GameMode->GetMatchFlowComponent() : nullptr;
	if (MatchFlow && MatchFlow->IsGoldenKillActive())
	{
		return TryGetPlayerInitialSpawnTransform(
			PlayerController,
			OutRespawnTransform);
	}

	const UMatchRuleDefinition* MatchRules =
		MatchFlow ? MatchFlow->GetMatchRuleDefinition() : nullptr;
	if (MatchRules && MatchRules->bUseRandomPlayerStartRespawns)
	{
		if (AActor* RespawnPlayerStart =
			FindRandomRespawnPlayerStart(
				PlayerController,
				*MatchRules))
		{
			OutRespawnTransform =
				RespawnPlayerStart->GetActorTransform();
			if (PlayerController)
			{
				LastRandomRespawnPlayerStartNames.FindOrAdd(
					TObjectKey<AController>(PlayerController)) =
					RespawnPlayerStart->GetFName();
			}
			return true;
		}
	}

	return TryGetPlayerInitialSpawnTransform(
		PlayerController,
		OutRespawnTransform);
}

AActor* UMatchSpawnComponent::FindRandomRespawnPlayerStart(
	AController* PlayerController,
	const UMatchRuleDefinition& MatchRules) const
{
	UWorld* World = GetWorld();
	if (!World || MatchRules.RandomRespawnPlayerStartTags.IsEmpty())
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
				MatchRules))
		{
			Candidates.Add(PlayerStartActor);
		}
	}

	if (Candidates.IsEmpty())
	{
		return nullptr;
	}

	if (MatchRules.bAvoidLastRandomRespawnPlayerStart
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

bool UMatchSpawnComponent::DoesPlayerStartMatchRandomRespawnTags(
	const APlayerStart* PlayerStart,
	const UMatchRuleDefinition& MatchRules) const
{
	if (!PlayerStart)
	{
		return false;
	}

	for (const FName& RespawnTag :
		MatchRules.RandomRespawnPlayerStartTags)
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



float UMatchSpawnComponent::GetPlayerRespawnDelay() const
{
	const AExperienceGameMode* GameMode =
		GetExperienceGameModeConst();
	const UMatchFlowComponent* MatchFlow =
		GameMode ? GameMode->GetMatchFlowComponent() : nullptr;
	const UMatchRuleDefinition* MatchRules =
		MatchFlow ? MatchFlow->GetMatchRuleDefinition() : nullptr;
	return MatchRules
		? FMath::Max(MatchRules->PlayerRespawnDelay, 0.0f)
		: 0.0f;
}
