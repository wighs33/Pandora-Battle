#include "Character/PdPlayer.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"

#include "Camera/CameraComponent.h"
#include "Common/Enum_Direction.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraComponent.h"
#include "Pandora/PandoraDefinition.h"
#include "SavedGameData/PdSaveGame.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayer)

DEFINE_LOG_CATEGORY_STATIC(PdPlayerLog, Log, All);

namespace
{
	bool TryGetPandoraLoadoutDirectionFromSaveName(const FName DirectionName, EEnum_Direction& OutDirection)
	{
		if (DirectionName == TEXT("Left"))
		{
			OutDirection = EEnum_Direction::Left;
			return true;
		}

		if (DirectionName == TEXT("Up"))
		{
			OutDirection = EEnum_Direction::Up;
			return true;
		}

		if (DirectionName == TEXT("Right"))
		{
			OutDirection = EEnum_Direction::Right;
			return true;
		}

		return false;
	}
}

/** ?åÎ†à?¥Ïñ¥ Í∏∞Î≥∏ ?ÅÌÉúÎ•?Ï¥àÍ∏∞?îÌï©?àÎã§. */
APdPlayer::APdPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================
	// === ?ÅÌò∏?ëÏö© Î∞ïÏä§ ?§Ï†ï

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(GetRootComponent());
	InteractionBox->SetBoxExtent(FVector(50.0f, 50.0f, 100.0f));
	InteractionBox->SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));
	InteractionBox->SetRelativeRotation(FRotator::ZeroRotator);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionBox->SetGenerateOverlapEvents(true);
	InteractionBox->SetCanEverAffectNavigation(false);
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &APdPlayer::HandleInteractionBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &APdPlayer::HandleInteractionBoxEndOverlap);

	// =================================================================================================================
	// === Ïπ¥Î©î??Î∂??§Ï†ï

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// =================================================================================================================
	// === Ï∂îÏ†Å Ïπ¥Î©î???§Ï†ï

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void APdPlayer::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionBox)
	{
		InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
		InteractionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
		InteractionBox->SetGenerateOverlapEvents(true);
	}

	if (!FollowCamera || !CameraBoom)
	{
		return;
	}

	DefaultCameraFOV = FollowCamera->FieldOfView;
	DefaultCameraBoomSocketOffset = CameraBoom->SocketOffset;
	DefaultFollowCameraRelativeRotation = FollowCamera->GetRelativeRotation();
	bHasCachedWeaponAimCameraDefaults = true;
}

void APdPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializePandoraTreeFromSave(NewController);
}

void APdPlayer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateWeaponAimCamera(DeltaSeconds);
}

/** PlayerState Í∏∞Ï? ASCÎ•?Î∞òÌôò?©Îãà?? */
UAbilitySystemComponent* APdPlayer::GetAbilitySystemComponent() const
{
	if (const APdPlayerState* PdPlayerState = GetPdPlayerState())
	{
		return PdPlayerState->GetAbilitySystemComponent();
	}

	return nullptr;
}

/** ASC ?åÏú† ?°ÌÑ∞Î•?Î∞òÌôò?©Îãà?? */
AActor* APdPlayer::GetAbilitySystemOwnerActor() const
{
	return GetPdPlayerState();
}

