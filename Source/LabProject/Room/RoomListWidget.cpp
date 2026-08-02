#include "Room/RoomListWidget.h"

#include "AudioSlider.h"
#include "Components/Button.h"
#include "Components/WrapBox.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/UI/ConnectingPopupWidget.h"
#include "Mode/PdGameInstance.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "Room/CreateRoomPopupWidget.h"
#include "Room/RoomItemWidget.h"
#include "TimerManager.h"
#include "UI/UiSubsystem.h"
#include "UI/Widget/AudioVolumeControl.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RoomListWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRoomListWidget, Log, All);

void URoomListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();

	if (!AudioVolumeSlider_)
	{
		AudioVolumeSlider_ = Cast<UAudioVolumeSlider>(GetWidgetFromName(TEXT("AudioVolumeSlider_")));
	}
	if (!Btn_Sound)
	{
		Btn_Sound = Cast<UButton>(GetWidgetFromName(TEXT("Btn_Sound")));
	}
	AudioVolumeControl = NewObject<UAudioVolumeControl>(this);
	AudioVolumeControl->Initialize(this, AudioVolumeSlider_, Btn_Sound);

	if (Btn_Refresh)
	{
		Btn_Refresh->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRefreshClicked);
	}

	if (Btn_CreateGame)
	{
		Btn_CreateGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreateGameClicked);
	}

	if (Btn_Close)
	{
		Btn_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (!FindSessionsCompleteHandle.IsValid())
		{
			FindSessionsCompleteHandle =
				OnlineSessionsSubsystem->OnFindRoomsRequestComplete.AddUObject(
					this,
					&ThisClass::HandleFindSessionsComplete);
		}
	}

	HideConnectingPopup();
	if (UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>())
	{
		PdGameInstance->PlayBgmForContext(EPdBgmContext::RoomList);
	}
	SetInfo();
}

void URoomListWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FRoomListWidgetSettings& Settings = WidgetDefinition->GetRoomListWidgetSettings();
		if (const TSubclassOf<URoomItemWidget> ResolvedRoomItemWidgetClass =
			WidgetDefinition->GetRoomItemWidgetClass())
		{
			RoomItemWidgetClass = ResolvedRoomItemWidgetClass;
		}
		if (const TSubclassOf<UCreateRoomPopupWidget> ResolvedCreateRoomPopupWidgetClass =
			WidgetDefinition->GetCreateRoomPopupWidgetClass())
		{
			CreateRoomPopupWidgetClass = ResolvedCreateRoomPopupWidgetClass;
		}
		MaxRoomSlots = FMath::Max(Settings.MaxRoomSlots, 1);
		MaxSearchResults = FMath::Max(Settings.MaxSearchResults, 1);
		bSearchLAN = Settings.bSearchLAN;
		bUseLobbies = Settings.bUseLobbies;
	}
}

void URoomListWidget::NativeDestruct()
{
	if (AudioVolumeControl)
	{
		AudioVolumeControl->Shutdown();
		AudioVolumeControl = nullptr;
	}

	if (Btn_Refresh)
	{
		Btn_Refresh->OnClicked.RemoveDynamic(this, &ThisClass::HandleRefreshClicked);
	}

	if (Btn_CreateGame)
	{
		Btn_CreateGame->OnClicked.RemoveDynamic(this, &ThisClass::HandleCreateGameClicked);
	}

	if (Btn_Close)
	{
		Btn_Close->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (FindSessionsCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnFindRoomsRequestComplete.Remove(
				FindSessionsCompleteHandle);
			FindSessionsCompleteHandle.Reset();
		}

		if (DestroySessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnDestroySessionRequestComplete.Remove(
				DestroySessionCompleteHandle);
			DestroySessionCompleteHandle.Reset();
		}

		const uint64 FindRequestId = ActiveFindRequestId;
		const uint64 DestroyRequestId = ActiveDestroyRequestId;
		ActiveFindRequestId = 0;
		ActiveDestroyRequestId = 0;
		if (FindRequestId != 0)
		{
			OnlineSessionsSubsystem->CancelSessionRequest(FindRequestId);
		}
		if (DestroyRequestId != 0)
		{
			OnlineSessionsSubsystem->CancelSessionRequest(DestroyRequestId);
		}
	}

	Super::NativeDestruct();
}

