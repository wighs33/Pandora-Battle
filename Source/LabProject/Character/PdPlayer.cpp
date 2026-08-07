#include "Character/PdPlayer.h"

#include "AbilitySystemComponent.h"
#include "CableComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"
#include "Mode/PdPlayerState.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/GrappleComponent.h"
#include "Component/Player/PaintCanvasComponent.h"
#include "Component/Player/PlayerActionComponent.h"
#include "Component/Player/PlayerAimComponent.h"
#include "Component/Player/PlayerCameraComponent.h"
#include "Component/Player/PlayerInteractionComponent.h"
#include "Definition/Player/PlayerPawnDefinition.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayer)

namespace
{
	const FName GrappleHookComponentName(TEXT("Hook"));
	const FName GrappleHookSocketName(TEXT("hand_r"));
}

APdPlayer::APdPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================

	PlayerInteractionComponent =
		CreateDefaultSubobject<UPlayerInteractionComponent>(TEXT("InteractionBox"));
	PlayerInteractionComponent->SetupAttachment(GetRootComponent());
	InteractionBox = PlayerInteractionComponent;

	// =================================================================================================================

	GrappleComponent = CreateDefaultSubobject<UGrappleComponent>(TEXT("GrappleComponent"));
	PaintCanvasComponent = CreateDefaultSubobject<UPaintCanvasComponent>(TEXT("PaintCanvasComponent"));
	PlayerActionComponent = CreateDefaultSubobject<UPlayerActionComponent>(TEXT("PlayerActionComponent"));
	PlayerAimComponent = CreateDefaultSubobject<UPlayerAimComponent>(TEXT("PlayerAimComponent"));
	PlayerCameraComponent = CreateDefaultSubobject<UPlayerCameraComponent>(TEXT("PlayerCameraComponent"));

	// =================================================================================================================

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

	// =================================================================================================================

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	HookComponent = CreateDefaultSubobject<UCableComponent>(GrappleHookComponentName);
	HookComponent->SetupAttachment(GetMesh(), GrappleHookSocketName);
	HookComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HookComponent->SetGenerateOverlapEvents(false);
	HookComponent->SetHiddenInGame(true);
	HookComponent->SetVisibility(false, true);
	HookComponent->bAttachStart = true;
	HookComponent->bAttachEnd = false;
	HookComponent->CableLength = 0.0f;
	HookComponent->EndLocation = FVector::ZeroVector;
	if (GrappleComponent)
	{
		GrappleComponent->SetHookComponent(HookComponent);
	}

	SpeechBubblePlaneComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpeechBubble"));
	SpeechBubblePlaneComponent->SetupAttachment(GetRootComponent());
	SpeechBubblePlaneComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpeechBubblePlaneComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SpeechBubblePlaneComponent->SetGenerateOverlapEvents(false);
	SpeechBubblePlaneComponent->SetCanEverAffectNavigation(false);
	SpeechBubblePlaneComponent->SetHiddenInGame(true);
	SpeechBubblePlaneComponent->SetVisibility(true, true);
	if (PaintCanvasComponent)
	{
		PaintCanvasComponent->SetSpeechBubbleComponent(SpeechBubblePlaneComponent);
	}
}

void APdPlayer::BeginPlay()
{
	Super::BeginPlay();
	ResolvePlayerPawnDefinition();
	ApplyPlayerPawnDefinition();
	if (PlayerAimComponent)
	{
		PlayerAimComponent->StartReplication();
	}

	if (GrappleComponent)
	{
		GrappleComponent->SetHookComponent(HookComponent);
		GrappleComponent->ConfigureHookComponent();
	}
	if (PaintCanvasComponent)
	{
		PaintCanvasComponent->SetSpeechBubbleComponent(SpeechBubblePlaneComponent);
		PaintCanvasComponent->HidePaintSpeechBubble();
	}

}

void APdPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PlayerAimComponent)
	{
		PlayerAimComponent->StopReplication();
	}
	if (PlayerCameraComponent)
	{
		PlayerCameraComponent->ShutdownCamera();
	}
	Super::EndPlay(EndPlayReason);
}

void APdPlayer::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GrappleComponent)
	{
		GrappleComponent->SetHookComponent(HookComponent);
		GrappleComponent->ConfigureHookComponent();
	}
	if (PaintCanvasComponent)
	{
		PaintCanvasComponent->SetSpeechBubbleComponent(SpeechBubblePlaneComponent);
	}
}

void APdPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ReapplyCurrentRotationPolicy();
	if (PlayerAimComponent)
	{
		PlayerAimComponent->StartReplication();
	}
	if (APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>())
	{
		if (UPandoraTreeComponent* PandoraTreeComponent = PdPlayerState->GetPandoraTreeComponent())
		{
			PandoraTreeComponent->InitializeForCurrentSession();
		}
	}
}

