#include "Story/StoryCharacter.h"

#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Story/StoryAnimInstance.h"
#include "Story/StoryCharacterDefinition.h"
#include "Story/StorySettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StoryCharacter)

namespace
{
	/** 스틱맨 몸의 크기(cm). 게임의 플레이어 캡슐과 같다. */
	constexpr float CapsuleRadius = 34.f;
	constexpr float CapsuleHalfHeight = 96.f;
	constexpr float HeadHeight = 178.f;
	constexpr float WalkSpeed = 260.f;

	const FName FaceCellParameter(TEXT("FaceCell"));
	const FName HeadBone(TEXT("head"));

	constexpr float LocomotionBlend = 0.2f;
	constexpr float IdleBlend = 0.28f;
	constexpr float ArriveDistance = 3.f;
	constexpr float FacingSpeed = 7.f;

	/** 머리를 돌리는 한계(도)와 따라가는 빠르기 */
	constexpr float MaxHeadYaw = 62.f;
	constexpr float MaxHeadPitch = 28.f;
	constexpr float HeadLookSpeed = 5.f;

	/** 말하는 입이 바뀌는 간격과 깜빡임 */
	constexpr float TalkFlapSeconds = 0.11f;
	constexpr float BlinkSeconds = 0.13f;
	constexpr float BlinkMinInterval = 1.8f;
	constexpr float BlinkMaxInterval = 4.6f;
	constexpr int32 BlinkCell = 2;

	/** 감정 표시가 튀어나오고 들어가는 시간, 머리 위 높이 */
	constexpr float EmotePopSeconds = 0.18f;
	constexpr float EmoteHeight = 62.f;
	constexpr float EmoteScale = 0.85f;

	constexpr float GlowBlendSpeed = 2.5f;
	constexpr float WanderMinPause = 0.8f;
	constexpr float WanderMaxPause = 2.2f;
	constexpr int32 WanderAttempts = 24;

	struct FFaceCells
	{
		int32 Rest = 0;
		int32 Talk = 1;
		bool bBlinks = true;
	};

	// 얼굴 텍스처(4x4 표정판)의 칸 번호. 왼쪽 위부터 오른쪽으로 센다.
	FFaceCells GetFaceCells(const EStoryExpression Expression)
	{
		switch (Expression)
		{
		case EStoryExpression::Happy: return {3, 4, false};
		case EStoryExpression::Laugh: return {4, 3, false};
		case EStoryExpression::Surprised: return {5, 1, true};
		case EStoryExpression::Scared: return {6, 7, true};
		case EStoryExpression::Worried: return {8, 1, true};
		case EStoryExpression::Angry: return {9, 10, false};
		case EStoryExpression::Smug: return {11, 4, true};
		case EStoryExpression::Deadpan: return {12, 1, false};
		case EStoryExpression::Asleep: return {13, 13, false};
		case EStoryExpression::Blank: return {14, 14, false};
		case EStoryExpression::Squeezed: return {15, 15, false};
		case EStoryExpression::Neutral: break;
		}
		return {};
	}

	/** 목표를 조금 넘었다 돌아오는 튀어나옴(0~1) */
	float PopScale(const float Alpha)
	{
		const float Clamped = FMath::Clamp(Alpha, 0.f, 1.f);
		constexpr float Overshoot = 1.70158f;
		const float Shifted = Clamped - 1.f;
		return 1.f + (Overshoot + 1.f) * FMath::Cube(Shifted) + Overshoot * FMath::Square(Shifted);
	}
}

AStoryCharacter::AStoryCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// FBX의 앞(+Y)을 액터의 앞(+X)에 맞춘다.
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -CapsuleHalfHeight), FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetAnimInstanceClass(UStoryAnimInstance::StaticClass());
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	FaceMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FaceMesh"));
	FaceMeshComponent->SetupAttachment(GetMesh());
	FaceMeshComponent->SetCastShadow(false);

	EmoteComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Emote"));
	EmoteComponent->SetupAttachment(GetRootComponent());
	EmoteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EmoteComponent->SetCastShadow(false);
	EmoteComponent->SetVisibility(false);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(GetMesh());
	GlowLight->SetRelativeLocation(FVector(0.f, 45.f, 130.f));
	GlowLight->SetLightColor(FLinearColor(1.f, 0.78f, 0.35f));
	GlowLight->SetIntensityUnits(ELightUnits::Candelas);
	GlowLight->SetIntensity(0.f);
	GlowLight->SetAttenuationRadius(420.f);
	GlowLight->SetCastShadows(false);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 430.f;
	SpringArm->SocketOffset = FVector(0.f, 0.f, 90.f);
	SpringArm->bUsePawnControlRotation = true;
	SpringArm->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	FollowCamera->SetFieldOfView(60.f);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->DefaultLandMovementMode = MOVE_None;
}

