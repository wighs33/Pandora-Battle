#include "AI/MonsterAIController.h"

#include "Component/Experience/ExperienceManagerComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Character/EnemyBase.h"
#include "Definition/Experience/ExperienceDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Mode/ExperienceGameState.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "StateTree.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MonsterAIController)

DEFINE_LOG_CATEGORY_STATIC(LogMonsterAIController, Log, All);

AMonsterAIController::AMonsterAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NativeStateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("NativeStateTreeAI"));
	BrainComponent = NativeStateTreeAI;

	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	NativeSightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("NativeSightConfig"));

	if (NativeStateTreeAI)
	{
		NativeStateTreeAI->SetStartLogicAutomatically(false);
	}

	ConfigurePerception();
}

void AMonsterAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (NativeStateTreeAI)
	{
		BrainComponent = NativeStateTreeAI;
		NativeStateTreeAI->SetStartLogicAutomatically(false);
	}

	ConfigurePerception();
	ValidateComponentConfiguration();
}

#if WITH_EDITOR
EDataValidationResult AMonsterAIController::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Errors;
	GatherComponentConfigurationErrors(Errors);
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}

	if (!Errors.IsEmpty())
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

void AMonsterAIController::BeginPlay()
{
	Super::BeginPlay();

	if (!ValidateComponentConfiguration())
	{
		return;
	}

	if (AIPerception)
	{
		AIPerception->OnTargetPerceptionUpdated.AddUniqueDynamic(
			this,
			&AMonsterAIController::HandleTargetPerceptionUpdated);
		AIPerception->OnTargetPerceptionForgotten.AddUniqueDynamic(
			this,
			&AMonsterAIController::HandleTargetPerceptionForgotten);
		RefreshPerceivedPlayerPawn();
	}

	StartMonsterStateTreeIfReady();
}

void AMonsterAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AIPerception)
	{
		AIPerception->OnTargetPerceptionUpdated.RemoveDynamic(
			this,
			&AMonsterAIController::HandleTargetPerceptionUpdated);
		AIPerception->OnTargetPerceptionForgotten.RemoveDynamic(
			this,
			&AMonsterAIController::HandleTargetPerceptionForgotten);
	}

	SetPerceivedPlayerPawn(nullptr);
	StopWaitingForExperience();
	StopWaitingForNavigationData();
	Super::EndPlay(EndPlayReason);
}

void AMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	SetPerceivedPlayerPawn(nullptr);
	if (!ValidateComponentConfiguration())
	{
		if (AEnemyBase* Enemy = Cast<AEnemyBase>(InPawn))
		{
			Enemy->SetUseNearestPlayerWhenTargetUnset(true);
		}
		return;
	}

	if (AEnemyBase* Enemy = Cast<AEnemyBase>(InPawn))
	{
		// The StateTree publishes the authoritative combat target. Do not let
		// EnemyBase independently pick an unseen player in multiplayer.
		Enemy->SetUseNearestPlayerWhenTargetUnset(false);
	}

	if (AIPerception)
	{
		AIPerception->RequestStimuliListenerUpdate();
		RefreshPerceivedPlayerPawn();
	}

	if (HasActorBegunPlay())
	{
		StartMonsterStateTreeIfReady();
	}
}

void AMonsterAIController::OnUnPossess()
{
	if (NativeStateTreeAI && NativeStateTreeAI->IsRunning())
	{
		NativeStateTreeAI->StopLogic(TEXT("Monster controller no longer possesses a pawn"));
	}

	SetPerceivedPlayerPawn(nullptr);
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetPawn()))
	{
		Enemy->SetUseNearestPlayerWhenTargetUnset(true);
	}

	Super::OnUnPossess();
}

APawn* AMonsterAIController::GetPerceivedPlayerPawn() const
{
	return IsValidPerceivedPlayerTarget(PerceivedPlayerPawn.Get())
		? PerceivedPlayerPawn.Get()
		: nullptr;
}

bool AMonsterAIController::ForgetPerceivedPlayerIfOutOfRange()
{
	APawn* TargetPawn = GetPerceivedPlayerPawn();
	if (!TargetPawn || IsWithinTargetRetentionDistance(TargetPawn))
	{
		return false;
	}

	SetPerceivedPlayerPawn(nullptr);
	if (AIPerception)
	{
		AIPerception->ForgetActor(TargetPawn);
	}

	RefreshPerceivedPlayerPawn(TargetPawn);
	return true;
}