void APdPlayer::PlayInteractionMontage(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		UE_LOG(PdPlayerLog, Warning,
			TEXT("[InteractionAnimation] skipped: montage missing. pawn=%s authority=%s local=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			IsLocallyControlled() ? TEXT("true") : TEXT("false"));
		return;
	}

	const float SafePlayRate = PlayRate > 0.0f ? PlayRate : 1.0f;
	if (HasAuthority())
	{
		UE_LOG(PdPlayerLog, Log,
			TEXT("[InteractionAnimation] multicast requested. pawn=%s montage=%s playRate=%.2f local=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Montage),
			SafePlayRate,
			IsLocallyControlled() ? TEXT("true") : TEXT("false"));
		MulticastPlayInteractionMontage(Montage, SafePlayRate);
		return;
	}

	const float PlayedDuration = PlayAnimMontage(Montage, SafePlayRate);
	if (PlayedDuration <= 0.0f)
	{
		UE_LOG(PdPlayerLog, Warning,
			TEXT("[InteractionAnimation] local play failed. pawn=%s montage=%s playRate=%.2f mesh=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Montage),
			SafePlayRate,
			*GetNameSafe(GetMesh()));
		return;
	}

	ActiveInteractionMontage = Montage;

	UE_LOG(PdPlayerLog, Log,
		TEXT("[InteractionAnimation] local play started. pawn=%s montage=%s playRate=%.2f duration=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(Montage),
		SafePlayRate,
		PlayedDuration);
}

void APdPlayer::MulticastPlayInteractionMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
	if (!Montage)
	{
		UE_LOG(PdPlayerLog, Warning,
			TEXT("[InteractionAnimation] multicast skipped: montage missing. pawn=%s authority=%s local=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			IsLocallyControlled() ? TEXT("true") : TEXT("false"));
		return;
	}

	const float SafePlayRate = PlayRate > 0.0f ? PlayRate : 1.0f;
	const float PlayedDuration = PlayAnimMontage(Montage, SafePlayRate);
	if (PlayedDuration <= 0.0f)
	{
		UE_LOG(PdPlayerLog, Warning,
			TEXT("[InteractionAnimation] multicast play failed. pawn=%s montage=%s playRate=%.2f authority=%s local=%s mesh=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Montage),
			SafePlayRate,
			HasAuthority() ? TEXT("true") : TEXT("false"),
			IsLocallyControlled() ? TEXT("true") : TEXT("false"),
			*GetNameSafe(GetMesh()));
		return;
	}

	ActiveInteractionMontage = Montage;

	UE_LOG(PdPlayerLog, Log,
		TEXT("[InteractionAnimation] multicast play started. pawn=%s montage=%s playRate=%.2f duration=%.3f authority=%s local=%s"),
		*GetNameSafe(this),
		*GetNameSafe(Montage),
		SafePlayRate,
		PlayedDuration,
		HasAuthority() ? TEXT("true") : TEXT("false"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));
}

void APdPlayer::StopInteractionMontage(float BlendOutTime)
{
	const float SafeBlendOutTime = FMath::Max(0.0f, BlendOutTime);
	if (!IsInteractionMontagePlaying())
	{
		ActiveInteractionMontage = nullptr;
		return;
	}

	if (HasAuthority())
	{
		MulticastStopInteractionMontage(SafeBlendOutTime);
		return;
	}

	StopInteractionMontageLocally(SafeBlendOutTime);
	ServerStopInteractionMontage(SafeBlendOutTime);
}

bool APdPlayer::IsInteractionMontagePlaying() const
{
	if (!ActiveInteractionMontage)
	{
		return false;
	}

	const USkeletalMeshComponent* MeshComponent = GetMesh();
	const UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	return AnimInstance && AnimInstance->Montage_IsPlaying(ActiveInteractionMontage);
}

void APdPlayer::ServerStopInteractionMontage_Implementation(float BlendOutTime)
{
	MulticastStopInteractionMontage(FMath::Max(0.0f, BlendOutTime));
}

void APdPlayer::MulticastStopInteractionMontage_Implementation(float BlendOutTime)
{
	UAnimMontage* MontageBeforeStop = ActiveInteractionMontage.Get();
	const bool bStopped = StopInteractionMontageLocally(FMath::Max(0.0f, BlendOutTime));
	UE_LOG(PdPlayerLog, Log,
		TEXT("[InteractionAnimation] stop requested. pawn=%s montage=%s stopped=%s authority=%s local=%s"),
		*GetNameSafe(this),
		*GetNameSafe(MontageBeforeStop),
		bStopped ? TEXT("true") : TEXT("false"),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));
}

bool APdPlayer::StopInteractionMontageLocally(float BlendOutTime)
{
	UAnimMontage* MontageToStop = ActiveInteractionMontage.Get();
	if (!MontageToStop)
	{
		return false;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(MontageToStop))
	{
		ActiveInteractionMontage = nullptr;
		return false;
	}

	AnimInstance->Montage_Stop(BlendOutTime, MontageToStop);
	ActiveInteractionMontage = nullptr;
	return true;
}

/** PlayerStateÎ•??ÑÎ°ú?ùÌä∏ ?Ä?ÖÏúºÎ°?Î∞òÌôò?©Îãà?? */
APdPlayerState* APdPlayer::GetPdPlayerState() const
{
	return GetPlayerState<APdPlayerState>();
}

void APdPlayer::InitializePandoraTreeFromSave(AController* NewController)
{
	if (!HasAuthority())
	{
		return;
	}

	APdPlayerState* PdPlayerState = GetPdPlayerState();
	UPandoraTreeComponent* PandoraTreeComponent = PdPlayerState ? PdPlayerState->GetPandoraTreeComponent() : nullptr;
	UPandoraComponent* PandoraComponent = PdPlayerState ? PdPlayerState->GetPandoraComponent() : nullptr;
	if (!PandoraTreeComponent)
	{
		return;
	}

	CachedPlayerSaveId = GetPlayerSaveId(NewController);
	UPdGameInstance* PdGameInstance = GetWorld() ? Cast<UPdGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	PlayerSaveGameData = PdGameInstance ? PdGameInstance->GetOrCreateSaveGame(CachedPlayerSaveId) : nullptr;

	TArray<FGrantedPandora> SavedGrantedPandoras;
	int32 SavedPandoraPoints = -1;
	if (PdGameInstance && PlayerSaveGameData)
	{
		SavedPandoraPoints = PlayerSaveGameData->PlayerPandoraData.PandoraPoints;
		PdGameInstance->BuildGrantedPandorasFromNames(PlayerSaveGameData->PlayerPandoraData.GrantedPandorasByName, SavedGrantedPandoras);
		if (!PlayerSaveGameData->PlayerPandoraData.SelectedPandoraName.IsNone())
		{
			UE_LOG(PdPlayerLog, Log,
				TEXT("[PandoraSave] Clearing persisted selected pandora on startup. Selection is session-only now. player=%s selected=%s"),
				*CachedPlayerSaveId,
				*PlayerSaveGameData->PlayerPandoraData.SelectedPandoraName.ToString());

			FPlayerPandoraData SanitizedPandoraData = PlayerSaveGameData->PlayerPandoraData;
			SanitizedPandoraData.SelectedPandoraName = NAME_None;
			SavePlayerPandoraData(SanitizedPandoraData);
		}
		if (!PlayerSaveGameData->PlayerPandoraData.PandoraLoadoutByDirection.IsEmpty())
		{
			UE_LOG(PdPlayerLog, Log,
				TEXT("[PandoraSave] Clearing persisted pandora loadout on startup. Loadout slots are session-only now. player=%s slotCount=%d"),
				*CachedPlayerSaveId,
				PlayerSaveGameData->PlayerPandoraData.PandoraLoadoutByDirection.Num());

			FPlayerPandoraData SanitizedPandoraData = PlayerSaveGameData->PlayerPandoraData;
			SanitizedPandoraData.PandoraLoadoutByDirection.Reset();
			SavePlayerPandoraData(SanitizedPandoraData);
		}
	}

	if (BoundPandoraTreeComponent)
	{
		BoundPandoraTreeComponent->OnPandorasChanged.RemoveDynamic(this, &ThisClass::HandlePandoraTreePandorasChanged);
		BoundPandoraTreeComponent->OnPointsChanged.RemoveDynamic(this, &ThisClass::HandlePandoraTreePointsChanged);
	}

	if (BoundPandoraComponent)
	{
		BoundPandoraComponent->OnPandoraSelectionChanged.RemoveDynamic(this, &ThisClass::HandlePandoraSelectionChanged);
		BoundPandoraComponent->OnPandoraLoadoutChanged.RemoveDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);
	}

	BoundPandoraTreeComponent = PandoraTreeComponent;
	BoundPandoraTreeComponent->OnPandorasChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraTreePandorasChanged);
	BoundPandoraTreeComponent->OnPointsChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraTreePointsChanged);

	BoundPandoraComponent = PandoraComponent;
	if (BoundPandoraComponent)
	{
		BoundPandoraComponent->OnPandoraSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraSelectionChanged);
		BoundPandoraComponent->OnPandoraLoadoutChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraLoadoutChanged);
	}

	BoundPandoraTreeComponent->InitializePandoraTree(SavedGrantedPandoras, SavedPandoraPoints);

	// Pandora loadout slots are not restored from save; the UI starts empty until the player places them.
	// Current Pandora selection grants gameplay abilities, so it is intentionally not restored from save.
	// The player must select/equip a Pandora from the UI each session.
}

