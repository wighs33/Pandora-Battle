#include "Mode/PdPlayerController.h"

#include "Component/Chat/ChatControllerComponent.h"
#include "Component/Player/ControllerDebugGrantComponent.h"
#include "Component/Player/ControllerInputComponent.h"
#include "Component/Player/ControllerPresentationComponent.h"
#include "Component/Player/ControllerProfileSyncComponent.h"
#include "Component/Player/ControllerSessionComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Definition/Player/PlayerControllerDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Lobby/Contents/LobbyPlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayerController)

DEFINE_LOG_CATEGORY(PdPlayerControllerLog);

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
	ControllerDebugGrantComponent =
		CreateDefaultSubobject<UControllerDebugGrantComponent>(TEXT("ControllerDebugGrantComponent"));
	PlayerControllerDefinition = TSoftObjectPtr<UPlayerControllerDefinition>(
		UPlayerControllerDefinition::GetDefaultDefinitionPath());
}

void APdPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	ApplyControllerDefinition();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
	BeginControllerDefinitionPreload();
}

void APdPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
		this,
		UGameFrameworkComponentManager::NAME_GameActorReady);
	ApplyDefaultInputDefinitionIfMissing();

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

void APdPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseControllerDefinitionPreload();
	LoadedPlayerControllerDefinition = nullptr;

	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->Shutdown();
	}

	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

void APdPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	ApplyDefaultInputDefinitionIfMissing();

	if (ControllerInputComponent)
	{
		ControllerInputComponent->RefreshInputDefinition();
	}

	if (InputComponent)
	{
		if (ChatControllerComponent)
		{
			ChatControllerComponent->BindInput(*InputComponent);
		}
		if (ControllerPresentationComponent)
		{
			ControllerPresentationComponent->BindInput(*InputComponent);
		}
	}
}

void APdPlayerController::AcknowledgePossession(APawn* P)
{
	Super::AcknowledgePossession(P);
	ApplyDefaultInputDefinitionIfMissing();

	if (IsLocalController())
	{
		if (ControllerPresentationComponent)
		{
			ControllerPresentationComponent->RefreshAfterPossession(
				P,
				!IsA<ALobbyPlayerController>());
		}
	}
	if (ControllerProfileSyncComponent
		&& (HasAuthority() || IsLocalController()))
	{
		ControllerProfileSyncComponent->ScheduleLocalCosmeticProfileSync();
	}
}

void APdPlayerController::ApplyCameraViewPitchClamp()
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->ApplyCameraViewPitchClamp();
	}
}

void APdPlayerController::Client_ShowRightNotification_Implementation(
	const FPdNotificationData& NotificationData)
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->ShowRightNotification(NotificationData);
	}
}

void APdPlayerController::Client_AddKillLogEntry_Implementation(
	const FKillLogEntry& KillLogEntry)
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->AddKillLogEntry(KillLogEntry);
	}
}

void APdPlayerController::Client_ShowGoldenKillAnnouncement_Implementation(
	const FText& AnnouncementText)
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->ShowGoldenKillAnnouncement(AnnouncementText);
	}
}

void APdPlayerController::Client_AddGameVictoryGoldReward_Implementation(
	const FString& PlayerId,
	const int32 GoldReward)
{
	if (ControllerProfileSyncComponent)
	{
		ControllerProfileSyncComponent->ApplyGameVictoryGoldReward(PlayerId, GoldReward);
	}
}

void APdPlayerController::Client_AddCollectedItemCount_Implementation(
	const FString& PlayerId,
	const int32 ItemCount)
{
	if (ControllerProfileSyncComponent)
	{
		ControllerProfileSyncComponent->ApplyCollectedItemCount(PlayerId, ItemCount);
	}
}

void APdPlayerController::Client_TravelToTitleWithGameResult_Implementation(
	const FGameResultPresentationData& GameResultData,
	const FString& TitleMapName)
{
	if (ControllerSessionComponent)
	{
		ControllerSessionComponent->TravelToTitleWithGameResult(GameResultData, TitleMapName);
	}
}

void APdPlayerController::Client_StartRespawnDelayCountdown_Implementation(
	const float DelaySeconds)
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->StartRespawnDelayCountdown(DelaySeconds);
	}
}

void APdPlayerController::Client_HideRespawnDelayCountdown_Implementation()
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->HideRespawnDelayCountdown();
	}
}