void APdPlayer::UnPossessed()
{
	if (PlayerAimComponent)
	{
		PlayerAimComponent->StopReplication();
	}
	if (PlayerCameraComponent)
	{
		PlayerCameraComponent->ShutdownCamera();
	}
	Super::UnPossessed();
}

void APdPlayer::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	ReapplyCurrentRotationPolicy();
	if (!IsLocallyControlled() && PlayerCameraComponent)
	{
		PlayerCameraComponent->ShutdownCamera();
	}
}

void APdPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled())
	{
		UpdateAimOffsetForAnimation();
		if (PlayerCameraComponent)
		{
			PlayerCameraComponent->TickPresentation(DeltaSeconds);
		}
	}
}

void APdPlayer::ResolvePlayerPawnDefinition()
{
	if (PlayerPawnDefinition)
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const FPrimaryAssetId DefinitionId =
		UPlayerPawnDefinition::GetDefaultPrimaryAssetId();
	PlayerPawnDefinition =
		AssetManager.GetPrimaryAssetObject<UPlayerPawnDefinition>(DefinitionId);
	if (!PlayerPawnDefinition)
	{
		const FSoftObjectPath DefinitionPath =
			AssetManager.GetPrimaryAssetPath(DefinitionId);
		PlayerPawnDefinition = Cast<UPlayerPawnDefinition>(DefinitionPath.TryLoad());
	}

	if (!PlayerPawnDefinition)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Player pawn definition '%s' was not registered or preloaded."),
			*DefinitionId.ToString());
	}
}

void APdPlayer::ApplyPlayerPawnDefinition()
{
	FPlayerInteractionSettings InteractionSettings;
	InteractionSettings.ServerValidationDistance = InteractionServerValidationDistance;

	FPlayerCameraPresentationSettings CameraSettings;
	FPlayerAimSettings AimSettings;

	if (PlayerPawnDefinition)
	{
		InteractionSettings = PlayerPawnDefinition->GetInteractionSettings();
		CameraSettings = PlayerPawnDefinition->GetCameraSettings();
		AimSettings = PlayerPawnDefinition->GetAimSettings();
	}

	InteractionServerValidationDistance = InteractionSettings.ServerValidationDistance;

	if (PlayerInteractionComponent)
	{
		PlayerInteractionComponent->ApplySettings(InteractionSettings);
	}
	if (PlayerCameraComponent)
	{
		PlayerCameraComponent->InitializeCamera(CameraBoom, FollowCamera, CameraSettings);
	}
	if (PlayerAimComponent)
	{
		PlayerAimComponent->ApplySettings(AimSettings);
	}
	if (PlayerActionComponent)
	{
		PlayerActionComponent->ApplyDefinition(PlayerPawnDefinition);
	}
	if (UCombatComponent* CombatComponent = GetCombatComponent())
	{
		CombatComponent->ApplyDefinition(PlayerPawnDefinition);
	}

	ReapplyCurrentRotationPolicy();
}

void APdPlayer::UpdateAimOffsetForReplicationComponent()
{
	UpdateAimOffsetForAnimation();
}

void APdPlayer::ApplyReplicatedAimOffsetFromComponent(
	const float AimYaw,
	const float AimPitch)
{
	if (!IsLocallyControlled())
	{
		SetAimOffsetForAnimation(AimYaw, AimPitch);
	}
}

UAbilitySystemComponent* APdPlayer::GetAbilitySystemComponent() const
{
	if (const APdPlayerState* PdPlayerState = GetPdPlayerState())
	{
		return PdPlayerState->GetAbilitySystemComponent();
	}

	return nullptr;
}

void APdPlayer::HandleDeath_Implementation()
{
	if (UCombatComponent* CombatComponent = GetCombatComponent())
	{
		CombatComponent->StopPrimaryAttack();
		if (IsLocallyControlled())
		{
			CombatComponent->StopAim();
		}
	}

	if (HasAuthority() && !IsLocallyControlled())
	{
		SetWeaponAimActive(false, FWeaponAimCameraSettings());
	}

	if (GrappleComponent)
	{
		GrappleComponent->StopGrapple();
	}

	Super::HandleDeath_Implementation();
}

void APdPlayer::ResetDeathStateForRespawn()
{
	Super::ResetDeathStateForRespawn();

	// Restoring the skeletal mesh propagates visibility to attached children.
	// Reset the grapple after that step so a previously extended cable cannot
	// become visible again on the reused character.
	if (GrappleComponent)
	{
		GrappleComponent->ResetForRespawn();
	}

	ReapplyCurrentRotationPolicy();
}

AActor* APdPlayer::GetAbilitySystemOwnerActor() const
{
	return GetPdPlayerState();
}

bool APdPlayer::ShouldUseContinuousCharacterTick() const
{
	// Camera interpolation and local occlusion are presentation-only. Simulated
	// proxies and dedicated-server player pawns do not need an actor tick.
	return IsLocallyControlled();
}

bool APdPlayer::IsGrappling() const
{
	return GrappleComponent && GrappleComponent->IsGrappling();
}

