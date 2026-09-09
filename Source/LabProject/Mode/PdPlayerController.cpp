#include "Mode/PdPlayerController.h"

#include "Mode/PdPlayerState.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"

#include "Component/Chat/ChatControllerComponent.h"
#include "Component/Player/ControllerInputComponent.h"
#include "Component/Player/ControllerPresentationComponent.h"
#include "Component/Player/ControllerProfileSyncComponent.h"
#include "Component/Player/ControllerSessionComponent.h"
#include "Component/Player/PlayerNotificationComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Definition/Player/PlayerControllerDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PawnMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayerController)

DEFINE_LOG_CATEGORY(PdPlayerControllerLog);

// 플레이어가 항상 사용하는 기능 컴포넌트와 기본 설정 에셋 경로를 구성한다.
APdPlayerController::APdPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ControllerInputComponent =
		CreateDefaultSubobject<UControllerInputComponent>(TEXT("ControllerInputComponent"));
	ChatControllerComponent =
		CreateDefaultSubobject<UChatControllerComponent>(TEXT("ChatControllerComponent"));
	ControllerPresentationComponent =
		CreateDefaultSubobject<UControllerPresentationComponent>(TEXT("ControllerPresentationComponent"));
	ControllerProfileSyncComponent =
		CreateDefaultSubobject<UControllerProfileSyncComponent>(TEXT("ControllerProfileSyncComponent"));
	ControllerSessionComponent =
		CreateDefaultSubobject<UControllerSessionComponent>(TEXT("ControllerSessionComponent"));
	NotificationComponent = CreateDefaultSubobject<UPlayerNotificationComponent>(TEXT("NotificationComponent"));
	PlayerControllerDefinition = TSoftObjectPtr<UPlayerControllerDefinition>(
		UPlayerControllerDefinition::GetDefaultDefinitionPath());
}

// 기본 화면 설정을 적용하고, 게임피처 수신 등록과 설정 에셋의 비동기 로딩을 시작한다.
void APdPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	ApplyControllerDefinition();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
	BeginControllerDefinitionPreload();
}

// 로컬 입력·화면을 준비하고 서버와 소유 클라이언트의 외형 프로필 동기화를 시작한다.
void APdPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
		this,
		UGameFrameworkComponentManager::NAME_GameActorReady);
	RefreshControllerInput();

	if (IsLocalController())
	{
		if (ControllerPresentationComponent)
		{
			ControllerPresentationComponent->InitializeLocalPresentation();
		}
	}
	if (ControllerProfileSyncComponent
		&& (HasAuthority() || IsLocalController()))
	{
		ControllerProfileSyncComponent->ScheduleLocalCosmeticProfileSync();
	}
}

// 설정 로딩과 게임피처 수신을 종료한다. 각 컴포넌트의 정리는 엔진이 호출하는 EndPlay에 맡긴다.
void APdPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseControllerDefinitionPreload();
	LoadedPlayerControllerDefinition = nullptr;

	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

// 엔진 입력 컴포넌트가 준비되면 기본 입력 설정을 선택하고 바인딩을 갱신한다.
void APdPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	RefreshControllerInput();
}

// 입력 수집 이후에 능력 입력을 처리해, 같은 프레임의 짧은 누름·해제도 순서를 보장한다.
void APdPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (!bGamePaused)
	{
		if (const APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>())
		{
			if (UPdAbilitySystemComponent* AbilitySystem = Cast<UPdAbilitySystemComponent>(PdPlayerState->GetAbilitySystemComponent()))
			{
				AbilitySystem->ProcessAbilityInput();
			}
		}
	}
	Super::PostProcessInput(DeltaTime, bGamePaused);
}

// 소유 클라이언트가 조종할 Pawn을 확인하면 입력·화면·외형 프로필을 갱신한다.
void APdPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	RefreshControllerInput();

	if (IsLocalController())
	{
		if (ControllerPresentationComponent)
		{
			ControllerPresentationComponent->RefreshAfterPossession(P);
		}
	}
	if (ControllerProfileSyncComponent
		&& (HasAuthority() || IsLocalController()))
	{
		ControllerProfileSyncComponent->ScheduleLocalCosmeticProfileSync();
	}
}

// 로컬 설정에 맞게 카메라의 위아래 회전 범위를 제한한다.
void APdPlayerController::ApplyCameraViewPitchClamp()
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->ApplyCameraViewPitchClamp();
	}
}

// 소유 클라이언트에서 보상 표시용 에셋을 준비하도록 알림 컴포넌트에 전달한다.
void APdPlayerController::Client_ShowRewardNotifications_Implementation(const TArray<FPdRewardNotification>& Rewards)
{
	if (NotificationComponent)
	{
		NotificationComponent->ShowRewardNotifications(Rewards);
	}
}

