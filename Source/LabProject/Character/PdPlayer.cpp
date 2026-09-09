#include "Character/PdPlayer.h"

#include "AbilitySystemComponent.h"
#include "CableComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"
#include "Mode/PdPlayerState.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/GrappleComponent.h"
#include "Component/Player/PaintCanvasComponent.h"
#include "Component/Player/PlayerActionComponent.h"
#include "Component/Player/PlayerAimComponent.h"
#include "Component/Player/PlayerCameraComponent.h"
#include "Component/Player/PlayerInteractionComponent.h"
#include "Component/Player/PlayerLoadoutComponent.h"
#include "Definition/Player/PlayerPawnDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayer)

DEFINE_LOG_CATEGORY_STATIC(LogPdPlayer, Log, All);

namespace
{
	const FName GrappleHookComponentName(TEXT("Hook"));
	const FName GrappleHookSocketName(TEXT("hand_r"));
}

// 플레이어가 사용할 전투·상호작용·조준·그래플 기능과 카메라·말풍선의 기본 구성을 만든다.
APdPlayer::APdPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerInteractionComponent =
		CreateDefaultSubobject<UPlayerInteractionComponent>(TEXT("InteractionBox"));
	PlayerInteractionComponent->SetupAttachment(GetRootComponent());
	InteractionBox = PlayerInteractionComponent;

	GrappleComponent = CreateDefaultSubobject<UGrappleComponent>(TEXT("GrappleComponent"));
	PaintCanvasComponent = CreateDefaultSubobject<UPaintCanvasComponent>(TEXT("PaintCanvasComponent"));
	PlayerActionComponent = CreateDefaultSubobject<UPlayerActionComponent>(TEXT("PlayerActionComponent"));
	PlayerAimComponent = CreateDefaultSubobject<UPlayerAimComponent>(TEXT("PlayerAimComponent"));
	PlayerCameraComponent = CreateDefaultSubobject<UPlayerCameraComponent>(TEXT("PlayerCameraComponent"));
	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = false;

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

	SpeechBubblePlaneComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpeechBubble"));
	SpeechBubblePlaneComponent->SetupAttachment(GetRootComponent());
	SpeechBubblePlaneComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpeechBubblePlaneComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SpeechBubblePlaneComponent->SetGenerateOverlapEvents(false);
	SpeechBubblePlaneComponent->SetCanEverAffectNavigation(false);
	SpeechBubblePlaneComponent->SetHiddenInGame(true);
	SpeechBubblePlaneComponent->SetVisibility(true, true);
}

// 월드에 들어올 플레이어의 조작·카메라 설정을 미리 요청하고, 공통 캐릭터의 준비 조건에 포함한다.
void APdPlayer::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	BeginPlayerPawnDefinitionPreload();
}

// 플레이어가 월드를 떠나거나 제거될 때 대기 중인 설정 로딩과 로컬 카메라 연출을 정리한다.
void APdPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleasePlayerPawnDefinitionPreload();
	bPlayerPawnDefinitionReady = false;
	if (PlayerCameraComponent)
	{
		PlayerCameraComponent->ShutdownCamera();
	}
	Super::EndPlay(EndPlayReason);
}

// 생성이 끝난 그래플 케이블과 말풍선 메시를 각각의 동작을 담당하는 컴포넌트에 연결한다.
void APdPlayer::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GrappleComponent)
	{
		GrappleComponent->SetHookComponent(HookComponent);
	}
	if (PaintCanvasComponent)
	{
		PaintCanvasComponent->SetSpeechBubbleComponent(SpeechBubblePlaneComponent);
	}
}

// 서버에서 조종자가 배정되면 회전 방식을 맞추고, 준비된 캐릭터에 선택 장비를 적용하며 조준값 복제를 시작한다.
void APdPlayer::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ReapplyCurrentRotationPolicy();
	if (IsCharacterRuntimeInitialized())
	{
		ApplySelectedPlayerLoadout();
		if (PlayerAimComponent)
		{
			PlayerAimComponent->StartReplication();
		}
	}
}

// 조종을 넘기거나 해제할 때 조준값 전송과 기존 플레이어용 카메라 연출을 중단한다.
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

// 조종자 변경에 맞춰 회전 방식을 갱신하고, 더 이상 로컬 플레이어가 아니면 카메라 연출을 정리한다.
void APdPlayer::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	ReapplyCurrentRotationPolicy();
	if (!IsLocallyControlled() && PlayerCameraComponent)
	{
		PlayerCameraComponent->ShutdownCamera();
	}
}