void AStoryCharacter::Initialize(const UStoryCharacterDefinition& InDefinition, const TMap<EStoryEmote, TObjectPtr<UStaticMesh>>& InEmoteMeshes)
{
	Definition = &InDefinition;
	EmoteMeshes = InEmoteMeshes;
	GetMesh()->SetSkeletalMeshAsset(InDefinition.BodyMesh);
	MeshBaseHeight = GetMesh()->GetRelativeLocation().Z;
	for (USkeletalMesh* Outfit : InDefinition.OutfitMeshes)
	{
		AddOutfitMesh(Outfit);
	}
	FaceMeshComponent->SetSkeletalMeshAsset(InDefinition.FaceMesh);
	FaceMeshComponent->SetLeaderPoseComponent(GetMesh());
	if (InDefinition.FaceMaterial)
	{
		FaceMaterial = UMaterialInstanceDynamic::Create(InDefinition.FaceMaterial, this);
		FaceMeshComponent->SetMaterial(0, FaceMaterial);
	}
	DesiredYaw = GetActorRotation().Yaw;
	BobElapsed = FMath::FRandRange(0.f, InDefinition.BobSeconds);
	PlayClip(NAME_None, true, 0.f);
	SetExpression(EStoryExpression::Neutral);
}

USkeletalMeshComponent* AStoryCharacter::AddOutfitMesh(USkeletalMesh* InMesh)
{
	USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(this);
	Component->SetupAttachment(GetMesh());
	Component->SetSkeletalMeshAsset(InMesh);
	Component->SetLeaderPoseComponent(GetMesh());
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 옷은 몸의 경계를 함께 써서, 화면 밖으로 잘못 판정되어 사라지지 않게 한다.
	Component->bUseAttachParentBound = true;
	Component->RegisterComponent();
	return Component;
}

UStoryAnimInstance* AStoryCharacter::GetStoryAnimInstance() const
{
	return Cast<UStoryAnimInstance>(GetMesh()->GetAnimInstance());
}

void AStoryCharacter::PlayClip(const FName ClipName, const bool bLoop, const float BlendSeconds)
{
	HeldClip = ClipName;
	bHeldClipLoops = ClipName.IsNone() || bLoop;
	bPlayingLocomotion = false;
	const FName ResolvedName = ClipName.IsNone() ? Definition->IdleClip : ClipName;
	const TObjectPtr<UAnimSequenceBase>* Clip = Definition->Clips.Find(ResolvedName);
	if (UStoryAnimInstance* Anim = GetStoryAnimInstance(); Anim && ensureMsgf(Clip, TEXT("Story clip %s is missing."), *ResolvedName.ToString()))
	{
		Anim->Play(*Clip, bHeldClipLoops, BlendSeconds);
	}
}

float AStoryCharacter::GetClipLength(const FName ClipName) const
{
	const TObjectPtr<UAnimSequenceBase>* Clip = Definition->Clips.Find(ClipName);
	return Clip && *Clip ? (*Clip)->GetPlayLength() : 0.f;
}

void AStoryCharacter::SetExpression(const EStoryExpression InExpression)
{
	Expression = InExpression;
	BlinkCountdown = FMath::FRandRange(BlinkMinInterval, BlinkMaxInterval);
}

void AStoryCharacter::SetTalking(const bool bInTalking)
{
	bTalking = bInTalking;
	TalkElapsed = 0.f;
}

void AStoryCharacter::ShowEmote(const EStoryEmote Emote, const float Seconds)
{
	if (const TObjectPtr<UStaticMesh>* EmoteMesh = EmoteMeshes.Find(Emote))
	{
		EmoteComponent->SetStaticMesh(*EmoteMesh);
		EmoteComponent->SetVisibility(true);
		EmoteElapsed = 0.f;
		EmoteSeconds = Seconds;
	}
}

void AStoryCharacter::SetGlow(const float InTargetIntensity)
{
	GlowTarget = InTargetIntensity;
}

void AStoryCharacter::FaceYaw(const float WorldYaw, const bool bInstant)
{
	DesiredYaw = WorldYaw;
	if (bInstant)
	{
		SetActorRotation(FRotator(0.f, WorldYaw, 0.f));
	}
}

