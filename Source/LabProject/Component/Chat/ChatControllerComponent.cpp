#include "Component/Chat/ChatControllerComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChatControllerComponent)

UChatControllerComponent::UChatControllerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UChatControllerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ChatMessageAdded.Clear();
	ChatViewCommand.Clear();
	bChatFocused = false;
	Super::EndPlay(EndPlayReason);
}

void UChatControllerComponent::FocusChat()
{
	if (IsLocalChatOwner())
	{
		ChatViewCommand.Broadcast(EChatViewCommand::Focus);
	}
}

void UChatControllerComponent::ExitChat()
{
	if (IsLocalChatOwner())
	{
		ChatViewCommand.Broadcast(EChatViewCommand::Exit);
	}
}

bool UChatControllerComponent::IsChatFocused() const
{
	return bChatFocused;
}

void UChatControllerComponent::ScrollChat(const bool bUp)
{
	ChatViewCommand.Broadcast(bUp ? EChatViewCommand::ScrollUp : EChatViewCommand::ScrollDown);
}

void UChatControllerComponent::SubmitChatInput()
{
	ChatViewCommand.Broadcast(EChatViewCommand::SubmitInput);
}

void UChatControllerComponent::SubmitChatMessage(const FString& RawMessage)
{
	const FString SanitizedMessage = SanitizeChatMessage(RawMessage);
	if (!SanitizedMessage.IsEmpty())
	{
		Server_SendChatMessage(SanitizedMessage);
	}

	ExitChat();
}

void UChatControllerComponent::AddChatMessage(const FString& Message)
{
	ChatMessageAdded.Broadcast(Message);
}

void UChatControllerComponent::Server_SendChatMessage_Implementation(const FString& Message)
{
	if (!CanSendMessage())
	{
		return;
	}

	const FString SanitizedMessage = SanitizeChatMessage(Message);
	if (SanitizedMessage.IsEmpty())
	{
		return;
	}

	LastServerSendTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const FString FinalMessage = FString::Printf(TEXT("%s: %s"), *GetSenderDisplayName(), *SanitizedMessage);
	BroadcastChatMessage(FinalMessage);
}

void UChatControllerComponent::Client_AddChatMessage_Implementation(const FString& Message)
{
	AddChatMessage(Message);
}

bool UChatControllerComponent::IsLocalChatOwner() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	return PlayerController && PlayerController->IsLocalController();
}

void UChatControllerComponent::HandleChatInputAction()
{
	if (bChatFocused)
	{
		SubmitChatInput();
	}
	else
	{
		FocusChat();
	}
}

FString UChatControllerComponent::SanitizeChatMessage(const FString& Message) const
{
	FString Result = Message.TrimStartAndEnd();
	Result.ReplaceInline(TEXT("\r"), TEXT(" "));
	Result.ReplaceInline(TEXT("\n"), TEXT(" "));

	if (Result.Len() > MaxMessageLength)
	{
		Result.LeftInline(MaxMessageLength);
	}

	return Result;
}

FString UChatControllerComponent::GetSenderDisplayName() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	const APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState : nullptr;
	if (const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState))
	{
		const FString MatchDisplayName = PdPlayerState->GetPlayerMatchComponent()->GetMatchDisplayName().ToString()
			.TrimStartAndEnd();
		if (!MatchDisplayName.IsEmpty())
		{
			return MatchDisplayName;
		}
	}

	if (const UWorld* World = GetWorld())
	{
		if (const ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(World->GetGameInstance()))
		{
			int32 FallbackNicknameIndex = 1;
			if (const AGameStateBase* GameState = World->GetGameState())
			{
				const int32 PlayerIndex = PlayerState ? GameState->PlayerArray.IndexOfByKey(PlayerState) : INDEX_NONE;
				FallbackNicknameIndex = PlayerIndex != INDEX_NONE ? PlayerIndex + 1 : GameState->PlayerArray.Num() + 1;
			}

			const FString ResolvedNickname = LobbySubsystem->ResolveDefaultPlayerNickname(PlayerController, PlayerState,
				FallbackNicknameIndex).ToString().TrimStartAndEnd();
			if (!ResolvedNickname.IsEmpty())
			{
				return ResolvedNickname;
			}
		}
	}

	return TEXT("Player");
}

bool UChatControllerComponent::CanSendMessage() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetTimeSeconds() - LastServerSendTime >= MinSendInterval;
}

void UChatControllerComponent::BroadcastChatMessage(const FString& Message)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (!PlayerController)
		{
			continue;
		}

		if (UChatControllerComponent* ChatComponent = PlayerController->FindComponentByClass<UChatControllerComponent>())
		{
			ChatComponent->Client_AddChatMessage(Message);
		}
	}
}
