#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "Definition/Player/CharacterActionDefinition.h"
#include "ControllerInputComponent.generated.h"

class APdPlayer;
class APdPlayerController;
class UControllerInputDefinition;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UCombatComponent;
class UPlayerRewardComponent;
class UInventoryComponent;
class APdHUD;
struct FStreamableHandle;

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UControllerInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UControllerInputComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Input Setup
	void RefreshInputDefinition();
	void SetInputDefinition(const TSoftObjectPtr<UControllerInputDefinition>& NewInputDefinition);
	const TSoftObjectPtr<UControllerInputDefinition>& GetInputDefinition() const { return ActiveInputDefinition; }
	UControllerInputDefinition* GetLoadedInputDefinition();

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Input Events
	void HandleMoveInput(const FInputActionValue& InputValue);
	void HandleLookInput(const FInputActionValue& InputValue);
	void HandleJumpInputStarted(const FInputActionValue& InputValue);
	void HandleJumpInputEnded(const FInputActionValue& InputValue);
	void HandleCrouchInputStarted(const FInputActionValue& InputValue);
	void HandleCrouchInputEnded(const FInputActionValue& InputValue);
	void HandleInteractInput(const FInputActionValue& InputValue);
	void HandleOpenInfoProfileInputStarted(const FInputActionValue& InputValue);
	void HandleOpenInfoItemInputStarted(const FInputActionValue& InputValue);
	void HandleOpenInfoSkinInputStarted(const FInputActionValue& InputValue);
	void HandleOpenInfoPandoraInputStarted(const FInputActionValue& InputValue);
	void HandleOpenInfoMapInputStarted(const FInputActionValue& InputValue);
	void HandleOpenSettingUiInputStarted(const FInputActionValue& InputValue);
	void HandleEscapeInputStarted(const FInputActionValue& InputValue);
	void HandleOpenLobbyInputStarted(const FInputActionValue& InputValue);
	void HandleSelectPandoraInputStarted(const FInputActionValue& InputValue);
	void HandleSelectPandoraInputEnded(const FInputActionValue& InputValue);
	void HandlePandoraTreeInputStarted(const FInputActionValue& InputValue);
	void HandleScoreboardInputStarted(const FInputActionValue& InputValue);
	void HandleScoreboardInputEnded(const FInputActionValue& InputValue);
	void HandleChatInputStarted(const FInputActionValue& InputValue);
	void HandleChatScrollInputTriggered(const FInputActionValue& InputValue);
	void HandleAttackInputStarted(const FInputActionValue& InputValue);
	void HandleAttackInputEnded(const FInputActionValue& InputValue);
	void HandleAimInputStarted(const FInputActionValue& InputValue);
	void HandleAimInputEnded(const FInputActionValue& InputValue);
	void HandleGrappleInputStarted(const FInputActionValue& InputValue);
	void HandleGrappleInputEnded(const FInputActionValue& InputValue);
	void HandleSkill1InputStarted(const FInputActionValue& InputValue);
	void HandleSkill1InputEnded(const FInputActionValue& InputValue);
	void HandleSkill2InputStarted(const FInputActionValue& InputValue);
	void HandleSkill2InputEnded(const FInputActionValue& InputValue);
	void HandleSkill3InputStarted(const FInputActionValue& InputValue);
	void HandleSkill3InputEnded(const FInputActionValue& InputValue);
	void HandleSkill4InputStarted(const FInputActionValue& InputValue);
	void HandleSkill4InputEnded(const FInputActionValue& InputValue);
	void HandleQuickSlot1InputStarted(const FInputActionValue& InputValue);
	void HandleQuickSlot2InputStarted(const FInputActionValue& InputValue);
	void HandleQuickSlot3InputStarted(const FInputActionValue& InputValue);
	void HandleQuickSlot4InputStarted(const FInputActionValue& InputValue);
	void HandleQuickSlotInputStarted(const FInputActionValue& InputValue, int32 SlotIndex);
	void HandleGesture1InputStarted(const FInputActionValue& InputValue);
	void HandleGesture2InputStarted(const FInputActionValue& InputValue);
	void HandleGesture3InputStarted(const FInputActionValue& InputValue);
	void HandleGesture4InputStarted(const FInputActionValue& InputValue);
	void HandleGestureInputStarted(const FInputActionValue& InputValue, int32 GestureSlotIndex);
	void HandleTargetConfirmInputStarted(const FInputActionValue& InputValue);
	void HandleAbilityInputStarted(const FInputActionValue& InputValue, const FGameplayTag& InputTag);
	void HandleAbilityInputEnded(const FInputActionValue& InputValue, const FGameplayTag& InputTag);

	//------------------------------------------------------------------------------------------------------------------
	//--- Controller Services
	APdPlayerController* GetPdController() const;
	APdHUD* GetPdHUD() const;
	APdPlayer* GetPlayerCharacter() const;
	UPlayerRewardComponent* GetPlayerRewardComponent() const;
	UInventoryComponent* GetPlayerInventoryComponent() const;
	UCombatComponent* GetPlayerCombatComponent() const;
	bool IsGameplayInputBlockedByUi() const;
	bool IsOpenLobbyInputAllowed() const;
	void BeginInputDefinitionPreload();
	void HandleInputDefinitionPreloadComplete(uint32 RequestGeneration);
	void HandleInputContentPreloadComplete(uint32 RequestGeneration);
	void ReleaseInputDefinitionPreload();
	bool ApplyInputDefinition();
	void RemoveAppliedInputDefinition();
	void AddInputBindingHandle(uint32 BindingHandle);
	UInputAction* LoadInputAction(const TSoftObjectPtr<UInputAction>& InputAction);
	const UCharacterActionDefinition* LoadCharacterActionDefinition();
	bool IsCharacterActionAvailable(APdPlayer* PlayerCharacter, ECharacterActionType ActionType) const;
	void BindNativeInputActions(UEnhancedInputComponent& EnhancedInputComponent, const UControllerInputDefinition& Definition);

	//------------------------------------------------------------------------------------------------------------------
	//--- Input Definition
	TSoftObjectPtr<UControllerInputDefinition> ActiveInputDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UControllerInputDefinition> LoadedInputDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterActionDefinition> LoadedCharacterActionDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AppliedInputMapping;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> LoadedInputActions;

	TArray<uint32> BindingHandles;
	TSharedPtr<FStreamableHandle> InputDefinitionLoadHandle;
	TSharedPtr<FStreamableHandle> InputContentLoadHandle;
	uint32 InputPreloadRequestGeneration = 0;
	bool bInputPreloadPending = false;
	bool bAppliedInputDefinition = false;
	bool bAddedInputMapping = false;
	bool bSelectPandoraActionOpened = false;
};
