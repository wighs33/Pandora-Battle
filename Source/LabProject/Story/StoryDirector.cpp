#include "Story/StoryDirector.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Story/StoryChapterDefinition.h"
#include "Story/StoryCharacter.h"
#include "Story/StoryFloatingProp.h"
#include "Story/StoryMark.h"
#include "Story/StoryPortal.h"
#include "Story/StoryStep.h"
#include "UI/Story/StoryScreen.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryDirector)

DEFINE_LOG_CATEGORY_STATIC(LogStory, Log, All);

namespace
{
	/** 손에 든 카메라처럼 아주 조금 흔들리는 폭(cm)과 빠르기 */
	constexpr float HandheldDistance = 1.6f;
	constexpr float HandheldSpeed = 0.35f;
	constexpr float ShakeSpeed = 26.f;
	constexpr float PlayerCameraBlendSeconds = 0.8f;
	constexpr float PlayerCameraPitch = -12.f;
	constexpr float GoalMarkerHeight = 130.f;
	constexpr float GoalMarkerScale = 1.3f;
	constexpr float GoalMarkerBob = 14.f;
	constexpr float GoalMarkerBobSeconds = 1.6f;
	constexpr float GoalMarkerSpin = 90.f;
	/** 건너뛴 뒤 암전하는 시간 */
	constexpr float SkipFadeSeconds = 0.6f;
}

AStoryDirector::AStoryDirector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	SetRootComponent(Camera);
	Camera->bConstrainAspectRatio = false;
}

AStoryDirector::~AStoryDirector() = default;

AStoryDirector* AStoryDirector::Find(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	TActorIterator<AStoryDirector> It(World);
	return World && It ? *It : nullptr;
}

void AStoryDirector::BeginChapter(UStoryScreen& InScreen)
{
	if (bPlaying || !Chapter)
	{
		UE_CLOG(!Chapter, LogStory, Error, TEXT("%s has no chapter."), *GetName());
		return;
	}
	Screen = &InScreen;
	SpawnCast();
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		PlayerController->SetViewTarget(this);
	}
	bPlaying = true;
	StepIndex = INDEX_NONE;
	PlayShot(FStoryShot());
}

void AStoryDirector::SpawnCast()
{
	for (const FStoryCastMember& Member : Chapter->Cast)
	{
		if (!ensure(Member.Character))
		{
			continue;
		}
		const FTransform Mark = GetMark(Member.StartMark);
		AStoryCharacter* Character = GetWorld()->SpawnActorDeferred<AStoryCharacter>(AStoryCharacter::StaticClass(), Mark, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Character->FinishSpawning(Mark);
		Character->Initialize(*Member.Character, Chapter->EmoteMeshes);
		Character->PlaceAt(Mark.GetLocation(), Mark.Rotator().Yaw);
		Character->SetPlayerControlled(false);
		Characters.Add(Member.Id, Character);
	}
}

void AStoryDirector::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPlaying)
	{
		TickSteps(DeltaSeconds);
	}
	TickCamera(DeltaSeconds);
}

void AStoryDirector::TickSteps(const float DeltaSeconds)
{
	BackgroundTasks.RemoveAll([this, DeltaSeconds](const TUniquePtr<FStoryTask>& Task) { return Task->Tick(*this, DeltaSeconds); });
	if (CurrentTask && !CurrentTask->Tick(*this, DeltaSeconds))
	{
		return;
	}
	CurrentTask.Reset();

	// 바로 끝나는 단계는 같은 프레임에 이어서 시작한다.
	while (bPlaying && !CurrentTask)
	{
		++StepIndex;
		if (!Chapter->Steps.IsValidIndex(StepIndex))
		{
			FinishChapter();
			return;
		}
		const UStoryStep* Step = Chapter->Steps[StepIndex];
		if (!ensure(Step))
		{
			continue;
		}
		TUniquePtr<FStoryTask> Task = Step->Start(*this);
		if (Task && Step->bWait)
		{
			CurrentTask = MoveTemp(Task);
		}
		else if (Task)
		{
			BackgroundTasks.Add(MoveTemp(Task));
		}
	}
}