// 서버가 확정한 처치 기록을 소유 클라이언트의 킬 로그에 표시한다.
void APdPlayerController::Client_AddKillLogEntry_Implementation(
	const FKillLogEntry& KillLogEntry)
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->AddKillLogEntry(KillLogEntry);
	}
}

// 서버가 보낸 골든 킬 안내를 소유 클라이언트에게 표시한다.
void APdPlayerController::Client_ShowGoldenKillAnnouncement_Implementation(
	const FText& AnnouncementText)
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->ShowGoldenKillAnnouncement(AnnouncementText);
	}
}

// 서버가 지급한 승리 골드를 로컬 저장 데이터에 반영한다.
void APdPlayerController::Client_AddGameVictoryGoldReward_Implementation(
	const FString& PlayerId,
	const int32 GoldReward)
{
	if (ControllerProfileSyncComponent)
	{
		ControllerProfileSyncComponent->ApplyGameVictoryGoldReward(PlayerId, GoldReward);
	}
}

// 서버가 집계한 아이템 획득 수를 로컬 기록에 반영한다.
void APdPlayerController::Client_AddCollectedItemCount_Implementation(
	const FString& PlayerId,
	const int32 ItemCount)
{
	if (ControllerProfileSyncComponent)
	{
		ControllerProfileSyncComponent->ApplyCollectedItemCount(PlayerId, ItemCount);
	}
}

// 경기 결과를 보관한 뒤 세션을 정리하고 타이틀로 이동한다.
void APdPlayerController::Client_TravelToTitleWithGameResult_Implementation(
	const FGameResultPresentationData& GameResultData,
	const FString& TitleMapName)
{
	if (ControllerSessionComponent)
	{
		ControllerSessionComponent->TravelToTitleWithGameResult(GameResultData, TitleMapName);
	}
}

// 경기 결과 없이 세션을 정리하고 타이틀로 이동한다.
void APdPlayerController::Client_TravelToTitleWithoutGameResult_Implementation(
	const FString& TitleMapName)
{
	if (ControllerSessionComponent)
	{
		ControllerSessionComponent->TravelToTitleWithoutGameResult(
			TitleMapName);
	}
}

// 사망한 플레이어에게 남은 리스폰 대기 시간을 표시한다.
void APdPlayerController::Client_StartRespawnDelayCountdown_Implementation(
	const float DelaySeconds)
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->StartRespawnDelayCountdown(DelaySeconds);
	}
}

// 리스폰 대기 표시를 닫는다.
void APdPlayerController::Client_HideRespawnDelayCountdown_Implementation()
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->HideRespawnDelayCountdown();
	}
}

// 지정된 리스폰 Pawn의 카메라만 복구한다. 위치와 사망 상태는 캐릭터 자신의 복구 RPC가 처리한다.
void APdPlayerController::Client_ResetRespawnedPawnStateAtTransform_Implementation(
	APawn* RespawnedPawn, const FTransform& RespawnTransform)
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->RefreshAfterRespawn(RespawnedPawn, RespawnTransform.GetRotation().Rotator());
	}
}

// 서버가 계산한 포탈 출구 위치·시선·속도를 소유 클라이언트에 반영한다.
void APdPlayerController::Client_ApplyPortalTeleport_Implementation(
	const FVector& TargetLocation,
	const FRotator& TargetRotation,
	const FVector& TargetVelocity,
	const FRotator& TargetControlRotation)
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn))
	{
		return;
	}

	ControlledPawn->TeleportTo(TargetLocation, TargetRotation, false, true);
	SetControlRotation(TargetControlRotation);

	if (UPawnMovementComponent* MovementComponent = ControlledPawn->GetMovementComponent())
	{
		MovementComponent->Velocity = TargetVelocity;
	}
}

// 로컬 스킨·업적 선택이 바뀌었을 때 서버에 다시 동기화하도록 요청한다.
void APdPlayerController::RequestLocalCosmeticProfileSync()
{
	if (ControllerProfileSyncComponent)
	{
		ControllerProfileSyncComponent->ScheduleLocalCosmeticProfileSync();
	}
}

// 서버의 요청에 따라 소유 클라이언트가 로컬 외형 프로필을 다시 제출한다.
void APdPlayerController::Client_RequestLocalCosmeticProfileSync_Implementation()
{
	RequestLocalCosmeticProfileSync();
}

// 클라이언트의 외형 프로필 주장을 서버의 카탈로그 검증과 적용 경로에 전달한다.
void APdPlayerController::Server_SubmitLocalCosmeticProfile_Implementation(
	const TArray<FName>& OwnedSkinNames,
	const FName SelectedAchievementId)
{
	if (ControllerProfileSyncComponent)
	{
		ControllerProfileSyncComponent
			->ApplySubmittedLocalCosmeticProfileOnServer(
				OwnedSkinNames,
				SelectedAchievementId);
	}
}

