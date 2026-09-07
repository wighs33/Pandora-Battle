#include "AI/MonsterAIController.h"

#include "Component/Experience/ExperienceManagerComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Character/EnemyBase.h"
#include "AI/MonsterCharacter.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Definition/Character/EnemyBaseDefinition.h"
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

// 감지와 StateTree를 네이티브 컴포넌트로 만들고, 변경 가능한 감지 설정의 기본값만 지정한다.
AMonsterAIController::AMonsterAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NativeStateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("NativeStateTreeAI"));
	BrainComponent = NativeStateTreeAI;
	bStartAILogicOnPossess = false;

	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	NativeSightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("NativeSightConfig"));

	if (NativeStateTreeAI)
	{
		NativeStateTreeAI->SetStartLogicAutomatically(false);
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
	ConfigurePerception();
}

// BP에 저장된 감지 수치를 보존해 등록하고, 컴포넌트의 중복과 연결 상태를 한 번 검증한다.
void AMonsterAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	bStartAILogicOnPossess = false;

	if (NativeStateTreeAI)
	{
		BrainComponent = NativeStateTreeAI;
		NativeStateTreeAI->SetStartLogicAutomatically(false);
	}

	ConfigurePerception();
	bComponentConfigurationValid = ValidateComponentConfiguration();
}

#if WITH_EDITOR
// 에디터에서 감지·StateTree 컴포넌트가 중복되거나 잘못 연결된 구성을 찾는다.
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

	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

// 감지 이벤트를 연결하고 이미 준비된 몬스터의 AI 시작을 시도한다.
void AMonsterAIController::BeginPlay()
{
	Super::BeginPlay();

	if (!bComponentConfigurationValid)
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

// 월드 종료 후 감지 이벤트와 준비 완료 콜백이 남지 않도록 해제한다.
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

	StopMonsterAI();
	Super::EndPlay(EndPlayReason);
}

// 이전 몬스터의 행동과 기억을 정리하고 새 몬스터의 준비 상태부터 확인한다.
void AMonsterAIController::OnPossess(APawn* InPawn)
{
	StopMonsterAI();
	Super::OnPossess(InPawn);
	bAIStopped = false;
	bLoggedConfigurationError = false;
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(InPawn))
	{
		Enemy->SetUseNearestPlayerWhenTargetUnset(false);
	}
	if (!bComponentConfigurationValid)
	{
		return;
	}
	AIPerception->SetSenseEnabled(UAISense_Sight::StaticClass(), true);
	AIPerception->RequestStimuliListenerUpdate();
	RefreshPerceivedPlayerPawn();
	StartMonsterStateTreeIfReady();
}

// 조종하지 않는 몬스터를 계속 추적하거나 공격하지 않도록 AI를 종료한다.
void AMonsterAIController::OnUnPossess()
{
	StopMonsterAI();
	Super::OnUnPossess();
}

// StateTree에 현재 유효한 플레이어 표적만 제공한다.
APawn* AMonsterAIController::GetPerceivedPlayerPawn() const
{
	return IsValidPerceivedPlayerTarget(PerceivedPlayerPawn.Get())
		? PerceivedPlayerPawn.Get()
		: nullptr;
}

// 추적 유지 거리를 벗어난 표적을 잊고 다른 감지 대상을 찾는다.
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

// 기본값을 다시 덮지 않고 현재 SightConfig를 감지 컴포넌트에 등록한다.
void AMonsterAIController::ConfigurePerception()
{
	if (!AIPerception || !NativeSightConfig)
	{
		return;
	}

	AIPerception->ConfigureSense(*NativeSightConfig);
	SetPerceptionComponent(*AIPerception);
}

// 두 개의 감지·행동 컴포넌트가 동시에 작동할 수 있는 설정 오류를 수집한다.
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

// 플레이 시작 시 잘못된 컴포넌트 구성을 알리고 AI 실행을 막는다.
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

// 죽었거나 같은 팀인 대상 등 공격할 수 없는 플레이어를 제외한다.
bool AMonsterAIController::IsValidPerceivedPlayerTarget(APawn* PlayerPawn) const
{
	if (bAIStopped || !IsValid(PlayerPawn) || !PlayerPawn->IsPlayerControlled())
	{
		return false;
	}

	const AEnemyBase* Enemy = Cast<AEnemyBase>(GetPawn());
	return !Enemy || Enemy->IsActorValidAttackTarget(PlayerPawn);
}

