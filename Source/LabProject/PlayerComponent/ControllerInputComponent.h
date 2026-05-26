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
class APdHUD;

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

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Input Events
	// ?¥Îèô ?ÖÎ†•
	void HandleMoveInput(const FInputActionValue& InputValue);

	// ?úÏïº ?ÖÎ†•
	void HandleLookInput(const FInputActionValue& InputValue);

	// ?êÌîÑ ?úÏûë
	void HandleJumpInputStarted(const FInputActionValue& InputValue);

	// ?êÌîÑ Ï¢ÖÎ£å
	void HandleJumpInputEnded(const FInputActionValue& InputValue);

	// ?âÍ∏∞ ?úÏûë
	void HandleCrouchInputStarted(const FInputActionValue& InputValue);

	// ?âÍ∏∞ Ï¢ÖÎ£å
	void HandleCrouchInputEnded(const FInputActionValue& InputValue);

	// ?ÅÌò∏?ëÏö© ?ÖÎ†•
	void HandleInteractInput(const FInputActionValue& InputValue);

	void HandleOpenInfoUiInputStarted(const FInputActionValue& InputValue);
	void HandleSelectPandoraInputStarted(const FInputActionValue& InputValue);
	void HandleSelectPandoraInputEnded(const FInputActionValue& InputValue);
	void HandlePandoraTreeInputStarted(const FInputActionValue& InputValue);
	void HandleAttackInputStarted(const FInputActionValue& InputValue);
	void HandleAttackInputEnded(const FInputActionValue& InputValue);
	void HandleAimInputStarted(const FInputActionValue& InputValue);
	void HandleAimInputEnded(const FInputActionValue& InputValue);
	void HandleDashInputStarted(const FInputActionValue& InputValue);
	void HandleDashInputEnded(const FInputActionValue& InputValue);
	void HandleSkill1InputStarted(const FInputActionValue& InputValue);
	void HandleSkill1InputEnded(const FInputActionValue& InputValue);
	void HandleSkill2InputStarted(const FInputActionValue& InputValue);
	void HandleSkill2InputEnded(const FInputActionValue& InputValue);
	void HandleSkill3InputStarted(const FInputActionValue& InputValue);
	void HandleSkill3InputEnded(const FInputActionValue& InputValue);
	void HandleSkill4InputStarted(const FInputActionValue& InputValue);
	void HandleSkill4InputEnded(const FInputActionValue& InputValue);
	void HandleTargetConfirmInputStarted(const FInputActionValue& InputValue);
	void HandleAbilityInputStarted(const FInputActionValue& InputValue, const FGameplayTag& InputTag);
	void HandleAbilityInputEnded(const FInputActionValue& InputValue, const FGameplayTag& InputTag);

	//------------------------------------------------------------------------------------------------------------------
	//--- Controller Services
	APdPlayerController* GetPdController() const;
	APdHUD* GetPdHUD() const;
	APdPlayer* GetPlayerCharacter() const;
	UPlayerRewardComponent* GetPlayerRewardComponent() const;
	UCombatComponent* GetPlayerCombatComponent() const;
	bool IsGameplayInputBlockedByUi() const;
	UControllerInputDefinition* LoadInputDefinition();
	bool ApplyInputDefinition();
	void RemoveAppliedInputDefinition();
	void AddInputBindingHandle(uint32 BindingHandle);
	UInputAction* LoadInputAction(const TSoftObjectPtr<UInputAction>& InputAction);
	void BindNativeInputActions(UEnhancedInputComponent& EnhancedInputComponent, const UControllerInputDefinition& Definition);

	//------------------------------------------------------------------------------------------------------------------
	//--- Input Definition
	TSoftObjectPtr<UControllerInputDefinition> ActiveInputDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UControllerInputDefinition> LoadedInputDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AppliedInputMapping;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> LoadedInputActions;

	TArray<uint32> BindingHandles;
	bool bAppliedInputDefinition = false;
	bool bAddedInputMapping = false;
};
