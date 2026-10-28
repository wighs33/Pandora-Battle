#include "UI/Room/CreateRoomPopupWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Definition/Level/LevelDefinition.h"
#include "Online/OnlineSessionsSubsystem.h"
#include "UI/Core/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CreateRoomPopupWidget)

void UCreateRoomPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	ApplyWidgetDefinitionSettings();

	if (UButton* CreateButton = GetCreateButton())
	{
		CreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreateClicked);
	}

	if (Btn_Cancel)
	{
		Btn_Cancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	}

	// 결과 알림은 화면이 있는 동안 한 번 구독하고, 지금 기다리는 요청의 결과만 처리한다.
	if (UOnlineSessionsSubsystem* OnlineSessions = GetOnlineSessions())
	{
		CreateSessionCompleteHandle =
			OnlineSessions->OnCreateRoomRequestComplete.AddUObject(this, &ThisClass::HandleCreateSessionComplete);
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

	if (UOnlineSessionsSubsystem* OnlineSessions = GetOnlineSessions())
	{
		OnlineSessions->OnCreateRoomRequestComplete.Remove(CreateSessionCompleteHandle);
		if (ActiveCreateRequestId != 0)
		{
			OnlineSessions->CancelSessionRequest(ActiveCreateRequestId);
		}
	}
	CreateSessionCompleteHandle.Reset();
	ActiveCreateRequestId = 0;

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

	UOnlineSessionsSubsystem* OnlineSessions = GetOnlineSessions();
	if (!OnlineSessions)
	{
		return;
	}

	if (UButton* CreateButton = GetCreateButton())
	{
		CreateButton->SetIsEnabled(false);
	}

	ActiveCreateRequestId = OnlineSessions->BeginCreateRoomSession(GetOwningLocalPlayer(), GetRoomNameInput(),
		InitialSessionMapName, MaxPublicConnections, bCreateLAN);
	if (ActiveCreateRequestId == 0)
	{
		if (UButton* CreateButton = GetCreateButton())
		{
			CreateButton->SetIsEnabled(true);
		}
	}
}

void UCreateRoomPopupWidget::HandleCancelClicked()
{
	DeactivateWidget();
}

void UCreateRoomPopupWidget::HandleCreateSessionComplete(const uint64 RequestId, const bool bWasSuccessful)
{
	if (RequestId == 0 || RequestId != ActiveCreateRequestId)
	{
		return;
	}
	ActiveCreateRequestId = 0;

	if (bWasSuccessful)
	{
		OpenLobbyAsListenServer();
		return;
	}

	if (UButton* CreateButton = GetCreateButton())
	{
		CreateButton->SetIsEnabled(true);
	}
}

void UCreateRoomPopupWidget::OpenLobbyAsListenServer() const
{
	const FString LobbyMapName = GetResolvedLobbyTravelMapName();
	if (LobbyMapName.IsEmpty())
	{
		return;
	}

	if (const UOnlineSessionsSubsystem* OnlineSessions = GetOnlineSessions())
	{
		OnlineSessions->TravelToListenMap(LobbyMapName);
	}
}

UOnlineSessionsSubsystem* UCreateRoomPopupWidget::GetOnlineSessions() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>() : nullptr;
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
	const ULevelDefinition* Definition = ULevelDefinition::ResolveDefaultDefinition();
	return Definition ? Definition->GetLobbyTravelMapName() : FString();
}