void AMonsterAIController::ConfigurePerception()
{
	if (!AIPerception || !NativeSightConfig)
	{
		return;
	}

	NativeSightConfig->SightRadius = 1000.0f;
	NativeSightConfig->LoseSightRadius = 2500.0f;
	NativeSightConfig->PeripheralVisionAngleDegrees = 90.0f;
	NativeSightConfig->AutoSuccessRangeFromLastSeenLocation = 500.0f;
	NativeSightConfig->SetMaxAge(1.0f);
	NativeSightConfig->SetStartsEnabled(true);
	NativeSightConfig->DetectionByAffiliation.bDetectEnemies = true;
	NativeSightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	NativeSightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	AIPerception->ConfigureSense(*NativeSightConfig);
	SetPerceptionComponent(*AIPerception);
}

void AMonsterAIController::GatherComponentConfigurationErrors(
	TArray<FText>& OutErrors) const
{
	TArray<UAIPerceptionComponent*> PerceptionComponents;
	GetComponents<UAIPerceptionComponent>(PerceptionComponents);

	if (!IsValid(AIPerception)
		|| PerceptionComponents.Num() != 1
		|| !PerceptionComponents.Contains(AIPerception))
	{
		OutErrors.Add(FText::Format(
			NSLOCTEXT(
				"MonsterAIController",
				"InvalidPerceptionComponentCount",
				"{0} must contain exactly one AIPerceptionComponent: the inherited "
				"AIPerception component. Found {1}. Remove Blueprint-added perception components."),
			FText::FromString(GetPathName()),
			FText::AsNumber(PerceptionComponents.Num())));
	}

	if (!IsValid(NativeSightConfig))
	{
		OutErrors.Add(FText::Format(
			NSLOCTEXT(
				"MonsterAIController",
				"MissingNativeSightConfig",
				"{0} is missing its inherited NativeSightConfig subobject."),
			FText::FromString(GetPathName())));
	}

	if (GetAIPerceptionComponent() != AIPerception)
	{
		OutErrors.Add(FText::Format(
			NSLOCTEXT(
				"MonsterAIController",
				"InvalidPerceptionComponentRoute",
				"{0} must use its inherited AIPerception component as the controller "
				"perception component."),
			FText::FromString(GetPathName())));
	}

	TArray<UStateTreeAIComponent*> StateTreeComponents;
	GetComponents<UStateTreeAIComponent>(StateTreeComponents);

	if (!IsValid(NativeStateTreeAI)
		|| StateTreeComponents.Num() != 1
		|| !StateTreeComponents.Contains(NativeStateTreeAI))
	{
		OutErrors.Add(FText::Format(
			NSLOCTEXT(
				"MonsterAIController",
				"InvalidStateTreeComponentCount",
				"{0} must contain exactly one StateTreeAIComponent: the inherited "
				"NativeStateTreeAI component. Found {1}. Remove Blueprint-added StateTree components."),
			FText::FromString(GetPathName()),
			FText::AsNumber(StateTreeComponents.Num())));
	}

	if (BrainComponent != NativeStateTreeAI)
	{
		OutErrors.Add(FText::Format(
			NSLOCTEXT(
				"MonsterAIController",
				"InvalidBrainComponentRoute",
				"{0} must use its inherited NativeStateTreeAI component as BrainComponent."),
			FText::FromString(GetPathName())));
	}
}

bool AMonsterAIController::ValidateComponentConfiguration()
{
	TArray<FText> Errors;
	GatherComponentConfigurationErrors(Errors);
	if (Errors.IsEmpty())
	{
		bLoggedComponentConfigurationError = false;
		return true;
	}

	if (!bLoggedComponentConfigurationError)
	{
		for (const FText& Error : Errors)
		{
			UE_LOG(
				LogMonsterAIController,
				Error,
				TEXT("%s"),
				*Error.ToString());
		}
		bLoggedComponentConfigurationError = true;
	}

	return false;
}

bool AMonsterAIController::IsValidPerceivedPlayerTarget(APawn* PlayerPawn) const
{
	if (!IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
	{
		return false;
	}

	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetPawn());
	return !Enemy || Enemy->IsActorValidAttackTarget(PlayerPawn);
}