// 스킬바와 입력 아이콘이 현재 입력 설정을 조회할 수 있게 한다.
UControllerInputDefinition* APdPlayerController::GetLoadedInputDefinition() const
{
	return ControllerInputComponent
		? ControllerInputComponent->GetLoadedInputDefinition()
		: nullptr;
}

// 로컬 플레이어의 경기 나가기 요청을 세션 처리 경로에 전달한다.
bool APdPlayerController::RequestExitMatchToTitle()
{
	return ControllerSessionComponent
		&& ControllerSessionComponent->RequestExitMatchToTitle();
}

// 현재 사용할 화면 설정을 화면 표시 컴포넌트에 반영한다.
void APdPlayerController::ApplyControllerDefinition()
{
	const UPlayerControllerDefinition* Definition = GetControllerDefinition();
	if (!Definition)
	{
		return;
	}

	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->ApplySettings(
			Definition->GetPresentationSettings());
	}
}

// 이미 로드된 설정을 조회하고, 아직 없으면 클래스 기본 설정을 반환한다.
const UPlayerControllerDefinition* APdPlayerController::GetControllerDefinition() const
{
	if (LoadedPlayerControllerDefinition)
	{
		return LoadedPlayerControllerDefinition;
	}
	if (const UPlayerControllerDefinition* Definition = PlayerControllerDefinition.Get())
	{
		return Definition;
	}
	return GetDefault<UPlayerControllerDefinition>();
}

// 컨트롤러 설정을 비동기로 준비하고 종료된 요청의 콜백이 적용되지 않도록 구분한다.
void APdPlayerController::BeginControllerDefinitionPreload()
{
	ReleaseControllerDefinitionPreload();

	if (PlayerControllerDefinition.IsNull())
	{
		return;
	}
	if (UPlayerControllerDefinition* LoadedDefinition = PlayerControllerDefinition.Get())
	{
		LoadedPlayerControllerDefinition = LoadedDefinition;
		ApplyControllerDefinition();
		return;
	}

	const uint32 RequestGeneration = PlayerControllerDefinitionLoadGeneration;
	TSharedPtr<FStreamableHandle> NewLoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			PlayerControllerDefinition.ToSoftObjectPath(),
			FStreamableDelegate::CreateWeakLambda(
				this,
				[this, RequestGeneration]()
				{
					HandleControllerDefinitionPreloaded(RequestGeneration);
				}));

	if (NewLoadHandle.IsValid()
		&& RequestGeneration == PlayerControllerDefinitionLoadGeneration
		&& !LoadedPlayerControllerDefinition)
	{
		PlayerControllerDefinitionLoadHandle = MoveTemp(NewLoadHandle);
	}
	else if (NewLoadHandle.IsValid())
	{
		NewLoadHandle->ReleaseHandle();
	}
}

// 설정 에셋 로드가 끝나면 실행 중인 화면 처리에도 새 설정을 반영한다.
void APdPlayerController::HandleControllerDefinitionPreloaded(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != PlayerControllerDefinitionLoadGeneration)
	{
		return;
	}

	LoadedPlayerControllerDefinition = PlayerControllerDefinition.Get();
	if (!LoadedPlayerControllerDefinition)
	{
		UE_LOG(
			PdPlayerControllerLog,
			Error,
			TEXT("PlayerController definition '%s' did not resolve after asynchronous preload."),
			*PlayerControllerDefinition.ToString());
		return;
	}

	ApplyControllerDefinition();
}

// 남아 있는 설정 로딩을 취소하고 이전 요청의 콜백을 무효화한다.
void APdPlayerController::ReleaseControllerDefinitionPreload()
{
	++PlayerControllerDefinitionLoadGeneration;
	if (PlayerControllerDefinitionLoadHandle.IsValid())
	{
		PlayerControllerDefinitionLoadHandle->CancelHandle();
		PlayerControllerDefinitionLoadHandle->ReleaseHandle();
		PlayerControllerDefinitionLoadHandle.Reset();
	}
}

// 입력 설정이 없으면 프로젝트 기본값을 선택하고, 준비된 입력 바인딩을 한 번 갱신한다.
void APdPlayerController::RefreshControllerInput()
{
	if (!IsLocalController() || !ControllerInputComponent)
	{
		return;
	}

	if (!ControllerInputComponent->GetInputDefinition().IsNull())
	{
		ControllerInputComponent->RefreshInputDefinition();
		return;
	}

	const TSoftObjectPtr<UControllerInputDefinition> DefaultInputDefinition =
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
			.ControllerInput;
	if (!DefaultInputDefinition.IsNull())
	{
		ControllerInputComponent->SetInputDefinition(DefaultInputDefinition);
	}
}