FString APdPlayer::GetPlayerSaveId(AController* InController) const
{
	return GetNameSafe(InController);
}

void APdPlayer::SavePlayerPandoraData(const FPlayerPandoraData& InPlayerPandoraData)
{
	if (!HasAuthority() || CachedPlayerSaveId.IsEmpty())
	{
		return;
	}

	UPdGameInstance* PdGameInstance = GetWorld() ? Cast<UPdGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (!PdGameInstance)
	{
		return;
	}

	if (!PlayerSaveGameData)
	{
		PlayerSaveGameData = PdGameInstance->GetOrCreateSaveGame(CachedPlayerSaveId);
	}

	if (!PlayerSaveGameData)
	{
		return;
	}

	PlayerSaveGameData->PlayerPandoraData = InPlayerPandoraData;
	PdGameInstance->SaveGame(CachedPlayerSaveId);
}

void APdPlayer::HandlePandoraTreePandorasChanged()
{
	if (!HasAuthority() || !BoundPandoraTreeComponent || !PlayerSaveGameData)
	{
		return;
	}

	FPlayerPandoraData PlayerPandoraData = PlayerSaveGameData->PlayerPandoraData;
	PlayerPandoraData.GrantedPandorasByName = BoundPandoraTreeComponent->GetGrantedPandoraLevelsByName();
	SavePlayerPandoraData(PlayerPandoraData);
}