void URoomListWidget::SetInfo()
{
	if (!RoomList)
	{

		return;
	}

	RoomList->ClearChildren();
	Rooms.Reset();

	if (!RoomItemWidgetClass)
	{
		if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
		{
			RoomItemWidgetClass = WidgetDefinition->GetRoomItemWidgetClass();
		}
	}

	if (!RoomItemWidgetClass)
	{
		UE_LOG(LogRoomListWidget, Error, TEXT("[RoomUI] RoomItemWidgetClass is not configured in widget or DA_Widget."));
		return;
	}

	for (int32 Index = 0; Index < MaxRoomSlots; ++Index)
	{
		URoomItemWidget* RoomItemWidget = CreateWidget<URoomItemWidget>(GetOwningPlayer(), RoomItemWidgetClass);
		if (!RoomItemWidget)
		{
			continue;
		}

		RoomList->AddChildToWrapBox(RoomItemWidget);
		Rooms.Add(RoomItemWidget);
	}



	RefreshUI();
}

void URoomListWidget::RefreshUI()
{


	for (int32 Index = 0; Index < Rooms.Num(); ++Index)
	{
		URoomItemWidget* RoomItemWidget = Rooms[Index];
		if (!RoomItemWidget)
		{
			continue;
		}

		if (SessionInfos.IsValidIndex(Index))
		{
			RoomItemWidget->SetInfo(SessionInfos[Index]);
			RoomItemWidget->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			RoomItemWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URoomListWidget::HandleRefreshClicked()
{
	const UWorld* World = GetWorld();



	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		return;
	}

	SessionInfos.Reset();
	RefreshUI();
	ShowConnectingPopup(true);

	if (Btn_Refresh)
	{
		Btn_Refresh->SetIsEnabled(false);
	}

	ActiveFindRequestId = OnlineSessionsSubsystem->BeginFindRoomSessions(
		GetOwningLocalPlayer(),
		MaxSearchResults,
		bSearchLAN,
		bUseLobbies);
	if (ActiveFindRequestId == 0)
	{
		if (Btn_Refresh)
		{
			Btn_Refresh->SetIsEnabled(true);
		}
		HideConnectingPopup();
	}
}

void URoomListWidget::HandleCreateGameClicked()
{
	if (!CreateRoomPopupWidgetClass)
	{
		if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
		{
			CreateRoomPopupWidgetClass = WidgetDefinition->GetCreateRoomPopupWidgetClass();
		}
	}

	if (!CreateRoomPopupWidgetClass)
	{
		UE_LOG(LogRoomListWidget, Error, TEXT("[Room] CreateRoomPopupWidgetClass is not configured in widget or DA_Widget."));
		return;
	}

	if (UCreateRoomPopupWidget* PopupWidget = CreateWidget<UCreateRoomPopupWidget>(GetOwningPlayer(), CreateRoomPopupWidgetClass))
	{
		PopupWidget->AddToViewport(20);
	}
}

void URoomListWidget::HandleRefreshCancel()
{
	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		const uint64 RequestId = ActiveFindRequestId;
		ActiveFindRequestId = 0;
		if (RequestId != 0)
		{
			OnlineSessionsSubsystem->CancelSessionRequest(RequestId);
		}
	}

	if (Btn_Refresh)
	{
		Btn_Refresh->SetIsEnabled(true);
	}
	HideConnectingPopup();

}

void URoomListWidget::HandleCloseClicked()
{
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (OnlineSessionsSubsystem && ActiveFindRequestId != 0)
	{
		const uint64 FindRequestId = ActiveFindRequestId;
		ActiveFindRequestId = 0;
		OnlineSessionsSubsystem->CancelSessionRequest(FindRequestId);
	}

	if (!OnlineSessionsSubsystem || !OnlineSessionsSubsystem->HasNamedSession())
	{
		HideConnectingPopup();
		OpenTitleMap();
		return;
	}

	ShowConnectingPopup(false);

	if (DestroySessionCompleteHandle.IsValid())
	{
		OnlineSessionsSubsystem->OnDestroySessionRequestComplete.Remove(
			DestroySessionCompleteHandle);
		DestroySessionCompleteHandle.Reset();
	}

	bPendingCloseAfterDestroy = true;
	DestroySessionCompleteHandle =
		OnlineSessionsSubsystem->OnDestroySessionRequestComplete.AddUObject(
			this,
			&ThisClass::HandleDestroySessionForClose);
	ActiveDestroyRequestId =
		OnlineSessionsSubsystem->BeginDestroySession(GetOwningLocalPlayer());
	if (ActiveDestroyRequestId == 0)
	{
		bPendingCloseAfterDestroy = false;
		HideConnectingPopup();
	}
}

void URoomListWidget::HandleFindSessionsComplete(
	const uint64 RequestId,
	const TArray<FBlueprintSessionResult>& Results,
	const bool bWasSuccessful)
{
	if (RequestId == 0 || RequestId != ActiveFindRequestId)
	{
		return;
	}
	ActiveFindRequestId = 0;

	if (Btn_Refresh)
	{
		Btn_Refresh->SetIsEnabled(true);
	}

	HideConnectingPopup();
	SessionInfos = bWasSuccessful ? Results : TArray<FBlueprintSessionResult>();
	RefreshUI();
}

void URoomListWidget::HandleDestroySessionForClose(
	const uint64 RequestId,
	const bool bWasSuccessful)
{
	static_cast<void>(bWasSuccessful);

	if (RequestId == 0 || RequestId != ActiveDestroyRequestId)
	{
		return;
	}
	ActiveDestroyRequestId = 0;

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (DestroySessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnDestroySessionRequestComplete.Remove(
				DestroySessionCompleteHandle);
			DestroySessionCompleteHandle.Reset();
		}
	}

	if (bPendingCloseAfterDestroy)
	{
		bPendingCloseAfterDestroy = false;
		OpenTitleMap();
	}
}

