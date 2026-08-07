#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ChatControllerComponent.generated.h"

class APlayerController;
class UChatBoxWidget;
class UChatEntryWidget;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UChatControllerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UChatControllerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void HandleChatInputAction();

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

	UFUNCTION(Server, Reliable)
	void Server_SendChatMessage(const FString& Message);

	UFUNCTION(Client, Reliable)
	void Client_AddChatMessage(const FString& Message);

private:
	bool EnsureChatBox(bool bLogIfMissing = true);
	UChatBoxWidget* FindChatBoxInPlayerHUD() const;

	FString SanitizeChatMessage(const FString& Message) const;
	FString GetSenderDisplayName() const;
	bool CanSendMessage() const;
	void BroadcastChatMessage(const FString& Message);

	UPROPERTY(EditAnywhere, Category = "!Chat|UI")
	TSubclassOf<UChatEntryWidget> ChatEntryWidgetClass;

	UPROPERTY(EditAnywhere, Category = "!Chat|Input", meta = (ClampMin = "1.0"))
	float ScrollMultiplier = 60.0f;

	UPROPERTY(EditAnywhere, Category = "!Chat|Validation", meta = (ClampMin = "1"))
	int32 MaxMessageLength = 120;

	UPROPERTY(EditAnywhere, Category = "!Chat|Validation", meta = (ClampMin = "0.0"))
	float MinSendInterval = 0.2f;

	UPROPERTY(Transient)
	TObjectPtr<UChatBoxWidget> ChatBoxWidget;

	double LastServerSendTime = -1000.0;
};