void AStoryDirector::SkipChapter()
{
	if (!bPlaying)
	{
		return;
	}
	EndPlayerControl();
	BackgroundTasks.Reset();
	Screen->HideLine();
	Screen->EndMash();
	Screen->FadeTo(FLinearColor::Black, 1.f, SkipFadeSeconds);
	StepIndex = Chapter->Steps.Num();
	CurrentTask = MakeUnique<FStoryWaitTask>(SkipFadeSeconds);
}

void AStoryDirector::FinishChapter()
{
	bPlaying = false;
	ReturnToTitle();
}

void AStoryDirector::ReturnToTitle()
{
	const ULevelDefinition* Levels = ULevelDefinition::ResolveDefaultDefinition();
	const FString TitleMapName = Levels ? Levels->GetTitleTravelMapName() : FString();
	if (!TitleMapName.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*TitleMapName));
	}
}

AStoryCharacter* AStoryDirector::FindCharacter(const FName Id) const
{
	const TObjectPtr<AStoryCharacter>* Character = Characters.Find(Id);
	UE_CLOG(!Character, LogStory, Warning, TEXT("Story character %s is not in the cast."), *Id.ToString());
	return Character ? Character->Get() : nullptr;
}

AStoryCharacter* AStoryDirector::GetPlayerCharacter() const
{
	return Chapter ? FindCharacter(Chapter->PlayerCharacterId) : nullptr;
}

FTransform AStoryDirector::GetMark(const FName MarkId) const
{
	for (TActorIterator<AStoryMark> It(GetWorld()); It; ++It)
	{
		if (It->MarkId == MarkId)
		{
			return It->GetActorTransform();
		}
	}
	UE_LOG(LogStory, Warning, TEXT("Story mark %s is not in the level."), *MarkId.ToString());
	return FTransform::Identity;
}

void AStoryDirector::SetSpeaker(const FName Id)
{
	if (Id != CurrentSpeaker)
	{
		PreviousSpeaker = CurrentSpeaker;
		CurrentSpeaker = Id;
	}
	const AStoryCharacter* Speaker = FindCharacter(Id);
	const FName ListenerId = !PreviousSpeaker.IsNone() && Id != Chapter->PlayerCharacterId ? Chapter->PlayerCharacterId : PreviousSpeaker;
	for (const TPair<FName, TObjectPtr<AStoryCharacter>>& Pair : Characters)
	{
		const bool bSpeaks = Pair.Key == Id;
		const TObjectPtr<AStoryCharacter>* Listener = Characters.Find(ListenerId);
		Pair.Value->LookAt(bSpeaks ? (Listener ? Listener->Get() : nullptr) : Speaker);
	}
}

void AStoryDirector::PlayShot(const FStoryShot& Shot)
{
	BlendFrom = Camera->GetComponentTransform();
	BlendFromFov = Camera->FieldOfView;
	ActiveShot = Shot;
	ShotElapsed = 0.f;
	ShotPivot = GetShotPivot(Shot);
	const AStoryCharacter* Subject = Shot.Subject.IsNone() ? nullptr : FindCharacter(Shot.Subject);
	ShotYaw = Subject && !Shot.bStageAxes ? Subject->GetFacingYaw() : 0.f;
}

FVector AStoryDirector::GetShotPivot(const FStoryShot& Shot) const
{
	const AStoryCharacter* Subject = Shot.Subject.IsNone() ? nullptr : FindCharacter(Shot.Subject);
	if (!Subject)
	{
		return FVector::ZeroVector;
	}
	const AStoryCharacter* Partner = Shot.Partner.IsNone() ? nullptr : FindCharacter(Shot.Partner);
	return Partner ? (Subject->GetHeadLocation() + Partner->GetHeadLocation()) * 0.5 : Subject->GetHeadLocation();
}

FTransform AStoryDirector::ComputeShotTransform(const FStoryShot& Shot, const float ShotSeconds) const
{
	const FVector Pivot = Shot.bFollow ? GetShotPivot(Shot) : ShotPivot;
	const FRotator Axes(0.f, ShotYaw, 0.f);
	const FVector Eye = Pivot + Axes.RotateVector(Shot.CameraOffset + Shot.Drift * ShotSeconds);
	const FVector Aim = Pivot + Axes.RotateVector(Shot.AimOffset);
	return FTransform((Aim - Eye).Rotation(), Eye);
}

