#include "UI/Room/RoomItemWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Online/OnlineSessionsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RoomItemWidget)

void URoomItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Join)
	{
		Btn_Join->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinClicked);
	}
}

void URoomItemWidget::NativeDestruct()
{
	if (Btn_Join)
	{
		Btn_Join->OnClicked.RemoveDynamic(this, &ThisClass::HandleJoinClicked);
	}

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (JoinSessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnJoinRoomRequestComplete.Remove(JoinSessionCompleteHandle);
			JoinSessionCompleteHandle.Reset();
		}

		const uint64 RequestId = ActiveJoinRequestId;
		ActiveJoinRequestId = 0;
		if (RequestId != 0)
		{
			OnlineSessionsSubsystem->CancelSessionRequest(RequestId);
		}
	}

	Super::NativeDestruct();
}

void URoomItemWidget::SetInfo(const FBlueprintSessionResult& InSessionResult)
{
	Result = InSessionResult;
	RefreshUI();
}

void URoomItemWidget::RefreshUI()
{
	FString RoomName;
	if (!Result.OnlineResult.Session.SessionSettings.Get(UOnlineSessionsSubsystem::GetRoomNameSettingKey(), RoomName) ||
		RoomName.IsEmpty())
	{
		RoomName = Result.OnlineResult.Session.OwningUserName;
	}

	FString MapName;
	if (!Result.OnlineResult.Session.SessionSettings.Get(UOnlineSessionsSubsystem::GetMapNameSettingKey(), MapName) ||
		MapName.IsEmpty())
	{
		MapName = TEXT("Unknown");
	}

	const int32 MaxPlayers = Result.OnlineResult.Session.SessionSettings.NumPublicConnections;
	const int32 CurrentPlayers = MaxPlayers - Result.OnlineResult.Session.NumOpenPublicConnections;

	if (Txt_RoomName)
	{
		Txt_RoomName->SetText(FText::FromString(RoomName));
	}

	if (Txt_MapName)
	{
		Txt_MapName->SetText(MenuTextOrFallback(FName(*(TEXT("Map.") + MapName)), FText::FromString(MapName)));
	}

	if (Txt_PlayerCount)
	{
		Txt_PlayerCount->SetText(FText::FromString(FString::Printf(TEXT("( %d / %d )"), CurrentPlayers, MaxPlayers)));
	}

	if (Btn_Join)
	{
		Btn_Join->SetIsEnabled(true);
	}
}

void URoomItemWidget::HandleJoinClicked()
{
	UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr;
	if (!OnlineSessionsSubsystem)
	{
		return;
	}

	if (Btn_Join)
	{
		Btn_Join->SetIsEnabled(false);
	}

	if (JoinSessionCompleteHandle.IsValid())
	{
		OnlineSessionsSubsystem->OnJoinRoomRequestComplete.Remove(JoinSessionCompleteHandle);
		JoinSessionCompleteHandle.Reset();
	}

	JoinSessionCompleteHandle =
		OnlineSessionsSubsystem->OnJoinRoomRequestComplete.AddUObject(this, &ThisClass::HandleJoinSessionComplete);
	ActiveJoinRequestId = OnlineSessionsSubsystem->BeginJoinRoomSession(GetOwningLocalPlayer(), Result);
	if (ActiveJoinRequestId == 0)
	{
		if (Btn_Join)
		{
			Btn_Join->SetIsEnabled(true);
		}
	}
}

void URoomItemWidget::HandleJoinSessionComplete(const uint64 RequestId, const bool bWasSuccessful)
{
	if (RequestId == 0 || RequestId != ActiveJoinRequestId)
	{
		return;
	}
	ActiveJoinRequestId = 0;

	if (UOnlineSessionsSubsystem* OnlineSessionsSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UOnlineSessionsSubsystem>()
		: nullptr)
	{
		if (JoinSessionCompleteHandle.IsValid())
		{
			OnlineSessionsSubsystem->OnJoinRoomRequestComplete.Remove(JoinSessionCompleteHandle);
			JoinSessionCompleteHandle.Reset();
		}
	}

	if (!bWasSuccessful && Btn_Join)
	{
		Btn_Join->SetIsEnabled(true);
	}
}