bool AMonsterAIController::IsWithinTargetRetentionDistance(
	const AActor* TargetActor) const
{
	const APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn) || !IsValid(TargetActor) || !NativeSightConfig)
	{
		return false;
	}

	const double RetentionDistance =
		FMath::Max(static_cast<double>(NativeSightConfig->LoseSightRadius), 0.0);
	if (RetentionDistance <= 0.0)
	{
		return true;
	}

	return FVector::DistSquared2D(
			   ControlledPawn->GetActorLocation(),
			   TargetActor->GetActorLocation())
		<= FMath::Square(RetentionDistance);
}

void AMonsterAIController::SetPerceivedPlayerPawn(APawn* PlayerPawn)
{
	APawn* ValidTarget = IsValidPerceivedPlayerTarget(PlayerPawn)
		? PlayerPawn
		: nullptr;

	if (PerceivedPlayerPawn != ValidTarget)
	{
		UE_LOG(
			LogMonsterAIController,
			Verbose,
			TEXT("%s changed target from %s to %s."),
			*GetNameSafe(this),
			*GetNameSafe(PerceivedPlayerPawn),
			*GetNameSafe(ValidTarget));
	}

	PerceivedPlayerPawn = ValidTarget;
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetPawn()))
	{
		Enemy->SetAttackTarget(ValidTarget);
	}
}

void AMonsterAIController::RefreshPerceivedPlayerPawn(
	const AActor* ExcludedActor,
	APawn* NewlySensedPawn)
{
	if (!AIPerception)
	{
		SetPerceivedPlayerPawn(nullptr);
		return;
	}

	const APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn))
	{
		SetPerceivedPlayerPawn(nullptr);
		return;
	}

	TArray<AActor*> KnownActors;
	AIPerception->GetKnownPerceivedActors(
		UAISense_Sight::StaticClass(),
		KnownActors);

	TArray<AActor*> CurrentlyPerceivedActors;
	AIPerception->GetCurrentlyPerceivedActors(
		UAISense_Sight::StaticClass(),
		CurrentlyPerceivedActors);

	if (IsValidPerceivedPlayerTarget(NewlySensedPawn)
		&& NewlySensedPawn != ExcludedActor)
	{
		CurrentlyPerceivedActors.AddUnique(NewlySensedPawn);
		KnownActors.AddUnique(NewlySensedPawn);
	}

	const auto IsSelectableTarget =
		[this, ExcludedActor](AActor* CandidateActor)
		{
			APawn* CandidatePawn = Cast<APawn>(CandidateActor);
			return CandidateActor != ExcludedActor
				&& IsValidPerceivedPlayerTarget(CandidatePawn)
				&& IsWithinTargetRetentionDistance(CandidatePawn);
		};

	APawn* CurrentTarget = PerceivedPlayerPawn.Get();
	const bool bCurrentTargetIsStillKnown =
		KnownActors.Contains(CurrentTarget)
		|| CurrentlyPerceivedActors.Contains(CurrentTarget);
	if (bCurrentTargetIsStillKnown && IsSelectableTarget(CurrentTarget))
	{
		// Keep a valid combat target stable even when another player generates
		// a perception update. This also repairs any external attack-target
		// overwrite by synchronizing EnemyBase again.
		SetPerceivedPlayerPawn(CurrentTarget);
		return;
	}

	AEnemyBase* Enemy = Cast<AEnemyBase>(GetPawn());
	APawn* CachedCombatTarget =
		Enemy ? Cast<APawn>(Enemy->GetCachedAttackTarget()) : nullptr;
	const bool bCachedTargetIsPerceived =
		KnownActors.Contains(CachedCombatTarget)
		|| CurrentlyPerceivedActors.Contains(CachedCombatTarget);
	if (bCachedTargetIsPerceived && IsSelectableTarget(CachedCombatTarget))
	{
		SetPerceivedPlayerPawn(CachedCombatTarget);
		return;
	}

	const FVector SelectionOrigin = ControlledPawn->GetActorLocation();
	const auto SelectClosestTarget =
		[&IsSelectableTarget, &SelectionOrigin](const TArray<AActor*>& Candidates)
		{
			APawn* ClosestTarget = nullptr;
			double ClosestDistanceSquared = TNumericLimits<double>::Max();

			for (AActor* CandidateActor : Candidates)
			{
				if (!IsSelectableTarget(CandidateActor))
				{
					continue;
				}

				APawn* CandidatePawn = CastChecked<APawn>(CandidateActor);
				const double CandidateDistanceSquared = FVector::DistSquared2D(
					SelectionOrigin,
					CandidatePawn->GetActorLocation());
				const bool bSameDistance =
					CandidateDistanceSquared == ClosestDistanceSquared;
				const bool bStableTieBreak =
					bSameDistance
					&& (!ClosestTarget
						|| CandidatePawn->GetPathName()
							< ClosestTarget->GetPathName());

				if (CandidateDistanceSquared < ClosestDistanceSquared
					|| bStableTieBreak)
				{
					ClosestTarget = CandidatePawn;
					ClosestDistanceSquared = CandidateDistanceSquared;
				}
			}

			return ClosestTarget;
		};

	// Prefer a player that is visible now. Remembered sight stimuli are only a
	// fallback for the short MaxAge grace period.
	APawn* NewTarget = SelectClosestTarget(CurrentlyPerceivedActors);
	if (!NewTarget)
	{
		NewTarget = SelectClosestTarget(KnownActors);
	}

	SetPerceivedPlayerPawn(NewTarget);
}

void AMonsterAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	APawn* PlayerPawn = Cast<APawn>(Actor);
	if (!IsValidPerceivedPlayerTarget(PlayerPawn))
	{
		if (Actor == PerceivedPlayerPawn)
		{
			SetPerceivedPlayerPawn(nullptr);
			RefreshPerceivedPlayerPawn(Actor);
		}
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		RefreshPerceivedPlayerPawn(nullptr, PlayerPawn);
		return;
	}

	// Preserve the perception MaxAge grace period after losing line of sight,
	// but never retain a target that has crossed the configured lose-sight
	// radius.
	if (Actor == PerceivedPlayerPawn
		&& !IsWithinTargetRetentionDistance(PlayerPawn))
	{
		SetPerceivedPlayerPawn(nullptr);
		RefreshPerceivedPlayerPawn(Actor);
	}
}

void AMonsterAIController::HandleTargetPerceptionForgotten(AActor* Actor)
{
	APawn* ForgottenPawn = Cast<APawn>(Actor);
	if (!ForgottenPawn || !ForgottenPawn->IsPlayerControlled())
	{
		return;
	}

	if (Actor == PerceivedPlayerPawn)
	{
		SetPerceivedPlayerPawn(nullptr);
	}

	// If another player remains perceived, switch immediately instead of
	// waiting for that actor to generate a fresh perception update.
	RefreshPerceivedPlayerPawn(Actor);
}

bool AMonsterAIController::ConfigureStateTreeAI()
{
	if (!ValidateComponentConfiguration())
	{
		return false;
	}

	if (!IsValid(ResolvedMonsterStateTree))
	{
		if (!bLoggedConfigurationError)
		{
			UE_LOG(
				LogMonsterAIController,
				Error,
				TEXT(
					"%s has no Monster State Tree. Configure MonsterStateTree on the active "
					"Experience before spawning monsters."),
				*GetPathName());
			bLoggedConfigurationError = true;
		}
		return false;
	}

	BrainComponent = NativeStateTreeAI;
	NativeStateTreeAI->SetStartLogicAutomatically(false);
	if (!NativeStateTreeAI->IsRunning())
	{
		NativeStateTreeAI->SetStateTree(ResolvedMonsterStateTree);
	}

	bStateTreeConfigured = true;
	return true;
}

bool AMonsterAIController::ResolveMonsterStateTreeFromExperience()
{
	if (IsValid(ResolvedMonsterStateTree))
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const AExperienceGameState* ExperienceGameState =
		World ? World->GetGameState<AExperienceGameState>() : nullptr;
	UExperienceManagerComponent* ExperienceManager =
		ExperienceGameState ? ExperienceGameState->GetExperienceManagerComponent() : nullptr;
	if (!ExperienceManager)
	{
		if (!bLoggedConfigurationError)
		{
			UE_LOG(
				LogMonsterAIController,
				Error,
				TEXT(
					"%s cannot resolve monster AI because the current GameState has no "
					"ExperienceManagerComponent."),
				*GetPathName());
			bLoggedConfigurationError = true;
		}
		return false;
	}

	if (ExperienceManager->IsExperienceLoaded())
	{
		const UExperienceDefinition* Experience =
			ExperienceManager->GetCurrentExperienceChecked();
		ResolvedMonsterStateTree = Experience ? Experience->MonsterStateTree : nullptr;
		return IsValid(ResolvedMonsterStateTree);
	}

	if (!ExperienceLoadedDelegateHandle.IsValid())
	{
		ExperienceManagerWaitingForLoad = ExperienceManager;
		ExperienceLoadedDelegateHandle =
			ExperienceManager->CallOrRegister_OnExperienceLoaded(
				FOnPdExperienceLoaded::FDelegate::CreateUObject(
					this,
					&AMonsterAIController::HandleExperienceLoaded));
	}

	return false;
}