void APdPlayer::HandlePandoraTreePointsChanged(int32 NewPointsAvailable)
{
	if (!HasAuthority() || !BoundPandoraTreeComponent || !PlayerSaveGameData)
	{
		return;
	}

	FPlayerPandoraData PlayerPandoraData = PlayerSaveGameData->PlayerPandoraData;
	PlayerPandoraData.PandoraPoints = NewPointsAvailable;
	SavePlayerPandoraData(PlayerPandoraData);
}

void APdPlayer::HandlePandoraSelectionChanged(UPandoraDefinition* NewPandoraDefinition)
{
	(void)NewPandoraDefinition;

	if (!HasAuthority() || !PlayerSaveGameData)
	{
		return;
	}

	if (!PlayerSaveGameData->PlayerPandoraData.SelectedPandoraName.IsNone())
	{
		FPlayerPandoraData PlayerPandoraData = PlayerSaveGameData->PlayerPandoraData;
		PlayerPandoraData.SelectedPandoraName = NAME_None;
		SavePlayerPandoraData(PlayerPandoraData);
	}
}

void APdPlayer::HandlePandoraLoadoutChanged()
{
	if (!HasAuthority() || !PlayerSaveGameData)
	{
		return;
	}

	if (!PlayerSaveGameData->PlayerPandoraData.PandoraLoadoutByDirection.IsEmpty())
	{
		FPlayerPandoraData PlayerPandoraData = PlayerSaveGameData->PlayerPandoraData;
		PlayerPandoraData.PandoraLoadoutByDirection.Reset();
		SavePlayerPandoraData(PlayerPandoraData);
	}
}

/** ?ÑÏû¨ ?ÅÌò∏?ëÏö© ?Ä?ÅÏù¥ ?àÎäîÏßÄ Î∞òÌôò?©Îãà?? */
bool APdPlayer::HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const
{
	OutCurrentInteractActors = CurrentInteractActors;
	UE_LOG(PdPlayerLog, Verbose, TEXT("[InteractActors] pawn=%s count=%d"),
		*GetNameSafe(this),
		OutCurrentInteractActors.Num());
	return OutCurrentInteractActors.Num() > 0;
}
AActor* APdPlayer::GetCurrentInteractActor() const
{
	for (const TScriptInterface<IInteractableInterface>& InteractableEntry : CurrentInteractActors)
	{
		AActor* InteractableActor = Cast<AActor>(InteractableEntry.GetObject());
		if (CanInteractWithActor(InteractableActor))
		{
			return InteractableActor;
		}
	}

	return nullptr;
}