// 몬스터의 평면 거리 기준으로 표적을 계속 기억할 수 있는지 판단한다.
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

// 감지 표적과 캐릭터의 실제 공격 대상을 함께 갱신한다.
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

// 현재 표적을 안정적으로 유지하고 필요할 때 가장 가까운 감지 플레이어로 교체한다.
void AMonsterAIController::RefreshPerceivedPlayerPawn(
	const AActor* ExcludedActor,
	APawn* NewlySensedPawn)
{
	if (bAIStopped || !AIPerception)
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
		// 새 감지 이벤트가 와도 유효한 현재 표적은 유지하고 캐릭터의 공격 대상과 동기화한다.
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

	// 현재 보이는 플레이어를 우선하고, 없을 때만 기억이 만료되지 않은 플레이어를 선택한다.
	APawn* NewTarget = SelectClosestTarget(CurrentlyPerceivedActors);
	if (!NewTarget)
	{
		NewTarget = SelectClosestTarget(KnownActors);
	}

	SetPerceivedPlayerPawn(NewTarget);
}

// 플레이어를 새로 보거나 놓친 결과를 표적 선택 정책에 반영한다.
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

	// 시야에서 사라진 표적은 기억 시간 동안 유지하되, 유지 거리 밖이면 즉시 해제한다.
	if (Actor == PerceivedPlayerPawn
		&& !IsWithinTargetRetentionDistance(PlayerPawn))
	{
		SetPerceivedPlayerPawn(nullptr);
		RefreshPerceivedPlayerPawn(Actor);
	}
}

// 감지 기억이 만료된 표적을 제거하고 남은 플레이어로 전환한다.
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

	// 다른 플레이어가 감지되어 있으면 새 이벤트를 기다리지 않고 전환한다.
	RefreshPerceivedPlayerPawn(Actor);
}

// 준비된 몬스터 전용 StateTree를 엔진의 BrainComponent에 연결한다.
bool AMonsterAIController::ConfigureStateTreeAI()
{
	if (!bComponentConfigurationValid)
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
					"%s has no Monster State Tree. Configure MonsterStateTree on the "
					"Enemy Base Definition before spawning monsters."),
				*GetPathName());
			bLoggedConfigurationError = true;
		}
		return false;
	}

	if (!NativeStateTreeAI->IsRunning())
	{
		NativeStateTreeAI->SetStateTree(ResolvedMonsterStateTree);
	}

	bStateTreeConfigured = true;
	return true;
}

// 현재 경기의 Experience가 준비되지 않았다면 완료 이벤트를 기다린다.
bool AMonsterAIController::IsExperienceReadyOrWait()
{
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
		return true;
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

// 전역 기본값이 아닌 현재 몬스터 정의에서 행동 자산을 비동기로 준비한다.
bool AMonsterAIController::ResolveMonsterStateTreeFromEnemyDefinition()
{
	if (ResolvedMonsterStateTree)
	{
		return true;
	}
	if (StateTreeLoadHandle)
	{
		return false;
	}
	const AMonsterCharacter* Monster = Cast<AMonsterCharacter>(GetPawn());
	const UEnemyBaseDefinition* Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	const TSoftObjectPtr<UStateTree> StateTreeReference = Definition ? Definition->GetMonsterStateTree() : nullptr;
	if (StateTreeReference.IsNull())
	{
		if (!bLoggedConfigurationError)
		{
			UE_LOG(LogMonsterAIController, Error, TEXT("%s requires a MonsterStateTree in its possessed monster's EnemyDefinition."),
				*GetPathName());
			bLoggedConfigurationError = true;
		}
		return false;
	}
	ResolvedMonsterStateTree = StateTreeReference.Get();
	if (ResolvedMonsterStateTree)
	{
		return true;
	}
	StateTreeLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
		StateTreeReference.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleMonsterStateTreeLoaded, StateTreeLoadGeneration),
		FStreamableManager::DefaultAsyncLoadPriority, false, true);
	if (StateTreeLoadHandle)
	{
		StateTreeLoadHandle->StartStalledHandle();
	}
	else if (!bLoggedConfigurationError)
	{
		UE_LOG(LogMonsterAIController, Error, TEXT("Could not request monster StateTree: %s."), *StateTreeReference.ToString());
		bLoggedConfigurationError = true;
	}
	return false;
}

