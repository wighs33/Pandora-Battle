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
class UControllerUiComponent;
class UPlayerRewardComponent;

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
	// 이동 입력
	void HandleMoveInput(const FInputActionValue& InputValue);

	// 시야 입력
	void HandleLookInput(const FInputActionValue& InputValue);

	// 점프 시작
	void HandleJumpInputStarted(const FInputActionValue& InputValue);

	// 점프 종료
	void HandleJumpInputEnded(const FInputActionValue& InputValue);

	// 앉기 시작
	void HandleCrouchInputStarted(const FInputActionValue& InputValue);

	// 앉기 종료
	void HandleCrouchInputEnded(const FInputActionValue& InputValue);

	// 상호작용 입력
	void HandleInteractInput(const FInputActionValue& InputValue);

	void HandleOpenInfoUiInputStarted(const FInputActionValue& InputValue);
	void HandleSelectPandoraInputStarted(const FInputActionValue& InputValue);
	void HandleSelectPandoraInputEnded(const FInputActionValue& InputValue);
	void HandleAttackInputStarted(const FInputActionValue& InputValue);
	void HandleAttackInputEnded(const FInputActionValue& InputValue);
	void HandleAimInputStarted(const FInputActionValue& InputValue);
	void HandleAimInputEnded(const FInputActionValue& InputValue);

	//------------------------------------------------------------------------------------------------------------------
	//--- Controller Services
	APdPlayerController* GetPdController() const;
	APdPlayer* GetPlayerCharacter() const;
	UPlayerRewardComponent* GetPlayerRewardComponent() const;
	UCombatComponent* GetPlayerCombatComponent() const;
	UControllerUiComponent* GetControllerUiComponent() const;
	bool IsGameplayInputBlockedByUi() const;
	UControllerInputDefinition* LoadInputDefinition();
	bool ApplyInputDefinition();
	void RemoveAppliedInputDefinition();
	void AddInputBindingHandle(uint32 BindingHandle);
	UInputAction* LoadInputAction(const TSoftObjectPtr<UInputAction>& InputAction);
	void BindNativeInputActions(UEnhancedInputComponent& EnhancedInputComponent, const UControllerInputDefinition& Definition);
	void BindAbilityInputActions(UEnhancedInputComponent& EnhancedInputComponent, const UControllerInputDefinition& Definition);
	void HandleAbilityInputStarted(const FInputActionValue& InputValue, FGameplayTag InputTag);
	void HandleAbilityInputEnded(const FInputActionValue& InputValue, FGameplayTag InputTag);

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
