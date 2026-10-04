#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
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
class IHudInputInterface;
class FContentLease;
class UContentDataSubsystem;
enum class EInfoUiSection : uint8;

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UControllerInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UControllerInputComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void ReleaseGameplayInput();
	void ReleaseHeldUiInput();
	void RefreshInputDefinition();
	void SetInputDefinition(const TSoftObjectPtr<UControllerInputDefinition>& NewInputDefinition);
	const TSoftObjectPtr<UControllerInputDefinition>& GetInputDefinition() const { return ActiveInputDefinition; }
	UControllerInputDefinition* GetLoadedInputDefinition();

	/** 입력 정의와 그 액션·아이콘을 다 읽어 적용할 때마다 알린다. */
	FSimpleMulticastDelegate& OnInputDefinitionApplied() { return InputDefinitionApplied; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleMoveInput(const FInputActionValue& InputValue);
	void HandleLookInput(const FInputActionValue& InputValue);
	void HandleJumpInputStarted(const FInputActionValue& InputValue);
	void HandleJumpInputEnded(const FInputActionValue& InputValue);
	void HandleCrouchInputStarted(const FInputActionValue& InputValue);
	void HandleCrouchInputEnded(const FInputActionValue& InputValue);
	void HandleInteractInput(const FInputActionValue& InputValue);
	void HandleOpenInfoInputStarted(const FInputActionValue& InputValue, EInfoUiSection Section);
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
	void HandleQuickSlotInputStarted(const FInputActionValue& InputValue, int32 SlotIndex);
	void HandleGestureInputStarted(const FInputActionValue& InputValue, int32 GestureSlotIndex);
	void HandleTargetConfirmInputStarted(const FInputActionValue& InputValue);
	void HandleAbilityInputStarted(const FInputActionValue& InputValue, FGameplayTag InputTag);
	void HandleAbilityInputEnded(const FInputActionValue& InputValue, FGameplayTag InputTag);
	void HandleInputDefinitionPreloadComplete();
	void HandleInputContentPreloadComplete();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	APdPlayerController* GetPdController() const;
	IHudInputInterface* GetHudInput() const;
	APdPlayer* GetPlayerCharacter() const;
	UPlayerRewardComponent* GetPlayerRewardComponent() const;
	UInventoryComponent* GetPlayerInventoryComponent() const;
	UCombatComponent* GetPlayerCombatComponent() const;
	bool IsGameplayInputBlockedByUi() const;
	bool IsOpenLobbyInputAllowed() const;
	void BeginInputDefinitionPreload();
	void ReleaseInputDefinitionPreload();
	UContentDataSubsystem* FindContentDataSubsystem() const;
	bool ApplyInputDefinition();
	void RemoveAppliedInputDefinition();
	void AddInputBindingHandle(uint32 BindingHandle);
	UInputAction* LoadInputAction(const TSoftObjectPtr<UInputAction>& InputAction);
	bool CanSwapPandoraAndWeapon(APdPlayer* PlayerCharacter) const;
	void BindNativeInputActions(UEnhancedInputComponent& EnhancedInputComponent, const UControllerInputDefinition& Definition);

private:
	TSoftObjectPtr<UControllerInputDefinition> ActiveInputDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UControllerInputDefinition> LoadedInputDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AppliedInputMapping;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> LoadedInputActions;

	TArray<uint32> BindingHandles;
	TSharedPtr<FContentLease> InputDefinitionLease;
	TSharedPtr<FContentLease> InputContentLease;
	bool bInputPreloadPending = false;
	bool bAppliedInputDefinition = false;
	bool bSelectPandoraActionOpened = false;
	FSimpleMulticastDelegate InputDefinitionApplied;
};