bool APdPlayer::InteractWithCurrentTarget()
{
	AActor* InteractableActor = GetCurrentInteractActor();
	if (!IsValid(InteractableActor))
	{
		UE_LOG(PdPlayerLog, Warning,
			TEXT("[Interact] failed: no valid current target. pawn=%s trackedCount=%d"),
			*GetNameSafe(this),
			CurrentInteractActors.Num());
		return false;
	}

	if (!InteractableActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		UE_LOG(PdPlayerLog, Warning,
			TEXT("[Interact] failed: target no longer implements interface. pawn=%s target=%s class=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InteractableActor),
			*GetNameSafe(InteractableActor->GetClass()));
		return false;
	}

	if (!IInteractableInterface::Execute_CanInteract(InteractableActor, this))
	{
		UE_LOG(PdPlayerLog, Verbose,
			TEXT("[Interact] target rejected interaction. pawn=%s target=%s class=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InteractableActor),
			*GetNameSafe(InteractableActor->GetClass()));
		return false;
	}

	UE_LOG(PdPlayerLog, Verbose,
		TEXT("[Interact] executing target interface. pawn=%s target=%s class=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InteractableActor),
		*GetNameSafe(InteractableActor->GetClass()));

	return IInteractableInterface::Execute_Interact(InteractableActor, this);
}

void APdPlayer::SetWeaponAimActive(bool bEnabled, const FWeaponAimCameraSettings& AimCameraSettings)
{
	ApplyWeaponAimState(bEnabled, AimCameraSettings);

	if (!HasAuthority())
	{
		ServerSetWeaponAimActive(bEnabled, AimCameraSettings);
	}
}

void APdPlayer::SetAbilityCameraOverrideActive(bool bEnabled, const FWeaponAimCameraSettings& CameraSettings)
{
	if (!IsLocallyControlled())
	{
		UE_LOG(PdPlayerLog, Verbose,
			TEXT("[AbilityCamera] ignored non-local pawn=%s enabled=%s"),
			*GetNameSafe(this),
			bEnabled ? TEXT("true") : TEXT("false"));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AbilityCameraOverrideTimerHandle);
	}

	if (bEnabled)
	{
		ActiveAbilityCameraOverrideSettings = CameraSettings;
	}

	bAbilityCameraOverrideActive = bEnabled;

	UE_LOG(PdPlayerLog, Log,
		TEXT("[AbilityCamera] pawn=%s enabled=%s fov=%.1f offset=%s rotation=%s interp=%.1f currentFOV=%.1f currentOffset=%s currentRotation=%s"),
		*GetNameSafe(this),
		bAbilityCameraOverrideActive ? TEXT("true") : TEXT("false"),
		CameraSettings.TargetFOV,
		*CameraSettings.TargetBoomSocketOffset.ToCompactString(),
		*CameraSettings.TargetCameraRotation.ToCompactString(),
		CameraSettings.InterpSpeed,
		FollowCamera ? FollowCamera->FieldOfView : 0.0f,
		CameraBoom ? *CameraBoom->SocketOffset.ToCompactString() : TEXT("None"),
		FollowCamera ? *FollowCamera->GetRelativeRotation().ToCompactString() : TEXT("None"));
}

void APdPlayer::SetAbilityCameraOverrideActiveForDuration(bool bEnabled, const FWeaponAimCameraSettings& CameraSettings, float Duration)
{
	SetAbilityCameraOverrideActive(bEnabled, CameraSettings);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AbilityCameraOverrideTimerHandle);

		if (bEnabled && IsLocallyControlled() && Duration > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				AbilityCameraOverrideTimerHandle,
				this,
				&ThisClass::ClearAbilityCameraOverride,
				Duration,
				false);
		}
	}

	UE_LOG(PdPlayerLog, Warning,
		TEXT("[ProjectileCameraDebug] AbilityCamera duration pawn=%s enabled=%s local=%s duration=%.2f timerActive=%s"),
		*GetNameSafe(this),
		bEnabled ? TEXT("true") : TEXT("false"),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"),
		Duration,
		GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(AbilityCameraOverrideTimerHandle) ? TEXT("true") : TEXT("false"));
}

