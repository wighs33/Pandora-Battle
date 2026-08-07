#include "Lobby/UI/LobbyUserWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformProcess.h"
#include "Lobby/Contents/LobbyPlayerController.h"
#include "Lobby/Contents/LobbyPlayerState.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyUserWidget)

namespace
{
	constexpr int32 LobbyTeamColorCount = 6;

	const TCHAR* LobbyTeamColorOptions[LobbyTeamColorCount] =
	{
		TEXT("Red"),
		TEXT("Blue"),
		TEXT("Yellow"),
		TEXT("Purple"),
		TEXT("Green"),
		TEXT("Orange")
	};

	int32 NormalizeLobbyTeamColorIndex(const int32 TeamColorIndex)
	{
		return TeamColorIndex == INDEX_NONE
			? 0
			: FMath::Clamp(TeamColorIndex, 0, LobbyTeamColorCount - 1);
	}

	FLinearColor GetLobbyTeamColor(const int32 TeamColorIndex)
	{
		switch (NormalizeLobbyTeamColorIndex(TeamColorIndex))
		{
		case 0:
			return FLinearColor(0.95f, 0.08f, 0.06f, 1.0f);
		case 1:
			return FLinearColor(0.08f, 0.28f, 1.0f, 1.0f);
		case 2:
			return FLinearColor(1.0f, 0.78f, 0.08f, 1.0f);
		case 3:
			return FLinearColor(0.58f, 0.18f, 0.95f, 1.0f);
		case 4:
			return FLinearColor(0.08f, 0.72f, 0.24f, 1.0f);
		case 5:
			return FLinearColor(1.0f, 0.42f, 0.04f, 1.0f);
		default:
			return FLinearColor::White;
		}
	}

	FLinearColor GetLobbyTeamColorTint(const int32 TeamColorIndex)
	{
		FLinearColor HsvColor = GetLobbyTeamColor(TeamColorIndex).LinearRGBToHSV();
		HsvColor.G = 0.9f;

		FLinearColor Tint = HsvColor.HSVToLinearRGB();
		Tint.A = 0.9f;
		return Tint;
	}

	int32 FindLobbyTeamColorOptionIndex(const UComboBoxString* ComboBox, const FString& OptionName)
	{
		if (ComboBox)
		{
			const int32 OptionCount = ComboBox->GetOptionCount();
			for (int32 OptionIndex = 0; OptionIndex < OptionCount; ++OptionIndex)
			{
				if (ComboBox->GetOptionAtIndex(OptionIndex).Equals(OptionName, ESearchCase::IgnoreCase))
				{
					return FMath::Clamp(OptionIndex, 0, LobbyTeamColorCount - 1);
				}
			}
		}

		for (int32 OptionIndex = 0; OptionIndex < LobbyTeamColorCount; ++OptionIndex)
		{
			if (OptionName.Equals(LobbyTeamColorOptions[OptionIndex], ESearchCase::IgnoreCase))
			{
				return OptionIndex;
			}
		}

		return INDEX_NONE;
	}

	bool TryParseSteamId64(const FString& SteamIdString, uint64& OutSteamId)
	{
		OutSteamId = 0;

		const FString TrimmedSteamId = SteamIdString.TrimStartAndEnd();
		if (TrimmedSteamId.IsEmpty())
		{
			return false;
		}

		for (const TCHAR Character : TrimmedSteamId)
		{
			if (!FChar::IsDigit(Character))
			{
				return false;
			}
		}

		const TCHAR* Start = *TrimmedSteamId;
		TCHAR* End = nullptr;
		OutSteamId = FCString::Strtoui64(Start, &End, 10);
		return End && End != Start && *End == TEXT('\0') && OutSteamId != 0;
	}

	void OpenSteamFriendAddUrlFallback(const FString& SteamIdString)
	{
		const FString FriendAddUrl = FString::Printf(TEXT("steam://friends/add/%s"), *SteamIdString);
		FPlatformProcess::LaunchURL(*FriendAddUrl, nullptr, nullptr);
	}
}

void ULobbyUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_KickPlayer)
	{
		Btn_KickPlayer->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleKickClicked);
	}

	if (Btn_FriendAdd)
	{
		Btn_FriendAdd->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFriendAddClicked);
	}

	if (Editable_PlayerName)
	{
		Editable_PlayerName->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandlePlayerNameCommitted);
	}

	EnsureTeamColorOptions();

	if (Cbb_TeamColor)
	{
		Cbb_TeamColor->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleTeamColorSelectionChanged);
	}
}