// 로컬 플레이어의 상체 조준 방향과 카메라 보간·가림 처리를 매 프레임 갱신한다.
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

// 지정된 플레이어 설정이나 기본 정의를 준비하며, 아직 없는 에셋은 게임 진행을 막지 않고 비동기로 읽는다.
void APdPlayer::BeginPlayerPawnDefinitionPreload()
{
	ReleasePlayerPawnDefinitionPreload();
	bPlayerPawnDefinitionReady = false;

	UAssetManager& AssetManager = UAssetManager::Get();
	const FSoftObjectPath DefinitionPath = PlayerPawnDefinition
		? FSoftObjectPath(PlayerPawnDefinition.Get())
		: AssetManager.GetPrimaryAssetPath(UPlayerPawnDefinition::GetDefaultPrimaryAssetId());
	const uint32 RequestGeneration = PlayerPawnDefinitionLoadGeneration;
	if (DefinitionPath.IsNull() || DefinitionPath.ResolveObject())
	{
		HandlePlayerPawnDefinitionPreloaded(DefinitionPath, RequestGeneration);
		return;
	}

	TSharedPtr<FStreamableHandle> NewHandle = AssetManager.GetStreamableManager().RequestAsyncLoad(
		DefinitionPath, FStreamableDelegate::CreateWeakLambda(this, [this, DefinitionPath, RequestGeneration]()
		{
			HandlePlayerPawnDefinitionPreloaded(DefinitionPath, RequestGeneration);
		}));
	if (NewHandle.IsValid() && RequestGeneration == PlayerPawnDefinitionLoadGeneration && !bPlayerPawnDefinitionReady)
	{
		PlayerPawnDefinitionLoadHandle = MoveTemp(NewHandle);
	}
	else if (NewHandle.IsValid())
	{
		NewHandle->ReleaseHandle();
	}
	else
	{
		HandlePlayerPawnDefinitionPreloaded(DefinitionPath, RequestGeneration);
	}
}

// 최신 로딩 결과의 플레이어 설정을 적용하고 공통 초기화를 이어 간다. 로딩 실패 시에는 기본값을 사용한다.
void APdPlayer::HandlePlayerPawnDefinitionPreloaded(FSoftObjectPath DefinitionPath, uint32 RequestGeneration)
{
	if (RequestGeneration != PlayerPawnDefinitionLoadGeneration)
	{
		return;
	}

	PlayerPawnDefinition = Cast<UPlayerPawnDefinition>(DefinitionPath.ResolveObject());
	if (!PlayerPawnDefinition)
	{
		UE_LOG(LogPdPlayer, Error, TEXT("Player pawn definition '%s' could not be loaded; native defaults will be used."),
			*UPlayerPawnDefinition::GetDefaultPrimaryAssetId().ToString());
	}
	ApplyPlayerPawnDefinition();
	bPlayerPawnDefinitionReady = true;
	TryInitializeCharacterRuntime();
}

// 플레이어 설정 로딩을 취소하고 이전 완료 콜백을 무효화해, 종료되거나 재준비 중인 캐릭터를 갱신하지 못하게 한다.
void APdPlayer::ReleasePlayerPawnDefinitionPreload()
{
	++PlayerPawnDefinitionLoadGeneration;
	if (PlayerPawnDefinitionLoadHandle.IsValid())
	{
		PlayerPawnDefinitionLoadHandle->CancelHandle();
		PlayerPawnDefinitionLoadHandle->ReleaseHandle();
		PlayerPawnDefinitionLoadHandle.Reset();
	}
}

// 플레이어 전용 조작·카메라 설정까지 준비되어 공통 초기화를 진행해도 되는지 판단한다.
bool APdPlayer::IsAdditionalCharacterRuntimeContentReady() const
{
	return Super::IsAdditionalCharacterRuntimeContentReady() && bPlayerPawnDefinitionReady;
}

// 공통 외형·ASC 초기화 요청 이후 선택 장비와 조준값 복제를 연결해, 장착 애니메이션이 기본 레이어로 덮이지 않게 한다.
void APdPlayer::HandleCharacterRuntimeInitialized()
{
	Super::HandleCharacterRuntimeInitialized();
	ApplySelectedPlayerLoadout();
	if (PlayerAimComponent)
	{
		PlayerAimComponent->StartReplication();
	}
}