void AStoryCharacter::FaceLocation(const FVector& Location, const bool bInstant)
{
	const FVector Direction = Location - GetActorLocation();
	if (Direction.SizeSquared2D() > UE_KINDA_SMALL_NUMBER)
	{
		FaceYaw(Direction.Rotation().Yaw, bInstant);
	}
}

float AStoryCharacter::GetFacingYaw() const
{
	return bPlayerControlled ? GetActorRotation().Yaw : DesiredYaw;
}

FVector AStoryCharacter::GetHeadLocation() const
{
	return GetMesh()->DoesSocketExist(HeadBone) ? GetMesh()->GetSocketLocation(HeadBone) : GetActorLocation() + FVector(0.0, 0.0, HeadHeight - CapsuleHalfHeight);
}

FVector AStoryCharacter::GetGroundLocation() const
{
	return GetActorLocation() - FVector(0.0, 0.0, CapsuleHalfHeight);
}

void AStoryCharacter::MoveTo(const FVector& GroundLocation, const float Speed, const bool bKeepFacing)
{
	MoveTarget = GroundLocation + FVector(0.0, 0.0, CapsuleHalfHeight);
	MoveSpeed = Speed;
	bMoveKeepsFacing = bKeepFacing;
}

void AStoryCharacter::PlaceAt(const FVector& GroundLocation, const float WorldYaw)
{
	MoveTarget.Reset();
	SetActorLocation(GroundLocation + FVector(0.0, 0.0, CapsuleHalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
	FaceYaw(WorldYaw, true);
}

void AStoryCharacter::StartWander(const FStoryWander& InWander)
{
	Wander = InWander;
	WanderPause = WanderMinPause;
}

void AStoryCharacter::StopWander()
{
	Wander.Reset();
	MoveTarget.Reset();
}

void AStoryCharacter::SetBodyScale(const float Scale)
{
	GetMesh()->SetRelativeScale3D(FVector(Scale));
}

void AStoryCharacter::AddMeshSpin(const float Degrees)
{
	GetMesh()->AddLocalRotation(FRotator(0.f, Degrees, 0.f));
}

void AStoryCharacter::SetPlayerControlled(const bool bControlled)
{
	bPlayerControlled = bControlled;
	MoveTarget.Reset();
	GetCapsuleComponent()->SetCollisionEnabled(bControlled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (bControlled)
	{
		GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	else
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		DesiredYaw = GetActorRotation().Yaw;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
	{
		if (UInputMappingContext* Mapping = GetDefault<UStorySettings>()->InputMapping.LoadSynchronous(); Mapping && bControlled)
		{
			Input->AddMappingContext(Mapping, 0);
		}
		else if (Mapping)
		{
			Input->RemoveMappingContext(Mapping);
		}
	}
	SetHeadLookEnabled(!bControlled);
	if (!bControlled)
	{
		PlayClip(NAME_None, true, IdleBlend);
	}
}

void AStoryCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		const UStorySettings* Settings = GetDefault<UStorySettings>();
		Input->BindAction(Settings->MoveAction.LoadSynchronous(), ETriggerEvent::Triggered, this, &ThisClass::HandleMoveInput);
		Input->BindAction(Settings->LookAction.LoadSynchronous(), ETriggerEvent::Triggered, this, &ThisClass::HandleLookInput);
	}
}

// 게임의 플레이어와 같은 축이다(Y가 앞뒤, X가 좌우).
void AStoryCharacter::HandleMoveInput(const FInputActionValue& Value)
{
	if (bPlayerControlled && Controller)
	{
		const FVector2D Input = Value.Get<FVector2D>();
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(YawRotation.RotateVector(FVector::ForwardVector), Input.Y);
		AddMovementInput(YawRotation.RotateVector(FVector::RightVector), Input.X);
	}
}

void AStoryCharacter::HandleLookInput(const FInputActionValue& Value)
{
	if (bPlayerControlled)
	{
		const FVector2D Input = Value.Get<FVector2D>();
		AddControllerYawInput(Input.X);
		AddControllerPitchInput(Input.Y);
	}
}

void AStoryCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Definition)
	{
		return;
	}
	if (bPlayerControlled)
	{
		PlayLocomotion(GetVelocity());
	}
	else
	{
		TickWander(DeltaSeconds);
		TickMovement(DeltaSeconds);
		TickFacing(DeltaSeconds);
	}
	TickHeadLook(DeltaSeconds);
	TickFace(DeltaSeconds);
	TickEmote(DeltaSeconds);
	TickBob(DeltaSeconds);
	GlowLight->SetIntensity(FMath::FInterpTo(GlowLight->Intensity, GlowTarget, DeltaSeconds, GlowBlendSpeed));
}

