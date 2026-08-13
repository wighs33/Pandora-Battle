#include "Room/CreateRoomPopupWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/UI/ConnectingPopupWidget.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "UI/UiSubsystem.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CreateRoomPopupWidget)

namespace
{
	void TravelRoomToListenMap(const UObject* WorldContextObject, const FString& MapName)
	{
		if (UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr)
		{
			if (World->GetNetMode() != NM_Client && World->GetNetDriver())
			{
				World->ServerTravel(FString::Printf(TEXT("%s?listen"), *MapName));
				return;
			}
		}

		UGameplayStatics::OpenLevel(WorldContextObject, FName(*MapName), true, TEXT("listen"));
	}
}

void UCreateRoomPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();

	if (UButton* CreateButton = GetCreateButton())
	{
		CreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreateClicked);
	}

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}
}

void UCreateRoomPopupWidget::NativeDestruct()
{
	if (UButton* CreateButton = GetCreateButton())
	{
		CreateButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCreateClicked);
	}

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelClicked);
	}

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (CreateSessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnCreateRoomRequestComplete.Remove(
				CreateSessionCompleteHandle);
			CreateSessionCompleteHandle.Reset();
		}

		const uint64 RequestId = ActiveCreateRequestId;
		ActiveCreateRequestId = 0;
		if (RequestId != 0)
		{
			OnlineSessionsSubsystem->CancelSessionRequest(RequestId);
		}
	}

	Super::NativeDestruct();
}

void UCreateRoomPopupWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FRoomListWidgetSettings& Settings = WidgetDefinition->GetRoomListWidgetSettings();
		DefaultRoomName = Settings.DefaultRoomName;
		MaxPublicConnections = FMath::Max(Settings.MaxPublicConnections, 1);
		bCreateLAN = Settings.bCreateLAN;
	}
}

void UCreateRoomPopupWidget::HandleCreateClicked()
{
	const FString InitialSessionMapName = TEXT("Lobby");

UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		return;
	}

	if (UButton* CreateButton = GetCreateButton())
	{
		CreateButton->SetIsEnabled(false);
	}
	ShowConnectingPopup(true);

	if (CreateSessionCompleteHandle.IsValid())
	{
		OnlineSessionsSubsystem->OnCreateRoomRequestComplete.Remove(
			CreateSessionCompleteHandle);
		CreateSessionCompleteHandle.Reset();
	}

	CreateSessionCompleteHandle =
		OnlineSessionsSubsystem->OnCreateRoomRequestComplete.AddUObject(
			this,
			&ThisClass::HandleCreateSessionComplete);
	ActiveCreateRequestId = OnlineSessionsSubsystem->BeginCreateRoomSession(
		GetOwningLocalPlayer(),
		GetRoomNameInput(),
		InitialSessionMapName,
		MaxPublicConnections,
		bCreateLAN);
	if (ActiveCreateRequestId == 0)
	{
		if (UButton* CreateButton = GetCreateButton())
		{
			CreateButton->SetIsEnabled(true);
		}
		HideConnectingPopup();
	}
}

void UCreateRoomPopupWidget::HandleCancelClicked()
{
	RemoveFromParent();
}

void UCreateRoomPopupWidget::HandleCreateLoadingCancel()
{
	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		const uint64 RequestId = ActiveCreateRequestId;
		ActiveCreateRequestId = 0;
		if (RequestId != 0)
		{
			OnlineSessionsSubsystem->CancelSessionRequest(RequestId);
		}
	}

	if (UButton* CreateButton = GetCreateButton())
	{
		CreateButton->SetIsEnabled(true);
	}
	HideConnectingPopup();

}

void UCreateRoomPopupWidget::HandleCreateSessionComplete(
	const uint64 RequestId,
	const bool bWasSuccessful)
{
	if (RequestId == 0 || RequestId != ActiveCreateRequestId)
	{
		return;
	}
	ActiveCreateRequestId = 0;

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (CreateSessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnCreateRoomRequestComplete.Remove(
				CreateSessionCompleteHandle);
			CreateSessionCompleteHandle.Reset();
		}
	}

	if (bWasSuccessful)
	{
		OpenLobbyAsListenServer();
		return;
	}

	if (UButton* CreateButton = GetCreateButton())
	{
		CreateButton->SetIsEnabled(true);
	}
	HideConnectingPopup();
}

void UCreateRoomPopupWidget::OpenLobbyAsListenServer() const
{
	const FString LobbyMapName = GetResolvedLobbyTravelMapName();
	if (LobbyMapName.IsEmpty())
	{

		return;
	}

	TravelRoomToListenMap(this, LobbyMapName);
}

FString UCreateRoomPopupWidget::GetRoomNameInput() const
{
	if (const UEditableTextBox* TextBox = GetRoomNameTextBox())
	{
		const FString RoomName = TextBox->GetText().ToString().TrimStartAndEnd();
		if (!RoomName.IsEmpty())
		{
			return RoomName;
		}
	}

	return DefaultRoomName;
}

UButton* UCreateRoomPopupWidget::GetCreateButton() const
{
	return Btn_Create.Get();
}

UEditableTextBox* UCreateRoomPopupWidget::GetRoomNameTextBox() const
{
	return TxtBox_InputGameName.Get();
}

FString UCreateRoomPopupWidget::GetResolvedLobbyTravelMapName() const
{
	const ULevelDefinition* Definition =
		ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetLobbyTravelMapName() : FString();
}

UUiSubsystem* UCreateRoomPopupWidget::GetUiSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}

UConnectingPopupWidget* UCreateRoomPopupWidget::ShowConnectingPopup(const bool bShowCancelButton)
{
	UUiSubsystem* UiSubsystem = GetUiSubsystem();
	if (!UiSubsystem)
	{
		return nullptr;
	}

	UConnectingPopupWidget* PopupWidget = UiSubsystem->ShowConnectingPopup(bShowCancelButton);
	if (PopupWidget)
	{
		PopupWidget->OnCanceled.RemoveDynamic(this, &ThisClass::HandleCreateLoadingCancel);
		if (bShowCancelButton)
		{
			PopupWidget->OnCanceled.AddUniqueDynamic(this, &ThisClass::HandleCreateLoadingCancel);
		}
	}

	return PopupWidget;
}

void UCreateRoomPopupWidget::HideConnectingPopup() const
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->HideConnectingPopup();
	}
}