void APdPlayer::ClearAbilityCameraOverride()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AbilityCameraOverrideTimerHandle);
	}

	UE_LOG(PdPlayerLog, Warning,
		TEXT("[ProjectileCameraDebug] AbilityCamera duration expired pawn=%s local=%s"),
		*GetNameSafe(this),
		IsLocallyControlled() ? TEXT("true") : TEXT("false"));

	SetAbilityCameraOverrideActive(false, FWeaponAimCameraSettings());
}
void APdPlayer::ApplyWeaponAimState(bool bEnabled, const FWeaponAimCameraSettings& AimCameraSettings)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		return;
	}

	if (bEnabled)
	{
		ActiveWeaponAimCameraSettings = AimCameraSettings;
	}

	bWeaponAimCameraActive = bEnabled;

	if (bEnabled)
	{
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->bUseControllerDesiredRotation = true;
		MovementComponent->RotationRate = FRotator(0.0f, 3000.0f, 0.0f);
		bIsWeaponAimActive = true;
		return;
	}

	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->bUseControllerDesiredRotation = false;
	MovementComponent->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	bIsWeaponAimActive = false;
}

void APdPlayer::ServerSetWeaponAimActive_Implementation(bool bEnabled, FWeaponAimCameraSettings AimCameraSettings)
{
	ApplyWeaponAimState(bEnabled, AimCameraSettings);
}

void APdPlayer::UpdateWeaponAimCamera(float DeltaSeconds)
{
	if (!bHasCachedWeaponAimCameraDefaults || !FollowCamera || !CameraBoom)
	{
		return;
	}

	const FWeaponAimCameraSettings* DesiredCameraSettings = nullptr;
	if (bAbilityCameraOverrideActive)
	{
		DesiredCameraSettings = &ActiveAbilityCameraOverrideSettings;
	}
	else if (bWeaponAimCameraActive)
	{
		DesiredCameraSettings = &ActiveWeaponAimCameraSettings;
	}

	const float TargetFOV = DesiredCameraSettings ? DesiredCameraSettings->TargetFOV : DefaultCameraFOV;
	const FVector TargetSocketOffset = DesiredCameraSettings ? DesiredCameraSettings->TargetBoomSocketOffset : DefaultCameraBoomSocketOffset;
	const FRotator TargetCameraRotation = DesiredCameraSettings ? DesiredCameraSettings->TargetCameraRotation : DefaultFollowCameraRelativeRotation;
	const float CameraTransitionSpeed = DesiredCameraSettings ? DesiredCameraSettings->InterpSpeed : ActiveWeaponAimCameraSettings.InterpSpeed;

	if (CameraTransitionSpeed <= 0.0f)
	{
		FollowCamera->SetFieldOfView(TargetFOV);
		CameraBoom->SocketOffset = TargetSocketOffset;
		FollowCamera->SetRelativeRotation(TargetCameraRotation);
		return;
	}

	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaSeconds, CameraTransitionSpeed));
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocketOffset, DeltaSeconds, CameraTransitionSpeed);
	FollowCamera->SetRelativeRotation(
		FMath::RInterpTo(FollowCamera->GetRelativeRotation(), TargetCameraRotation, DeltaSeconds, CameraTransitionSpeed));
}

bool APdPlayer::GetWeaponAimViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	if (!FollowCamera)
	{
		if (Controller)
		{
			FRotator ViewRotation;
			Controller->GetPlayerViewPoint(OutLocation, ViewRotation);
			OutDirection = ViewRotation.Vector();
			return !OutDirection.IsNearlyZero();
		}

		OutLocation = GetActorLocation();
		OutDirection = GetActorForwardVector();
		return !OutDirection.IsNearlyZero();
	}

	OutLocation = FollowCamera->GetComponentLocation();
	OutDirection = FollowCamera->GetForwardVector();
	return !OutDirection.IsNearlyZero();
}