void AStoryCharacter::TickMovement(const float DeltaSeconds)
{
	if (!MoveTarget.IsSet())
	{
		return;
	}
	const FVector Start = GetActorLocation();
	const FVector ToTarget = MoveTarget.GetValue() - Start;
	const float Step = MoveSpeed * DeltaSeconds;
	if (ToTarget.Size() <= FMath::Max(Step, ArriveDistance))
	{
		SetActorLocation(MoveTarget.GetValue());
		MoveTarget.Reset();
		PlayLocomotion(FVector::ZeroVector);
		return;
	}
	const FVector Velocity = ToTarget.GetSafeNormal() * MoveSpeed;
	SetActorLocation(Start + Velocity * DeltaSeconds);
	if (!bMoveKeepsFacing)
	{
		DesiredYaw = Velocity.Rotation().Yaw;
	}
	PlayLocomotion(Velocity);
}

// 떠 있는 인물은 걷는 동작 없이 움직인다. 멈추면 걷기 전에 하던 동작으로 돌아간다.
void AStoryCharacter::PlayLocomotion(const FVector& Velocity)
{
	UStoryAnimInstance* Anim = GetStoryAnimInstance();
	if (!Anim || Definition->bFloats)
	{
		return;
	}
	const float Speed = Velocity.Size2D();
	if (Speed < 5.f)
	{
		if (bPlayingLocomotion)
		{
			PlayClip(HeldClip, bHeldClipLoops, IdleBlend);
		}
		return;
	}
	const bool bBackward = FVector::DotProduct(Velocity.GetSafeNormal2D(), GetActorForwardVector()) < -0.3;
	if (const TObjectPtr<UAnimSequenceBase>* Clip = Definition->Clips.Find(bBackward ? Definition->WalkBackClip : Definition->WalkClip))
	{
		Anim->Play(*Clip, true, LocomotionBlend);
		Anim->SetPlayRate(Speed / Definition->WalkClipSpeed);
		bPlayingLocomotion = true;
	}
}

void AStoryCharacter::TickWander(const float DeltaSeconds)
{
	if (!Wander.IsSet() || !Wander->Center.IsValid())
	{
		return;
	}
	if (bTalking)
	{
		MoveTarget.Reset();
		WanderPause = WanderMinPause;
		return;
	}
	if (MoveTarget.IsSet())
	{
		return;
	}
	WanderPause -= DeltaSeconds;
	FVector Spot;
	if (WanderPause <= 0.f && ChooseWanderSpot(Spot))
	{
		MoveTo(Spot, Wander->Speed, true);
		WanderPause = FMath::FRandRange(WanderMinPause, WanderMaxPause);
	}
}

// 가운데 인물 앞쪽 부채꼴에서, 다른 인물과 겹치지 않는 자리를 고른다.
bool AStoryCharacter::ChooseWanderSpot(FVector& OutSpot) const
{
	const FVector Center = Wander->Center->GetActorLocation();
	for (int32 Attempt = 0; Attempt < WanderAttempts; ++Attempt)
	{
		const float Yaw = Wander->FacingYaw + FMath::FRandRange(-Wander->HalfArc, Wander->HalfArc);
		const FVector Offset = FRotator(0.f, Yaw, 0.f).Vector() * FMath::FRandRange(Wander->NearRadius, Wander->FarRadius);
		const FVector Spot(Center.X + Offset.X, Center.Y + Offset.Y, GetActorLocation().Z);
		const bool bBlocked = Wander->Avoid.ContainsByPredicate([&Spot, this](const TWeakObjectPtr<const AActor>& Other)
		{
			return Other.IsValid() && FVector::Dist2D(Other->GetActorLocation(), Spot) < Wander->NearRadius * 0.8f;
		});
		if (!bBlocked)
		{
			OutSpot = Spot - FVector(0.0, 0.0, CapsuleHalfHeight);
			return true;
		}
	}
	return false;
}

void AStoryCharacter::TickFacing(const float DeltaSeconds)
{
	if (Definition->bFloats && LookTarget.IsValid())
	{
		const FVector Direction = LookTarget->GetActorLocation() - GetActorLocation();
		if (Direction.SizeSquared2D() > UE_KINDA_SMALL_NUMBER)
		{
			DesiredYaw = Direction.Rotation().Yaw;
		}
	}
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, DesiredYaw, 0.f), DeltaSeconds, FacingSpeed));
}

