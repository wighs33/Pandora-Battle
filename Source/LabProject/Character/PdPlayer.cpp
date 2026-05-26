#include "Character/PdPlayer.h"

#include "Camera/CameraComponent.h"
#include "Common/Enum_Direction.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
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
	FName GetPandoraDefinitionSaveName(const UPandoraDefinition* PandoraDefinition)
	{
		return IsValid(PandoraDefinition) ? PandoraDefinition->GetFName() : NAME_None;
	}

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
/** ?Œë ˆ?´ì–´ ê¸°ë³¸ ?íƒœë¥?ì´ˆê¸°?”í•©?ˆë‹¤. */
APdPlayer::APdPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================
	// === ?í˜¸?‘ìš© ë°•ìŠ¤ ?¤ì •

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(GetRootComponent());
	InteractionBox->SetBoxExtent(FVector(50.0f, 50.0f, 100.0f));
	InteractionBox->SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));
	InteractionBox->SetRelativeRotation(FRotator::ZeroRotator);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionBox->SetGenerateOverlapEvents(true);
	InteractionBox->SetCanEverAffectNavigation(false);
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &APdPlayer::HandleInteractionBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &APdPlayer::HandleInteractionBoxEndOverlap);

	// =================================================================================================================
	// === ì¹´ë©”??ë¶??¤ì •

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// =================================================================================================================
	// === ì¶”ì  ì¹´ë©”???¤ì •

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void APdPlayer::BeginPlay()
{
	Super::BeginPlay();

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

/** PlayerState ê¸°ì? ASCë¥?ë°˜í™˜?©ë‹ˆ?? */
UAbilitySystemComponent* APdPlayer::GetAbilitySystemComponent() const
{
	if (const APdPlayerState* PdPlayerState = GetPdPlayerState())
	{
		return PdPlayerState->GetAbilitySystemComponent();
	}

	return nullptr;
}

/** ASC ?Œìœ  ?¡í„°ë¥?ë°˜í™˜?©ë‹ˆ?? */
AActor* APdPlayer::GetAbilitySystemOwnerActor() const
{
	return GetPdPlayerState();
}

/** PlayerStateë¥??„ë¡œ?íŠ¸ ?€?…ìœ¼ë¡?ë°˜í™˜?©ë‹ˆ?? */
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
	FName SavedSelectedPandoraName = NAME_None;
	TMap<FName, FName> SavedPandoraLoadoutByDirection;
	if (PdGameInstance && PlayerSaveGameData)
	{
		SavedPandoraPoints = PlayerSaveGameData->PlayerPandoraData.PandoraPoints;
		SavedSelectedPandoraName = PlayerSaveGameData->PlayerPandoraData.SelectedPandoraName;
		SavedPandoraLoadoutByDirection = PlayerSaveGameData->PlayerPandoraData.PandoraLoadoutByDirection;
		PdGameInstance->BuildGrantedPandorasFromNames(PlayerSaveGameData->PlayerPandoraData.GrantedPandorasByName, SavedGrantedPandoras);
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

	if (BoundPandoraComponent && PdGameInstance && !SavedPandoraLoadoutByDirection.IsEmpty())
	{
		for (const TPair<FName, FName>& SavedLoadoutSlot : SavedPandoraLoadoutByDirection)
		{
			EEnum_Direction Direction = EEnum_Direction::Center;
			if (!TryGetPandoraLoadoutDirectionFromSaveName(SavedLoadoutSlot.Key, Direction))
			{
				continue;
			}

			UPandoraDefinition* SavedLoadoutPandora = PdGameInstance->GetPandoraDefinitionByName(SavedLoadoutSlot.Value);
			if (SavedLoadoutPandora)
			{
				BoundPandoraComponent->RestorePandoraLoadoutSlot(Direction, SavedLoadoutPandora);
			}
		}
	}

	if (BoundPandoraComponent && PdGameInstance && !SavedSelectedPandoraName.IsNone())
	{
		UPandoraDefinition* SavedSelectedPandora = PdGameInstance->GetPandoraDefinitionByName(SavedSelectedPandoraName);
		if (SavedSelectedPandora)
		{
			bRestoringPandoraSelectionFromSave = true;
			BoundPandoraComponent->RequestPandoraSelection(SavedSelectedPandora);

			TWeakObjectPtr<APdPlayer> WeakThis = this;
			TWeakObjectPtr<UPandoraComponent> WeakPandoraComponent = BoundPandoraComponent;
			TWeakObjectPtr<UPandoraDefinition> WeakSavedSelectedPandora = SavedSelectedPandora;
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [WeakThis, WeakPandoraComponent, WeakSavedSelectedPandora]()
				{
					if (UPandoraComponent* Component = WeakPandoraComponent.Get())
					{
						Component->RequestPandoraSelection(WeakSavedSelectedPandora.Get());
					}

					if (APdPlayer* Player = WeakThis.Get())
					{
						Player->bRestoringPandoraSelectionFromSave = false;
					}
				}));
			}
			else
			{
				bRestoringPandoraSelectionFromSave = false;
			}
		}
	}
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
	if (!HasAuthority() || !PlayerSaveGameData || bRestoringPandoraSelectionFromSave)
	{
		return;
	}

	FPlayerPandoraData PlayerPandoraData = PlayerSaveGameData->PlayerPandoraData;
	PlayerPandoraData.SelectedPandoraName = GetPandoraDefinitionSaveName(NewPandoraDefinition);
	SavePlayerPandoraData(PlayerPandoraData);
}