// PlayerState에 보관된 현재 무기·판도라 선택을 이 캐릭터에 적용하도록 로드아웃 컴포넌트에 요청한다.
void APdPlayer::ApplySelectedPlayerLoadout()
{
	if (APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>())
	{
		if (UPlayerLoadoutComponent* LoadoutComponent = PdPlayerState->GetPlayerLoadoutComponent())
		{
			LoadoutComponent->ApplySelectedLoadout();
		}
	}
}

// 플레이어 정의의 상호작용 거리·카메라·조준·행동·맨손 전투 설정을 각 담당 컴포넌트에 적용한다.
void APdPlayer::ApplyPlayerPawnDefinition()
{
	FPlayerInteractionSettings InteractionSettings;

	FPlayerCameraPresentationSettings CameraSettings;
	FPlayerAimSettings AimSettings;

	if (PlayerPawnDefinition)
	{
		InteractionSettings = PlayerPawnDefinition->GetInteractionSettings();
		CameraSettings = PlayerPawnDefinition->GetCameraSettings();
		AimSettings = PlayerPawnDefinition->GetAimSettings();
	}

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
	if (CombatComponent)
	{
		CombatComponent->ApplySettings(PlayerPawnDefinition->GetCombatDamageSettings(), PlayerPawnDefinition->GetUnarmedCombatSettings());
	}

	ReapplyCurrentRotationPolicy();
}

// 능력과 쿨다운을 관리하는 현재 PlayerState의 ASC를 제공한다. 플레이어 캐릭터 자체에는 별도 ASC를 만들지 않는다.
UAbilitySystemComponent* APdPlayer::GetAbilitySystemComponent() const
{
	if (const APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>())
	{
		return PdPlayerState->GetAbilitySystemComponent();
	}

	return nullptr;
}

