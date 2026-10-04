#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChatControllerComponent.generated.h"

class APlayerController;

/** 채팅 화면이 해야 할 입력 동작. 입력 처리는 이 컴포넌트가 받고, 화면은 이 명령을 구독해 그린다. */
enum class EChatViewCommand : uint8
{
	Focus,
	Exit,
	ScrollUp,
	ScrollDown,
	SubmitInput,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FPdChatMessageAdded, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FPdChatViewCommand, EChatViewCommand);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UChatControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UChatControllerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void FocusChat();

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void ExitChat();

	UFUNCTION(BlueprintPure, Category = "!Chat")
	bool IsChatFocused() const;

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void ScrollChat(bool bUp);

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void SubmitChatInput();

	void SubmitChatMessage(const FString& RawMessage);

	UFUNCTION(BlueprintCallable, Category = "!Chat")
	void AddChatMessage(const FString& Message);

	// 채팅 화면은 이 알림을 구독해 메시지와 입력 명령을 받고, 입력창 포커스가 바뀌면 SetChatFocused로 알려 준다.
	FPdChatMessageAdded& OnChatMessageAdded() { return ChatMessageAdded; }
	FPdChatViewCommand& OnChatViewCommand() { return ChatViewCommand; }
	void SetChatFocused(bool bFocused) { bChatFocused = bFocused; }

	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void Server_SendChatMessage(const FString& Message);

	UFUNCTION(Client, Reliable)
	void Client_AddChatMessage(const FString& Message);

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleChatInputAction();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool IsLocalChatOwner() const;

	FString SanitizeChatMessage(const FString& Message) const;
	FString GetSenderDisplayName() const;
	bool CanSendMessage() const;
	void BroadcastChatMessage(const FString& Message);

private:
	UPROPERTY(EditAnywhere, Category = "!Chat|Validation", meta = (ClampMin = "1"))
	int32 MaxMessageLength = 120;

	UPROPERTY(EditAnywhere, Category = "!Chat|Validation", meta = (ClampMin = "0.0"))
	float MinSendInterval = 0.2f;

	FPdChatMessageAdded ChatMessageAdded;
	FPdChatViewCommand ChatViewCommand;
	bool bChatFocused = false;

	double LastServerSendTime = -1000.0;
};