void AStoryDirector::TickCamera(const float DeltaSeconds)
{
	ShotElapsed += DeltaSeconds;
	CameraTime += DeltaSeconds;
	ShakeElapsed += DeltaSeconds;

	FTransform Target = ComputeShotTransform(ActiveShot, ShotElapsed);
	float FieldOfView = ActiveShot.FieldOfView;
	if (ShotElapsed < ActiveShot.BlendSeconds)
	{
		const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, ShotElapsed / ActiveShot.BlendSeconds, 2.2f);
		Target.SetLocation(FMath::Lerp(BlendFrom.GetLocation(), Target.GetLocation(), Alpha));
		Target.SetRotation(FQuat::Slerp(BlendFrom.GetRotation(), Target.GetRotation(), Alpha));
		FieldOfView = FMath::Lerp(BlendFromFov, FieldOfView, Alpha);
	}

	const float Handheld = CameraTime * HandheldSpeed;
	FVector Sway(0.f, FMath::PerlinNoise1D(Handheld) * HandheldDistance, FMath::PerlinNoise1D(Handheld + 31.7f) * HandheldDistance);
	if (ShakeElapsed < ShakeSeconds)
	{
		const float Amount = ShakeStrength * FMath::Square(1.f - ShakeElapsed / ShakeSeconds);
		Sway.Y += FMath::PerlinNoise1D(CameraTime * ShakeSpeed) * Amount;
		Sway.Z += FMath::PerlinNoise1D(CameraTime * ShakeSpeed + 57.3f) * Amount;
	}
	Target.AddToTranslation(Target.GetRotation().RotateVector(Sway));
	Camera->SetWorldTransform(Target);
	Camera->SetFieldOfView(FieldOfView);
}

void AStoryDirector::Shake(const float Strength, const float Seconds)
{
	ShakeStrength = Strength;
	ShakeSeconds = Seconds;
	ShakeElapsed = 0.f;
}

void AStoryDirector::PlaySound(USoundBase* Sound, const float Pitch) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound, 1.f, Pitch);
	}
}

void AStoryDirector::BeginPlayerControl(const FVector& Goal, const FName ObjectiveKey)
{
	AStoryCharacter* Player = GetPlayerCharacter();
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!Player || !PlayerController)
	{
		return;
	}
	Screen->SetPlayerControl(true, ObjectiveKey);
	PlayerController->Possess(Player);
	Player->SetPlayerControlled(true);
	PlayerController->SetControlRotation(FRotator(PlayerCameraPitch, (Goal - Player->GetActorLocation()).Rotation().Yaw, 0.f));
	PlayerController->SetViewTargetWithBlend(Player, PlayerCameraBlendSeconds, VTBlend_EaseInOut, 2.f);

	const FTransform MarkerTransform(FRotator::ZeroRotator, Goal + FVector(0.0, 0.0, GoalMarkerHeight), FVector(GoalMarkerScale));
	GoalMarker = GetWorld()->SpawnActorDeferred<AStoryFloatingProp>(AStoryFloatingProp::StaticClass(), MarkerTransform);
	GoalMarker->GetStaticMeshComponent()->SetStaticMesh(GoalMarkerMesh);
	GoalMarker->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GoalMarker->BobHeight = GoalMarkerBob;
	GoalMarker->PeriodSeconds = GoalMarkerBobSeconds;
	GoalMarker->SpinDegreesPerSecond = GoalMarkerSpin;
	GoalMarker->FinishSpawning(MarkerTransform);
}

void AStoryDirector::EndPlayerControl()
{
	if (!GoalMarker)
	{
		return;
	}
	GoalMarker->Destroy();
	GoalMarker = nullptr;
	Screen->SetPlayerControl(false, NAME_None);
	if (AStoryCharacter* Player = GetPlayerCharacter())
	{
		Player->SetPlayerControlled(false);
	}
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		// 걷던 카메라 자리에서 이어지게, 감독 카메라를 지금 시점으로 옮긴 뒤 넘긴다.
		FVector ViewLocation;
		FRotator ViewRotation;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		Camera->SetWorldLocationAndRotation(ViewLocation, ViewRotation);
		PlayerController->SetViewTarget(this);
	}
}