bool APdPlayer::RequestCancelHitReactForMovement(const float BlendOutTime)
{
	return PlayerActionComponent
		&& PlayerActionComponent->RequestCancelHitReactForMovement(BlendOutTime);
}

void APdPlayer::HidePaintCanvas()
{
	if (PaintCanvasComponent)
	{
		PaintCanvasComponent->HidePaintCanvas();
	}
}

bool APdPlayer::HasActivePaintCanvas() const
{
	return PaintCanvasComponent ? PaintCanvasComponent->HasActivePaintCanvas() : false;
}

bool APdPlayer::ExportActivePaintCanvasToSpeechBubble()
{
	return PaintCanvasComponent
		&& PaintCanvasComponent->ExportActivePaintCanvasToSpeechBubble();
}

bool APdPlayer::ApplyActivePaintCanvasToFaceDecal(
	UMaterialInterface* FaceDecalMaterial,
	FName AttachSocketName,
	const FTransform& FaceDecalTransformOffset,
	FVector FaceDecalSize,
	FName TextureParameterName)
{
	return PaintCanvasComponent
		&& PaintCanvasComponent->ApplyActivePaintCanvasToFaceDecal(
			FaceDecalMaterial,
			AttachSocketName,
			FaceDecalTransformOffset,
			FaceDecalSize,
			TextureParameterName);
}

void APdPlayer::RestoreCachedLobbyPaintCanvasFaceDecal()
{
	if (PaintCanvasComponent)
	{
		PaintCanvasComponent->RestoreCachedLobbyPaintCanvasFaceDecal();
	}
}

void APdPlayer::PlayInteractionMontage(UAnimMontage* Montage, float PlayRate)
{
	if (PlayerInteractionComponent)
	{
		PlayerInteractionComponent->PlayInteractionMontage(Montage, PlayRate);
	}
}

void APdPlayer::StopInteractionMontage(float BlendOutTime)
{
	if (PlayerInteractionComponent)
	{
		PlayerInteractionComponent->StopInteractionMontage(BlendOutTime);
	}
}

bool APdPlayer::IsInteractionMontagePlaying() const
{
	return PlayerInteractionComponent
		&& PlayerInteractionComponent->IsInteractionMontagePlaying();
}

APdPlayerState* APdPlayer::GetPdPlayerState() const
{
	return GetPlayerState<APdPlayerState>();
}

bool APdPlayer::HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const
{
	return PlayerInteractionComponent
		&& PlayerInteractionComponent->HasCurrentInteractActors(OutCurrentInteractActors);
}

AActor* APdPlayer::GetCurrentInteractActor() const
{
	return PlayerInteractionComponent
		? PlayerInteractionComponent->GetCurrentInteractActor()
		: nullptr;
}

bool APdPlayer::InteractWithCurrentTarget()
{
	return PlayerInteractionComponent
		&& PlayerInteractionComponent->InteractWithCurrentTarget();
}

void APdPlayer::SetWeaponAimActive(bool bEnabled, const FWeaponAimCameraSettings& AimCameraSettings)
{
	if (PlayerAimComponent)
	{
		PlayerAimComponent->SetWeaponAimActive(bEnabled, AimCameraSettings);
	}
}

bool APdPlayer::IsWeaponAimActive() const
{
	return PlayerAimComponent && PlayerAimComponent->IsWeaponAimActive();
}

void APdPlayer::SetAbilityCameraOverrideActive(bool bEnabled, const FWeaponAimCameraSettings& CameraSettings)
{
	if (!PlayerCameraComponent)
	{
		return;
	}

	PlayerCameraComponent->SetAbilityCameraOverrideActive(bEnabled, CameraSettings);
}

void APdPlayer::SetAbilityCameraOverrideActiveForDuration(
	bool bEnabled,
	const FWeaponAimCameraSettings& CameraSettings,
	float Duration)
{
	if (!PlayerCameraComponent)
	{
		return;
	}

	PlayerCameraComponent->SetAbilityCameraOverrideActiveForDuration(
		bEnabled,
		CameraSettings,
		Duration);
}

void APdPlayer::ApplyCurrentRotationPolicy(
	UCharacterMovementComponent* MovementComponent)
{
	if (!MovementComponent)
	{
		return;
	}

	if (PlayerAimComponent)
	{
		PlayerAimComponent->RestoreMovementSettings(MovementComponent);
		return;
	}

	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->bUseControllerDesiredRotation = false;
	MovementComponent->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	bUseControllerRotationYaw = false;
}

bool APdPlayer::GetWeaponAimViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	return PlayerCameraComponent
		&& PlayerCameraComponent->GetAimViewPoint(OutLocation, OutDirection);
}

bool APdPlayer::CanInteractWithActor(AActor* InteractableActor) const
{
	return PlayerInteractionComponent
		&& PlayerInteractionComponent->CanInteractWithActor(InteractableActor);
}