void APdPlayerController::Client_ResetRespawnedPawnStateAtTransform_Implementation(
	const FTransform& RespawnTransform)
{
	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->ResetRespawnedPawnStateAtTransform(RespawnTransform);
	}
}

void APdPlayerController::RequestLocalCosmeticProfileSync()
{
	if (ControllerProfileSyncComponent)
	{
		ControllerProfileSyncComponent->ScheduleLocalCosmeticProfileSync();
	}
}

void APdPlayerController::Server_SubmitLocalCosmeticProfile_Implementation(
	const TArray<FName>& OwnedSkinNames)
{
	if (ControllerProfileSyncComponent)
	{
		ControllerProfileSyncComponent
			->ApplySubmittedLocalCosmeticProfileOnServer(OwnedSkinNames);
	}
}

UControllerInputDefinition* APdPlayerController::GetLoadedInputDefinition() const
{
	return ControllerInputComponent
		? ControllerInputComponent->GetLoadedInputDefinition()
		: nullptr;
}

void APdPlayerController::RequestDebugGrantTestResources()
{
	if (ControllerDebugGrantComponent)
	{
		ControllerDebugGrantComponent->RequestGrantTestResources();
	}
}

bool APdPlayerController::RequestExitMatchToTitle()
{
	return ControllerSessionComponent
		&& ControllerSessionComponent->RequestExitMatchToTitle();
}

void APdPlayerController::Server_GrantDebugTestResources_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (ControllerDebugGrantComponent)
	{
		ControllerDebugGrantComponent->GrantTestResourcesOnServer();
	}
#endif
}

void APdPlayerController::Server_RequestExitMatchToTitle_Implementation()
{
	if (ControllerSessionComponent)
	{
		ControllerSessionComponent->HandleServerRequestExitMatchToTitle();
	}
}

void APdPlayerController::ApplyControllerDefinition()
{
	const UPlayerControllerDefinition* Definition = LoadControllerDefinition();
	if (!Definition)
	{
		return;
	}

	if (ControllerPresentationComponent)
	{
		ControllerPresentationComponent->ApplySettings(
			Definition->GetPresentationSettings());
	}
	if (ControllerProfileSyncComponent)
	{
		ControllerProfileSyncComponent->ApplySettings(
			Definition->GetProfileSyncSettings());
	}
	if (ControllerDebugGrantComponent)
	{
		ControllerDebugGrantComponent->ApplySettings(
			Definition->GetDebugGrantSettings());
	}
}

UPlayerControllerDefinition* APdPlayerController::LoadControllerDefinition()
{
	if (LoadedPlayerControllerDefinition)
	{
		return LoadedPlayerControllerDefinition;
	}

	if (!PlayerControllerDefinition.IsNull())
	{
		LoadedPlayerControllerDefinition = PlayerControllerDefinition.Get();
	}

	return LoadedPlayerControllerDefinition
		? LoadedPlayerControllerDefinition.Get()
		: GetMutableDefault<UPlayerControllerDefinition>();
}

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
	if (IsLocalController() && ControllerInputComponent)
	{
		const TSoftObjectPtr<UControllerInputDefinition>& DesiredInputDefinition =
			LoadedPlayerControllerDefinition->GetInputSettings().DefaultInputDefinition;
		const TSoftObjectPtr<UControllerInputDefinition>& NativeInputDefinition =
			GetDefault<UPlayerControllerDefinition>()->GetInputSettings().DefaultInputDefinition;
		const TSoftObjectPtr<UControllerInputDefinition>& ActiveInputDefinition =
			ControllerInputComponent->GetInputDefinition();
		if (!DesiredInputDefinition.IsNull()
			&& (ActiveInputDefinition.IsNull()
				|| ActiveInputDefinition == NativeInputDefinition))
		{
			ControllerInputComponent->SetInputDefinition(DesiredInputDefinition);
		}
		else
		{
			ControllerInputComponent->RefreshInputDefinition();
		}
	}
}

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

void APdPlayerController::ApplyDefaultInputDefinitionIfMissing()
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

	const UPlayerControllerDefinition* Definition = LoadControllerDefinition();
	if (!Definition)
	{
		return;
	}

	const TSoftObjectPtr<UControllerInputDefinition>& DefaultInputDefinition =
		Definition->GetInputSettings().DefaultInputDefinition;
	if (!DefaultInputDefinition.IsNull())
	{
		ControllerInputComponent->SetInputDefinition(DefaultInputDefinition);
	}
}

UControllerInputComponent* APdPlayerController::GetControllerInputComponent() const
{
	return ControllerInputComponent;
}