void AMonsterAIController::StopWaitingForExperience()
{
	if (UExperienceManagerComponent* ExperienceManager =
			ExperienceManagerWaitingForLoad.Get())
	{
		ExperienceManager->RemoveOnExperienceLoaded(ExperienceLoadedDelegateHandle);
	}

	ExperienceManagerWaitingForLoad.Reset();
	ExperienceLoadedDelegateHandle.Reset();
}

void AMonsterAIController::HandleExperienceLoaded(const UExperienceDefinition* Experience)
{
	ExperienceManagerWaitingForLoad.Reset();
	ExperienceLoadedDelegateHandle.Reset();

	ResolvedMonsterStateTree = Experience ? Experience->MonsterStateTree : nullptr;
	bStateTreeConfigured = false;

	if (!IsValid(ResolvedMonsterStateTree))
	{
		if (!bLoggedConfigurationError)
		{
			UE_LOG(
				LogMonsterAIController,
				Error,
				TEXT(
					"%s cannot start monster AI because the loaded Experience has no "
					"MonsterStateTree."),
				*GetPathName());
			bLoggedConfigurationError = true;
		}
		return;
	}

	StartMonsterStateTreeIfReady();
}

bool AMonsterAIController::HasRequiredNavigationData() const
{
	const UWorld* World = GetWorld();
	const APawn* ControlledPawn = GetPawn();
	if (!World || !ControlledPawn)
	{
		return false;
	}

	const UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavigationSystem)
	{
		return false;
	}

	return NavigationSystem->GetNavDataForProps(
			   ControlledPawn->GetNavAgentPropertiesRef(),
			   ControlledPawn->GetActorLocation())
		!= nullptr;
}

void AMonsterAIController::WaitForNavigationData()
{
	if (bWaitingForNavigationData)
	{
		return;
	}

	if (UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavigationSystem->OnNavDataRegisteredEvent.AddUniqueDynamic(
			this,
			&AMonsterAIController::HandleNavigationDataAvailable);
		NavigationSystem->OnNavigationGenerationFinishedDelegate.AddUniqueDynamic(
			this,
			&AMonsterAIController::HandleNavigationDataAvailable);
		bWaitingForNavigationData = true;
	}
}

void AMonsterAIController::StopWaitingForNavigationData()
{
	if (!bWaitingForNavigationData)
	{
		return;
	}

	if (UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavigationSystem->OnNavDataRegisteredEvent.RemoveDynamic(
			this,
			&AMonsterAIController::HandleNavigationDataAvailable);
		NavigationSystem->OnNavigationGenerationFinishedDelegate.RemoveDynamic(
			this,
			&AMonsterAIController::HandleNavigationDataAvailable);
	}

	bWaitingForNavigationData = false;
}

void AMonsterAIController::HandleNavigationDataAvailable(ANavigationData* NavigationData)
{
	if (NavigationData)
	{
		StartMonsterStateTreeIfReady();
	}
}

void AMonsterAIController::StartMonsterStateTreeIfReady()
{
	if (!GetPawn())
	{
		return;
	}

	if (!ResolveMonsterStateTreeFromExperience())
	{
		return;
	}

	if (!bStateTreeConfigured && !ConfigureStateTreeAI())
	{
		return;
	}

	if (!HasRequiredNavigationData())
	{
		if (!bLoggedNavigationError)
		{
			UE_LOG(
				LogMonsterAIController,
				Error,
				TEXT(
					"%s cannot start monster AI because no compatible NavData exists at %s. "
					"The map or active Experience must provide a NavMeshBoundsVolume and built navigation data."),
				*GetPathName(),
				*GetPawn()->GetActorLocation().ToCompactString());
			bLoggedNavigationError = true;
		}

		WaitForNavigationData();
		return;
	}

	StopWaitingForNavigationData();
	bLoggedNavigationError = false;

	if (!NativeStateTreeAI->IsRunning())
	{
		NativeStateTreeAI->StartLogic();
	}
}