bool APdPlayer::CanInteractWithActor(AActor* InteractableActor) const
{
	TScriptInterface<IInteractableInterface> InteractableEntry;
	if (!TryMakeInteractableEntry(InteractableActor, InteractableEntry))
	{
		UE_LOG(PdPlayerLog, Warning, TEXT("[InteractValidation] rejected: actor invalid or not interactable pawn=%s actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InteractableActor));
		return false;
	}


	if (IsValid(InteractionBox) && InteractionBox->IsOverlappingActor(InteractableActor))
	{
		UE_LOG(PdPlayerLog, Log, TEXT("[InteractValidation] accepted by overlap pawn=%s actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InteractableActor));
		return true;
	}

	if (InteractionServerValidationDistance <= 0.0f)
	{
		UE_LOG(PdPlayerLog, Warning, TEXT("[InteractValidation] rejected: distance fallback disabled pawn=%s actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(InteractableActor));
		return false;
	}

	const float DistanceSquared = FVector::DistSquared(GetActorLocation(), InteractableActor->GetActorLocation());
	const bool bWithinDistance = DistanceSquared <= FMath::Square(InteractionServerValidationDistance);
	UE_LOG(PdPlayerLog, Log, TEXT("[InteractValidation] distance fallback pawn=%s actor=%s distance=%.1f max=%.1f result=%s"),
		*GetNameSafe(this),
		*GetNameSafe(InteractableActor),
		FMath::Sqrt(DistanceSquared),
		InteractionServerValidationDistance,
		bWithinDistance ? TEXT("true") : TEXT("false"));
	return bWithinDistance;
}

/** ?°ÌÑ∞Î•??ÅÌò∏?ëÏö© ?îÌä∏Î¶¨Î°ú Î≥Ä?òÌï©?àÎã§. */
bool APdPlayer::TryMakeInteractableEntry(AActor* OtherActor, TScriptInterface<IInteractableInterface>& OutInteractableActor) const
{
	// =================================================================================================================
	// === ?†Ìö®??Í≤Ä??

	if (!IsValid(OtherActor) || OtherActor == this || !OtherActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return false;
	}

	// =================================================================================================================
	// === ?∏ÌÑ∞?òÏù¥???îÌä∏Î¶?Íµ¨ÏÑ±

	OutInteractableActor.SetObject(OtherActor);
	OutInteractableActor.SetInterface(Cast<IInteractableInterface>(OtherActor));
	return true;
}

/** ?ÅÌò∏?ëÏö© Î∞ïÏä§ ÏßÑÏûÖ??Ï≤òÎ¶¨?©Îãà?? */
void APdPlayer::HandleInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	// =================================================================================================================
	// === ?ÅÌò∏?ëÏö© ?îÌä∏Î¶??ùÏÑ±

	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		UE_LOG(PdPlayerLog, Verbose, TEXT("[InteractActors] begin overlap ignored pawn=%s actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OtherActor));
		return;
	}

	// =================================================================================================================
	// === Ï§ëÎ≥µ Ï∂îÏ†Å Î∞©Ï?

	const bool bAlreadyTracked = CurrentInteractActors.ContainsByPredicate(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});

	if (bAlreadyTracked)
	{
		UE_LOG(PdPlayerLog, Verbose, TEXT("[InteractActors] begin overlap duplicate pawn=%s actor=%s count=%d"),
			*GetNameSafe(this),
			*GetNameSafe(OtherActor),
			CurrentInteractActors.Num());
		return;
	}

	// =================================================================================================================
	// === Î™©Î°ù Ï∂îÍ?

	CurrentInteractActors.Add(InteractableActor);
	UE_LOG(PdPlayerLog, Verbose, TEXT("[InteractActors] begin overlap added pawn=%s actor=%s count=%d"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		CurrentInteractActors.Num());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// ?îÎ≤ÑÍ∑??àÎÇ¥ Î¨∏Íµ¨?ÖÎãà??
	UKismetSystemLibrary::PrintString(this, TEXT("Interact (X)"), true, true, FLinearColor(0.0f, 0.66f, 1.0f), 2.0f);
#endif
}

/** ?ÅÌò∏?ëÏö© Î∞ïÏä§ ?¥ÌÉà??Ï≤òÎ¶¨?©Îãà?? */
void APdPlayer::HandleInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);

	// =================================================================================================================
	// === ?ÅÌò∏?ëÏö© ?îÌä∏Î¶??ïÏù∏

	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		UE_LOG(PdPlayerLog, Verbose, TEXT("[InteractActors] end overlap ignored pawn=%s actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OtherActor));
		return;
	}

	// =================================================================================================================
	// === Î™©Î°ù ?úÍ±∞

	CurrentInteractActors.RemoveAll(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});
	UE_LOG(PdPlayerLog, Verbose, TEXT("[InteractActors] end overlap removed pawn=%s actor=%s count=%d"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		CurrentInteractActors.Num());
}