void URoomListWidget::OpenTitleMap() const
{
	if (UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>())
	{
		PdGameInstance->PlayBgmForContext(EPdBgmContext::Startup);
	}

	const FString TitleMapName = GetResolvedTitleTravelMapName();
	if (!TitleMapName.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, TitleMapName]()
			{
				UGameplayStatics::OpenLevel(this, FName(*TitleMapName));
			}));
			return;
		}

		UGameplayStatics::OpenLevel(this, FName(*TitleMapName));
	}
}

FString URoomListWidget::GetResolvedTitleTravelMapName() const
{
	const FString LongPackageName = TitleMap.ToSoftObjectPath().GetLongPackageName();
	return LongPackageName.IsEmpty() ? TitleTravelMapName : LongPackageName;
}

UUiSubsystem* URoomListWidget::GetUiSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

UConnectingPopupWidget* URoomListWidget::ShowConnectingPopup(const bool bShowCancelButton)
{
	UUiSubsystem* UiSubsystem = GetUiSubsystem();
	if (!UiSubsystem)
	{
		return nullptr;
	}

	UConnectingPopupWidget* PopupWidget = UiSubsystem->ShowConnectingPopup(bShowCancelButton);
	if (PopupWidget)
	{
		PopupWidget->OnCanceled.RemoveDynamic(this, &ThisClass::HandleRefreshCancel);
		if (bShowCancelButton)
		{
			PopupWidget->OnCanceled.AddUniqueDynamic(this, &ThisClass::HandleRefreshCancel);
		}
	}

	return PopupWidget;
}

void URoomListWidget::HideConnectingPopup() const
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->HideConnectingPopup();
	}
}
