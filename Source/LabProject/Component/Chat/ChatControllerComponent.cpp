#include "Component/Chat/ChatControllerComponent.h"

#include "Blueprint/WidgetTree.h"
#include "Chat/ChatBoxWidget.h"
#include "Chat/ChatEntryWidget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerState.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChatControllerComponent)

UChatControllerComponent::UChatControllerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UChatControllerComponent::BeginPlay()
{
	Super::BeginPlay();

	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (PlayerController && PlayerController->IsLocalController())
	{
		EnsureChatBox(false);
	}
}

void UChatControllerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ChatBoxWidget = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UChatControllerComponent::BindInput(UInputComponent& InputComponent)
{
	InputComponent.BindKey(EKeys::Enter, IE_Pressed, this, &ThisClass::HandleEnterPressed);
	InputComponent.BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ThisClass::HandleMouseWheelUp);
	InputComponent.BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ThisClass::HandleMouseWheelDown);
}

void UChatControllerComponent::FocusChat()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!ChatBoxWidget)
	{
		EnsureChatBox();
	}

	if (!ChatBoxWidget)
	{

		return;
	}

	ChatBoxWidget->FocusChat();
}

void UChatControllerComponent::ExitChat()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (ChatBoxWidget)
	{
		ChatBoxWidget->ExitChat();
	}
}

bool UChatControllerComponent::IsChatFocused() const
{
	return ChatBoxWidget && ChatBoxWidget->IsChatFocused();
}

void UChatControllerComponent::ScrollChat(const bool bUp)
{
	if (ChatBoxWidget)
	{
		ChatBoxWidget->Scroll(bUp);
	}
}

void UChatControllerComponent::SubmitChatInput()
{
	if (ChatBoxWidget)
	{
		ChatBoxWidget->SubmitChatInput();
	}
}

void UChatControllerComponent::SubmitChatMessage(const FString& RawMessage)
{
	const FString SanitizedMessage = SanitizeChatMessage(RawMessage);
	if (!SanitizedMessage.IsEmpty())
	{
		Server_SendChatMessage(SanitizedMessage);
	}

	bSuppressNextEnterFocus = true;
	ExitChat();
}

void UChatControllerComponent::AddChatMessage(const FString& Message)
{
	if (!ChatBoxWidget)
	{
		EnsureChatBox();
	}

	if (ChatBoxWidget)
	{
		ChatBoxWidget->AddChatMessage(Message);
	}
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

bool UChatControllerComponent::EnsureChatBox(const bool bLogIfMissing)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return false;
	}

	if (ChatBoxWidget)
	{
		return true;
	}

	ChatBoxWidget = FindChatBoxInPlayerHUD();
	if (ChatBoxWidget)
	{
		ChatBoxWidget->InitializeChat(this, ChatEntryWidgetClass, ScrollMultiplier);

		return true;
	}


	return false;
}

UChatBoxWidget* UChatControllerComponent::FindChatBoxInPlayerHUD() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	const APdHUD* PdHUD = PlayerController ? Cast<APdHUD>(PlayerController->GetHUD()) : nullptr;
	UUserWidget* PlayerHUDWidget = PdHUD ? PdHUD->GetPlayerHudWidget() : nullptr;
	if (!PlayerHUDWidget || !PlayerHUDWidget->WidgetTree)
	{
		return nullptr;
	}

	if (UChatBoxWidget* NamedChatBox = PdWidgetLookup::FindWidgetByNames<UChatBoxWidget>(PlayerHUDWidget->WidgetTree, {
		TEXT("WBP_ChatBox"),
		TEXT("ChatBox")
	}))
	{
		return NamedChatBox;
	}

	return PdWidgetLookup::FindFirstWidgetOfType<UChatBoxWidget>(PlayerHUDWidget->WidgetTree);
}

void UChatControllerComponent::HandleEnterPressed()
{
	if (bSuppressNextEnterFocus)
	{
		bSuppressNextEnterFocus = false;
		return;
	}

	if (!ChatBoxWidget)
	{
		EnsureChatBox();
	}

	if (ChatBoxWidget && ChatBoxWidget->IsChatFocused())
	{
		SubmitChatInput();
	}
	else
	{
		FocusChat();
	}
}

void UChatControllerComponent::HandleMouseWheelUp()
{
	ScrollChat(true);
}

void UChatControllerComponent::HandleMouseWheelDown()
{
	ScrollChat(false);
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
		const FString MatchDisplayName = PdPlayerState
			->GetPlayerMatchComponent()
			->GetMatchDisplayName()
			.ToString()
			.TrimStartAndEnd();
		if (!MatchDisplayName.IsEmpty())
		{
			return MatchDisplayName;
		}
	}

	if (const UWorld* World = GetWorld())
	{
		if (const UPdGameInstance* PdGameInstance = World->GetGameInstance<UPdGameInstance>())
		{
			int32 FallbackNicknameIndex = 1;
			if (const AGameStateBase* GameState = World->GetGameState())
			{
				const int32 PlayerIndex = PlayerState
					? GameState->PlayerArray.IndexOfByKey(PlayerState)
					: INDEX_NONE;
				FallbackNicknameIndex = PlayerIndex != INDEX_NONE
					? PlayerIndex + 1
					: GameState->PlayerArray.Num() + 1;
			}

			const FString ResolvedNickname = PdGameInstance->ResolveDefaultPlayerNickname(
				PlayerController,
				PlayerState,
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