// 플레이어가 죽으면 공격·조준·그래플을 중단한 뒤 공통 캐릭터의 사망 처리를 실행한다.
void APdPlayer::HandleDeath_Implementation()
{
	if (CombatComponent)
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

// 재사용하는 플레이어 캐릭터를 부활시키고, 남아 있는 그래플 상태와 이동·조준 회전 설정을 복구한다.
void APdPlayer::ResetDeathStateForRespawn()
{
	Super::ResetDeathStateForRespawn();

	// 메시 복구가 자식의 가시성까지 바꾸므로, 그 뒤에 케이블을 숨겨 재사용 캐릭터에 남지 않게 한다.
	if (GrappleComponent)
	{
		GrappleComponent->ResetForRespawn();
	}

	ReapplyCurrentRotationPolicy();
}

// 플레이어의 능력 데이터 소유자를 일시적인 캐릭터가 아니라 PlayerState로 지정한다.
AActor* APdPlayer::GetAbilitySystemOwnerActor() const
{
	return GetPlayerState<APdPlayerState>();
}

// 카메라와 조준을 계속 갱신해야 하는 로컬 플레이어에게만 상시 캐릭터 Tick을 요청한다.
bool APdPlayer::ShouldUseContinuousCharacterTick() const
{
	return IsLocallyControlled();
}

// 입력이나 다른 행동이 그래플 이동 중인지 판단할 수 있도록 현재 그래플 상태를 제공한다.
bool APdPlayer::IsGrappling() const
{
	return GrappleComponent && GrappleComponent->IsGrappling();
}

// 이동을 재개할 때 진행 중인 피격 반응 능력과 몽타주를 해제하도록 행동 컴포넌트에 요청한다.
bool APdPlayer::RequestCancelHitReactForMovement(const float BlendOutTime)
{
	return PlayerActionComponent
		&& PlayerActionComponent->RequestCancelHitReactForMovement(BlendOutTime);
}

// 그림 입력 화면의 사용 상태를 종료하도록 페인트 컴포넌트에 요청한다. 이미 표시한 말풍선은 별도로 유지된다.
void APdPlayer::HidePaintCanvas()
{
	if (PaintCanvasComponent)
	{
		PaintCanvasComponent->HidePaintCanvas();
	}
}

// 그림을 입력 중이거나 그린 그림을 말풍선으로 표시 중인지 확인한다.
bool APdPlayer::HasActivePaintCanvas() const
{
	return PaintCanvasComponent ? PaintCanvasComponent->HasActivePaintCanvas() : false;
}

// 현재 그림을 캐릭터의 말풍선으로 표시하고 다른 플레이어에게 공유하도록 요청한다.
bool APdPlayer::ExportActivePaintCanvasToSpeechBubble()
{
	return PaintCanvasComponent
		&& PaintCanvasComponent->ExportActivePaintCanvasToSpeechBubble();
}

// 현재 그린 그림을 지정한 얼굴 소켓의 데칼로 적용하도록 페인트 컴포넌트에 요청한다.
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

// 로비에서 저장한 얼굴 그림을 현재 플레이어 캐릭터에 다시 적용하도록 요청한다.
void APdPlayer::RestoreCachedLobbyPaintCanvasFaceDecal()
{
	if (PaintCanvasComponent)
	{
		PaintCanvasComponent->RestoreCachedLobbyPaintCanvasFaceDecal();
	}
}

// 상자 열기 등 상호작용에 사용할 몽타주를 재생하고 필요한 네트워크 처리를 상호작용 컴포넌트에 맡긴다.
void APdPlayer::PlayInteractionMontage(UAnimMontage* Montage, float PlayRate)
{
	if (PlayerInteractionComponent)
	{
		PlayerInteractionComponent->PlayInteractionMontage(Montage, PlayRate);
	}
}

// 상호작용이 끝나거나 취소될 때 재생 중인 전용 몽타주를 지정한 시간에 걸쳐 중단하도록 요청한다.
void APdPlayer::StopInteractionMontage(float BlendOutTime)
{
	if (PlayerInteractionComponent)
	{
		PlayerInteractionComponent->StopInteractionMontage(BlendOutTime);
	}
}

// 주변에 추적 중인 대상 가운데 센서 겹침 또는 허용 거리 조건을 만족하는 첫 대상을 찾는다.
AActor* APdPlayer::GetCurrentInteractActor() const
{
	return PlayerInteractionComponent
		? PlayerInteractionComponent->GetCurrentInteractActor()
		: nullptr;
}

// 현재 대상이 상호작용을 허용하면 대상의 상호작용 기능을 실행하도록 요청한다.
bool APdPlayer::InteractWithCurrentTarget()
{
	return PlayerInteractionComponent
		&& PlayerInteractionComponent->InteractWithCurrentTarget();
}

// 무기 조준의 시작·종료를 조준 컴포넌트에 전달해 자세·회전 방식과 조준용 카메라를 전환한다.
void APdPlayer::SetWeaponAimActive(bool bEnabled, const FWeaponAimCameraSettings& AimCameraSettings)
{
	if (PlayerAimComponent)
	{
		PlayerAimComponent->SetWeaponAimActive(bEnabled, AimCameraSettings);
	}
}

// 공격이나 이동 처리에서 플레이어가 현재 무기 조준 상태인지 확인할 수 있게 한다.
bool APdPlayer::IsWeaponAimActive() const
{
	return PlayerAimComponent && PlayerAimComponent->IsWeaponAimActive();
}

// 스킬 연출 중 사용할 카메라 설정을 적용하거나 해제하도록 카메라 컴포넌트에 요청한다.
void APdPlayer::SetAbilityCameraOverrideActive(bool bEnabled, const FWeaponAimCameraSettings& CameraSettings)
{
	if (!PlayerCameraComponent)
	{
		return;
	}

	PlayerCameraComponent->SetAbilityCameraOverrideActive(bEnabled, CameraSettings);
}

// 스킬 전용 카메라 연출을 정해진 시간만 유지한 뒤 해제하도록 요청한다.
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

// 현재 조준 여부에 맞는 캐릭터 회전 방식을 적용하며, 조준 컴포넌트가 없으면 이동 방향을 바라보게 한다.
void APdPlayer::ApplyCurrentRotationPolicy(
	UCharacterMovementComponent* MovementComponent)
{
	if (!MovementComponent)
	{
		return;
	}

	if (PlayerAimComponent)
	{
		PlayerAimComponent->ApplyMovementSettings(MovementComponent);
		return;
	}

	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->bUseControllerDesiredRotation = false;
	MovementComponent->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	bUseControllerRotationYaw = false;
}

// 조준 판정에 사용할 카메라 기준 시작 위치와 바라보는 방향을 제공한다.
bool APdPlayer::GetWeaponAimViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	return PlayerCameraComponent
		&& PlayerCameraComponent->GetAimViewPoint(OutLocation, OutDirection);
}

// 대상이 상호작용 인터페이스를 가지며 센서 안이나 허용 거리 내에 있는지 확인한다. 대상 자체의 허용 조건은 검사하지 않는다.
bool APdPlayer::CanInteractWithActor(AActor* InteractableActor) const
{
	return PlayerInteractionComponent
		&& PlayerInteractionComponent->CanInteractWithActor(InteractableActor);
}