void AStoryCharacter::TickHeadLook(const float DeltaSeconds)
{
	float TargetYaw = 0.f;
	float TargetPitch = 0.f;
	if (bHeadLookEnabled && LookTarget.IsValid())
	{
		const AStoryCharacter* TargetCharacter = Cast<AStoryCharacter>(LookTarget.Get());
		const FVector TargetLocation = TargetCharacter ? TargetCharacter->GetHeadLocation() : LookTarget->GetActorLocation();
		const FRotator Local = (GetActorRotation().UnrotateVector(TargetLocation - GetHeadLocation())).Rotation();
		// 등 뒤를 볼 만큼 돌아가야 하면 머리를 돌리지 않는다.
		if (FMath::Abs(Local.Yaw) < 150.f)
		{
			TargetYaw = FMath::Clamp(Local.Yaw, -MaxHeadYaw, MaxHeadYaw);
			TargetPitch = FMath::Clamp(Local.Pitch, -MaxHeadPitch, MaxHeadPitch);
		}
	}
	HeadYaw = FMath::FInterpTo(HeadYaw, TargetYaw, DeltaSeconds, HeadLookSpeed);
	HeadPitch = FMath::FInterpTo(HeadPitch, TargetPitch, DeltaSeconds, HeadLookSpeed);
	if (UStoryAnimInstance* Anim = GetStoryAnimInstance())
	{
		Anim->SetHeadLook(HeadYaw, HeadPitch);
	}
}

void AStoryCharacter::TickFace(const float DeltaSeconds)
{
	const FFaceCells Cells = GetFaceCells(Expression);
	int32 Cell = Cells.Rest;
	if (bTalking)
	{
		TalkElapsed += DeltaSeconds;
		Cell = FMath::FloorToInt32(TalkElapsed / TalkFlapSeconds) % 2 == 0 ? Cells.Talk : Cells.Rest;
	}
	else if (Cells.bBlinks)
	{
		BlinkCountdown -= DeltaSeconds;
		if (BlinkCountdown < -BlinkSeconds)
		{
			BlinkCountdown = FMath::FRandRange(BlinkMinInterval, BlinkMaxInterval);
		}
		Cell = BlinkCountdown < 0.f ? BlinkCell : Cell;
	}
	if (FaceMaterial && Cell != ShownFaceCell)
	{
		ShownFaceCell = Cell;
		FaceMaterial->SetScalarParameterValue(FaceCellParameter, Cell);
	}
}

void AStoryCharacter::TickEmote(const float DeltaSeconds)
{
	if (!EmoteComponent->IsVisible())
	{
		return;
	}
	EmoteElapsed += DeltaSeconds;
	if (EmoteElapsed >= EmoteSeconds)
	{
		EmoteComponent->SetVisibility(false);
		return;
	}
	const float Remaining = EmoteSeconds - EmoteElapsed;
	const float Scale = EmoteScale * FMath::Min(PopScale(EmoteElapsed / EmotePopSeconds), FMath::Clamp(Remaining / EmotePopSeconds, 0.f, 1.f));
	const FVector Location = GetHeadLocation() + FVector(0.0, 0.0, EmoteHeight + 4.0 * FMath::Sin(EmoteElapsed * 5.f));
	float CameraYaw = GetActorRotation().Yaw;
	if (const APlayerController* Viewer = GetWorld()->GetFirstPlayerController(); Viewer && Viewer->PlayerCameraManager)
	{
		CameraYaw = (Viewer->PlayerCameraManager->GetCameraLocation() - Location).Rotation().Yaw;
	}
	// 감정 표시 메시의 앞은 FBX의 앞(+Y)이다.
	EmoteComponent->SetWorldLocationAndRotation(Location, FRotator(0.f, CameraYaw - 90.f, 0.f));
	EmoteComponent->SetWorldScale3D(FVector(FMath::Max(Scale, UE_KINDA_SMALL_NUMBER)));
}

void AStoryCharacter::TickBob(const float DeltaSeconds)
{
	if (!Definition->bFloats)
	{
		return;
	}
	BobElapsed += DeltaSeconds;
	FVector Relative = GetMesh()->GetRelativeLocation();
	Relative.Z = MeshBaseHeight + Definition->BobHeight * FMath::Sin(UE_TWO_PI * BobElapsed / Definition->BobSeconds);
	GetMesh()->SetRelativeLocation(Relative);
}