void APdPlayer::HandlePandoraLoadoutChanged()
{
	if (!HasAuthority() || !BoundPandoraComponent || !PlayerSaveGameData)
	{
		return;
	}

	FPlayerPandoraData PlayerPandoraData = PlayerSaveGameData->PlayerPandoraData;
	PlayerPandoraData.PandoraLoadoutByDirection = BoundPandoraComponent->GetPandoraLoadoutSaveNames();
	SavePlayerPandoraData(PlayerPandoraData);
}
/** ?„ì¬ ?í˜¸?‘ìš© ?€?ì´ ?ˆëŠ”ì§€ ë°˜í™˜?©ë‹ˆ?? */
bool APdPlayer::HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const
{
	OutCurrentInteractActors = CurrentInteractActors;
	UE_LOG(PdPlayerLog, Log, TEXT("[InteractActors] pawn=%s count=%d"),
		*GetNameSafe(this),
		OutCurrentInteractActors.Num());
	return OutCurrentInteractActors.Num() > 0;
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

/** ?¡í„°ë¥??í˜¸?‘ìš© ?”íŠ¸ë¦¬ë¡œ ë³€?˜í•©?ˆë‹¤. */
bool APdPlayer::TryMakeInteractableEntry(AActor* OtherActor, TScriptInterface<IInteractableInterface>& OutInteractableActor) const
{
	// =================================================================================================================
	// === ? íš¨??ê²€??

	if (!IsValid(OtherActor) || OtherActor == this || !OtherActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return false;
	}

	// =================================================================================================================
	// === ?¸í„°?˜ì´???”íŠ¸ë¦?êµ¬ì„±

	OutInteractableActor.SetObject(OtherActor);
	OutInteractableActor.SetInterface(Cast<IInteractableInterface>(OtherActor));
	return true;
}

/** ?í˜¸?‘ìš© ë°•ìŠ¤ ì§„ì…??ì²˜ë¦¬?©ë‹ˆ?? */
void APdPlayer::HandleInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	// =================================================================================================================
	// === ?í˜¸?‘ìš© ?”íŠ¸ë¦??ì„±

	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		UE_LOG(PdPlayerLog, Log, TEXT("[InteractActors] begin overlap ignored pawn=%s actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OtherActor));
		return;
	}

	// =================================================================================================================
	// === ì¤‘ë³µ ì¶”ì  ë°©ì?

	const bool bAlreadyTracked = CurrentInteractActors.ContainsByPredicate(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});

	if (bAlreadyTracked)
	{
		UE_LOG(PdPlayerLog, Log, TEXT("[InteractActors] begin overlap duplicate pawn=%s actor=%s count=%d"),
			*GetNameSafe(this),
			*GetNameSafe(OtherActor),
			CurrentInteractActors.Num());
		return;
	}

	// =================================================================================================================
	// === ëª©ë¡ ì¶”ê?

	CurrentInteractActors.Add(InteractableActor);
	UE_LOG(PdPlayerLog, Log, TEXT("[InteractActors] begin overlap added pawn=%s actor=%s count=%d"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		CurrentInteractActors.Num());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// ?”ë²„ê·??ˆë‚´ ë¬¸êµ¬?…ë‹ˆ??
	UKismetSystemLibrary::PrintString(this, TEXT("Interact (X)"), true, true, FLinearColor(0.0f, 0.66f, 1.0f), 2.0f);
#endif
}

/** ?í˜¸?‘ìš© ë°•ìŠ¤ ?´íƒˆ??ì²˜ë¦¬?©ë‹ˆ?? */
void APdPlayer::HandleInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);

	// =================================================================================================================
	// === ?í˜¸?‘ìš© ?”íŠ¸ë¦??•ì¸

	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		UE_LOG(PdPlayerLog, Log, TEXT("[InteractActors] end overlap ignored pawn=%s actor=%s"),
			*GetNameSafe(this),
			*GetNameSafe(OtherActor));
		return;
	}

	// =================================================================================================================
	// === ëª©ë¡ ?œê±°

	CurrentInteractActors.RemoveAll(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});
	UE_LOG(PdPlayerLog, Log, TEXT("[InteractActors] end overlap removed pawn=%s actor=%s count=%d"),
		*GetNameSafe(this),
		*GetNameSafe(OtherActor),
		CurrentInteractActors.Num());
}