void ULobbyUserWidget::NativeDestruct()
{
	if (Btn_KickPlayer)
	{
		Btn_KickPlayer->OnClicked.RemoveDynamic(this, &ThisClass::HandleKickClicked);
	}

	if (Btn_FriendAdd)
	{
		Btn_FriendAdd->OnClicked.RemoveDynamic(this, &ThisClass::HandleFriendAddClicked);
	}

	if (Editable_PlayerName)
	{
		Editable_PlayerName->OnTextCommitted.RemoveDynamic(this, &ThisClass::HandlePlayerNameCommitted);
	}

	if (Cbb_TeamColor)
	{
		Cbb_TeamColor->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleTeamColorSelectionChanged);
	}

	Super::NativeDestruct();
}

void ULobbyUserWidget::SetInfo(ALobbyPlayerState* InPlayerState)
{
	PlayerState = InPlayerState;
	RefreshUI();
}

void ULobbyUserWidget::RefreshUI()
{
	if (!PlayerState)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bLocalPlayer = IsRepresentingLocalPlayer();
	const bool bCanKick = CanLocalPlayerKick() && !bLocalPlayer;
	uint64 ParsedSteamId = 0;
	const bool bCanAddFriend = !bLocalPlayer && TryParseSteamId64(GetRepresentedSteamIdString(), ParsedSteamId);

	if (Btn_Ready)
	{
		Btn_Ready->SetVisibility(ESlateVisibility::Collapsed);
		Btn_Ready->SetIsEnabled(false);
	}

	if (Btn_KickPlayer)
	{
		Btn_KickPlayer->SetVisibility(bCanKick ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (Btn_FriendAdd)
	{
		Btn_FriendAdd->SetVisibility(bLocalPlayer ? ESlateVisibility::Hidden : ESlateVisibility::Visible);
		Btn_FriendAdd->SetIsEnabled(bCanAddFriend);
	}

	if (Img_OwnerMark)
	{
		Img_OwnerMark->SetVisibility(IsLobbyOwnerPlayer() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Txt_Ready)
	{
		Txt_Ready->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Txt_PlayerName)
	{
		Txt_PlayerName->SetVisibility(ESlateVisibility::Hidden);
		Txt_PlayerName->SetText(PlayerState->GetNickname());
	}

	if (Editable_PlayerName)
	{
		const bool bPreserveLocalNicknameDraft =
			bLocalPlayer && Editable_PlayerName->HasKeyboardFocus();

		Editable_PlayerName->SetVisibility(ESlateVisibility::Visible);
		Editable_PlayerName->SetHintText(PlayerState->GetNicknameHint());
		if (!bPreserveLocalNicknameDraft)
		{
			Editable_PlayerName->SetText(PlayerState->IsUsingNicknameHint()
				? FText::GetEmpty()
				: PlayerState->GetNickname());
		}
		Editable_PlayerName->SetIsReadOnly(!bLocalPlayer);
	}

	RefreshTeamColorUI();
}

void ULobbyUserWidget::HandleKickClicked()
{
	if (!PlayerState)
	{
		return;
	}

	if (ALobbyPlayerController* LobbyPlayerController = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		LobbyPlayerController->Server_HandleKickPlayer(PlayerState);
	}
}

void ULobbyUserWidget::HandleFriendAddClicked()
{
	if (!PlayerState || IsRepresentingLocalPlayer())
	{
		return;
	}

}

void ULobbyUserWidget::HandlePlayerNameCommitted(const FText& InText, ETextCommit::Type CommitMethod)
{
	if (!PlayerState || !IsRepresentingLocalPlayer())
	{
		return;
	}

	if (CommitMethod != ETextCommit::OnEnter && CommitMethod != ETextCommit::OnUserMovedFocus)
	{
		return;
	}

	if (ALobbyPlayerController* LobbyPlayerController = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		LobbyPlayerController->Server_HandleChangeNickname(InText);
	}
}

void ULobbyUserWidget::HandleTeamColorSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const int32 SelectedTeamColorIndex = FindLobbyTeamColorOptionIndex(Cbb_TeamColor, SelectedItem);
	if (SelectedTeamColorIndex == INDEX_NONE)
	{
		return;
	}

	SetColorBorderByTeamColorIndex(SelectedTeamColorIndex);

	if (SelectionType == ESelectInfo::Direct || !PlayerState || !IsRepresentingLocalPlayer())
	{
		return;
	}

	if (PlayerState->GetTeamColorIndex() == SelectedTeamColorIndex)
	{
		return;
	}

	if (ALobbyPlayerController* LobbyPlayerController = Cast<ALobbyPlayerController>(GetOwningPlayer()))
	{
		LobbyPlayerController->Server_HandleChangeTeamColor(SelectedTeamColorIndex);
	}
}

bool ULobbyUserWidget::IsRepresentingLocalPlayer() const
{
	const APlayerController* LocalPlayerController = GetOwningPlayer();
	return LocalPlayerController && LocalPlayerController->PlayerState == PlayerState;
}

bool ULobbyUserWidget::CanLocalPlayerKick() const
{
	const UWorld* World = GetWorld();
	const APlayerController* LocalPlayerController = GetOwningPlayer();
	return World && World->GetAuthGameMode() && LocalPlayerController && LocalPlayerController->IsLocalController();
}

bool ULobbyUserWidget::IsLobbyOwnerPlayer() const
{
	if (!PlayerState || PlayerState->IsLeavingLobby())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return false;
	}

	const ALobbyPlayerState* OwnerState = nullptr;
	for (APlayerState* CandidateState : GameState->PlayerArray)
	{
		const ALobbyPlayerState* LobbyPlayerState = Cast<ALobbyPlayerState>(CandidateState);
		if (!LobbyPlayerState || LobbyPlayerState->IsLeavingLobby())
		{
			continue;
		}

		if (!OwnerState || LobbyPlayerState->GetPlayerId() < OwnerState->GetPlayerId())
		{
			OwnerState = LobbyPlayerState;
		}
	}

	return OwnerState == PlayerState;
}

FString ULobbyUserWidget::GetRepresentedSteamIdString() const
{
	if (!PlayerState)
	{
		return FString();
	}

	const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();
	const TSharedPtr<const FUniqueNetId> UniqueNetId = UniqueId.GetUniqueNetId();
	return UniqueNetId.IsValid() ? UniqueNetId->ToString() : FString();
}

bool ULobbyUserWidget::OpenSteamFriendAddOverlay() const
{
	const FString SteamIdString = GetRepresentedSteamIdString();

	uint64 SteamIdValue = 0;
	if (!TryParseSteamId64(SteamIdString, SteamIdValue))
	{

		return false;
	}

	CSteamID TargetSteamId(SteamIdValue);
	if (!TargetSteamId.IsValid())
	{

		return false;
	}

	ISteamFriends* SteamFriendsInterface = SteamFriends();
	if (!SteamAPI_IsSteamRunning() || !SteamFriendsInterface)
	{
		OpenSteamFriendAddUrlFallback(SteamIdString);
		return true;
	}

	ISteamUtils* SteamUtilsInterface = SteamUtils();
	if (SteamUtilsInterface && !SteamUtilsInterface->IsOverlayEnabled())
	{
		OpenSteamFriendAddUrlFallback(SteamIdString);
		return true;
	}

	SteamFriendsInterface->ActivateGameOverlayToUser("friendadd", TargetSteamId);
	return true;
}

void ULobbyUserWidget::EnsureTeamColorOptions()
{
	if (!Cbb_TeamColor || Cbb_TeamColor->GetOptionCount() > 0)
	{
		return;
	}

	for (const TCHAR* OptionName : LobbyTeamColorOptions)
	{
		Cbb_TeamColor->AddOption(OptionName);
	}
}

void ULobbyUserWidget::RefreshTeamColorUI()
{
	if (!PlayerState)
	{
		return;
	}

	const int32 TeamColorIndex = NormalizeLobbyTeamColorIndex(PlayerState->GetTeamColorIndex());

	if (Cbb_TeamColor)
	{
		EnsureTeamColorOptions();
		const int32 OptionCount = Cbb_TeamColor->GetOptionCount();
		if (OptionCount > 0)
		{
			const int32 OptionIndex = FMath::Clamp(TeamColorIndex, 0, OptionCount - 1);
			Cbb_TeamColor->SetSelectedOption(Cbb_TeamColor->GetOptionAtIndex(OptionIndex));
		}
		Cbb_TeamColor->SetIsEnabled(IsRepresentingLocalPlayer());
	}

	SetColorBorderByTeamColorIndex(TeamColorIndex);
}

void ULobbyUserWidget::SetColorBorderByTeamColorIndex(const int32 TeamColorIndex)
{
	if (ColorBorder)
	{
		ColorBorder->SetBrushColor(GetLobbyTeamColorTint(TeamColorIndex));
	}
}