// 조종 대상이 바뀌기 전에 요청한 로딩 완료는 무시하고, 현재 몬스터의 행동 자산만 연결한다.
void AMonsterAIController::HandleMonsterStateTreeLoaded(uint32 RequestGeneration)
{
	if (RequestGeneration != StateTreeLoadGeneration || bAIStopped)
	{
		return;
	}
	const AMonsterCharacter* Monster = Cast<AMonsterCharacter>(GetPawn());
	const UEnemyBaseDefinition* Definition = Monster ? Monster->GetMonsterDefinition() : nullptr;
	ResolvedMonsterStateTree = Definition ? Definition->GetMonsterStateTree().Get() : nullptr;
	if (!ResolvedMonsterStateTree)
	{
		UE_LOG(LogMonsterAIController, Error, TEXT("%s failed to load its monster StateTree."), *GetPathName());
		bLoggedConfigurationError = true;
		return;
	}
	StartMonsterStateTreeIfReady();
}

// 사망·조종 해제·월드 종료 시 판단과 이동을 멈추고, 지연 콜백이 AI를 다시 켜지 못하게 한다.
void AMonsterAIController::StopMonsterAI()
{
	bAIStopped = true;
	++StateTreeLoadGeneration;
	StopWaitingForExperience();
	StopWaitingForNavigationData();
	if (StateTreeLoadHandle)
	{
		StateTreeLoadHandle->CancelHandle();
		StateTreeLoadHandle.Reset();
	}
	if (NativeStateTreeAI && NativeStateTreeAI->IsRunning())
	{
		NativeStateTreeAI->StopLogic(TEXT("Monster AI stopped"));
	}
	if (NativeStateTreeAI)
	{
		NativeStateTreeAI->SetStateTree(nullptr);
	}
	if (AMonsterCharacter* Monster = Cast<AMonsterCharacter>(GetPawn()))
	{
		Monster->StopMonsterAttack();
	}
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	SetPerceivedPlayerPawn(nullptr);
	if (AIPerception)
	{
		AIPerception->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
		AIPerception->ForgetAll();
	}
	ResolvedMonsterStateTree = nullptr;
	bStateTreeConfigured = false;
	bLoggedNavigationError = false;
}

// 더 이상 사용할 수 없는 경기 준비 알림을 해제한다.
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

// 경기 준비가 끝나면 남은 AI 시작 조건을 다시 확인한다.
void AMonsterAIController::HandleExperienceLoaded(const UExperienceDefinition* Experience)
{
	static_cast<void>(Experience);
	ExperienceManagerWaitingForLoad.Reset();
	ExperienceLoadedDelegateHandle.Reset();

	bStateTreeConfigured = false;

	StartMonsterStateTreeIfReady();
}

// 조종 중인 몬스터의 이동 규격에 맞는 내비게이션 데이터가 있는지 확인한다.
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

// 내비게이션 등록·생성 완료 이벤트를 기다린다.
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

// 내비게이션이 준비됐거나 AI가 끝났을 때 대기 이벤트를 해제한다.
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

// 내비게이션 데이터가 추가되면 AI 시작을 다시 시도한다.
void AMonsterAIController::HandleNavigationDataAvailable(ANavigationData* NavigationData)
{
	if (NavigationData)
	{
		StartMonsterStateTreeIfReady();
	}
}

// 몬스터·경기·행동 자산·내비게이션이 모두 준비된 서버에서만 행동을 시작한다.
void AMonsterAIController::StartMonsterStateTreeIfReady()
{
	const AMonsterCharacter* Monster = Cast<AMonsterCharacter>(GetPawn());
	if (bAIStopped || !HasAuthority() || (!HasActorBegunPlay() && !IsActorBeginningPlay()) || !bComponentConfigurationValid
		|| !IsValid(Monster) || !Monster->IsMonsterReadyForAI())
	{
		return;
	}

	if (!IsExperienceReadyOrWait())
	{
		return;
	}

	if (!ResolveMonsterStateTreeFromEnemyDefinition())
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
				Warning,
				TEXT(
					"%s is waiting for compatible NavData at %s. "
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
