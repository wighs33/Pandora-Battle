#include "PlayerComponent/ControllerInputComponent.h"

#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerState.h"
#include "PlayerComponent/CombatComponent.h"
#include "PlayerComponent/ControllerInputDefinition.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "PlayerComponent/PlayerRewardComponent.h"
#include "UI/Widget/InfoWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerInputComponent)

DEFINE_LOG_CATEGORY_STATIC(PdControllerInputComponentLog, Log, All);

UControllerInputComponent::UControllerInputComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

//----------------------------------------------------------------------------------------------------------------------
//--- Engine Callbacks
void UControllerInputComponent::OnRegister()
{
	Super::OnRegister();
	RefreshInputDefinition();
}

void UControllerInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAppliedInputDefinition();
	Super::EndPlay(EndPlayReason);
}

//----------------------------------------------------------------------------------------------------------------------
//--- Input Setup
void UControllerInputComponent::RefreshInputDefinition()
{
	if (!bAppliedInputDefinition)
	{
		ApplyInputDefinition();
	}
}

void UControllerInputComponent::SetInputDefinition(const TSoftObjectPtr<UControllerInputDefinition>& NewInputDefinition)
{
	if (ActiveInputDefinition == NewInputDefinition)
	{
		RefreshInputDefinition();
		return;
	}

	RemoveAppliedInputDefinition();
	ActiveInputDefinition = NewInputDefinition;
	LoadedInputDefinition = nullptr;
	RefreshInputDefinition();
}

UControllerInputDefinition* UControllerInputComponent::LoadInputDefinition()
{
	if (LoadedInputDefinition)
	{
		return LoadedInputDefinition;
	}

	if (ActiveInputDefinition.IsNull())
	{
		return nullptr;
	}

	LoadedInputDefinition = ActiveInputDefinition.LoadSynchronous();
	if (!LoadedInputDefinition)
	{
		UE_LOG(PdControllerInputComponentLog, Warning, TEXT("ControllerInputComponent on '%s' failed to load InputDefinition '%s'."),
			*GetNameSafe(GetOwner()),
			*ActiveInputDefinition.ToString());
	}

	return LoadedInputDefinition;
}

bool UControllerInputComponent::ApplyInputDefinition()
{
	if (bAppliedInputDefinition)
	{
		return true;
	}

	APdPlayerController* Controller = GetPdController();
	if (!Controller || !Controller->IsLocalController())
	{
		return false;
	}

	ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(Controller->InputComponent);
	if (!LocalPlayer || !EnhancedInputComponent)
	{
		return false;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return false;
	}

	UControllerInputDefinition* LoadedDefinition = LoadInputDefinition();
	if (!LoadedDefinition)
	{
		return false;
	}

	const TSoftObjectPtr<UInputMappingContext>& InputMappingAsset = LoadedDefinition->GetInputMapping();
	if (UInputMappingContext* InputMapping = InputMappingAsset.LoadSynchronous())
	{
		InputSubsystem->AddMappingContext(InputMapping, LoadedDefinition->GetPriority());
		AppliedInputMapping = InputMapping;
		bAddedInputMapping = true;
	}
	else if (!InputMappingAsset.IsNull())
	{
		UE_LOG(PdControllerInputComponentLog, Warning, TEXT("ControllerInputComponent on '%s' failed to load input mapping '%s'."),
			*GetNameSafe(Controller),
			*InputMappingAsset.ToString());
	}

	BindNativeInputActions(*EnhancedInputComponent, *LoadedDefinition);

	bAppliedInputDefinition = bAddedInputMapping || !BindingHandles.IsEmpty();
	if (!bAppliedInputDefinition)
	{
		UE_LOG(PdControllerInputComponentLog, Warning, TEXT("ControllerInputComponent applied nothing on '%s'. Check InputDefinition '%s'."),
			*GetNameSafe(Controller),
			*ActiveInputDefinition.ToString());
	}

	return bAppliedInputDefinition;
}

void UControllerInputComponent::RemoveAppliedInputDefinition()
{
	if (!bAppliedInputDefinition && !bAddedInputMapping && BindingHandles.IsEmpty())
	{
		LoadedInputActions.Reset();
		return;
	}

	if (APdPlayerController* Controller = GetPdController())
	{
		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(Controller->InputComponent))
		{
			for (uint32 BindingHandle : BindingHandles)
			{
				EnhancedInputComponent->RemoveBindingByHandle(BindingHandle);
			}
		}

		if (Controller->IsLocalController() && bAddedInputMapping)
		{
			if (ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					if (AppliedInputMapping)
					{
						InputSubsystem->RemoveMappingContext(AppliedInputMapping);
					}
				}
			}
		}
	}

	BindingHandles.Reset();
	LoadedInputActions.Reset();
	AppliedInputMapping = nullptr;
	bAppliedInputDefinition = false;
	bAddedInputMapping = false;
}

void UControllerInputComponent::AddInputBindingHandle(uint32 BindingHandle)
{
	if (BindingHandle != 0)
	{
		BindingHandles.Add(BindingHandle);
	}
}

UInputAction* UControllerInputComponent::LoadInputAction(const TSoftObjectPtr<UInputAction>& InputAction)
{
	if (InputAction.IsNull())
	{
		return nullptr;
	}

	UInputAction* LoadedInputAction = InputAction.LoadSynchronous();
	if (LoadedInputAction)
	{
		LoadedInputActions.Add(LoadedInputAction);
	}

	return LoadedInputAction;
}

void UControllerInputComponent::BindNativeInputActions(UEnhancedInputComponent& EnhancedInputComponent,
	const UControllerInputDefinition& Definition)
{
	if (UInputAction* MoveAction = LoadInputAction(Definition.GetMoveInputAction()))
	{
		FEnhancedInputActionEventBinding& Binding = EnhancedInputComponent.BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::HandleMoveInput);
		AddInputBindingHandle(Binding.GetHandle());
	}

	if (UInputAction* LookAction = LoadInputAction(Definition.GetLookInputAction()))
	{
		FEnhancedInputActionEventBinding& Binding = EnhancedInputComponent.BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::HandleLookInput);
		AddInputBindingHandle(Binding.GetHandle());
	}

	if (UInputAction* JumpAction = LoadInputAction(Definition.GetJumpInputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding = EnhancedInputComponent.BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::HandleJumpInputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CompletedBinding = EnhancedInputComponent.BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::HandleJumpInputEnded);
		AddInputBindingHandle(CompletedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CanceledBinding = EnhancedInputComponent.BindAction(JumpAction, ETriggerEvent::Canceled, this, &ThisClass::HandleJumpInputEnded);
		AddInputBindingHandle(CanceledBinding.GetHandle());
	}

	if (UInputAction* CrouchAction = LoadInputAction(Definition.GetCrouchInputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding = EnhancedInputComponent.BindAction(CrouchAction, ETriggerEvent::Started, this, &ThisClass::HandleCrouchInputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CompletedBinding = EnhancedInputComponent.BindAction(CrouchAction, ETriggerEvent::Completed, this, &ThisClass::HandleCrouchInputEnded);
		AddInputBindingHandle(CompletedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CanceledBinding = EnhancedInputComponent.BindAction(CrouchAction, ETriggerEvent::Canceled, this, &ThisClass::HandleCrouchInputEnded);
		AddInputBindingHandle(CanceledBinding.GetHandle());
	}

	if (UInputAction* InteractAction = LoadInputAction(Definition.GetInteractInputAction()))
	{
		FEnhancedInputActionEventBinding& Binding = EnhancedInputComponent.BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::HandleInteractInput);
		AddInputBindingHandle(Binding.GetHandle());
	}

	if (UInputAction* AttackAction = LoadInputAction(Definition.GetAttackInputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding = EnhancedInputComponent.BindAction(AttackAction, ETriggerEvent::Started, this, &ThisClass::HandleAttackInputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CompletedBinding = EnhancedInputComponent.BindAction(AttackAction, ETriggerEvent::Completed, this, &ThisClass::HandleAttackInputEnded);
		AddInputBindingHandle(CompletedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CanceledBinding = EnhancedInputComponent.BindAction(AttackAction, ETriggerEvent::Canceled, this, &ThisClass::HandleAttackInputEnded);
		AddInputBindingHandle(CanceledBinding.GetHandle());
	}

	if (UInputAction* AimAction = LoadInputAction(Definition.GetAimInputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding = EnhancedInputComponent.BindAction(AimAction, ETriggerEvent::Started, this, &ThisClass::HandleAimInputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CompletedBinding = EnhancedInputComponent.BindAction(AimAction, ETriggerEvent::Completed, this, &ThisClass::HandleAimInputEnded);
		AddInputBindingHandle(CompletedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CanceledBinding = EnhancedInputComponent.BindAction(AimAction, ETriggerEvent::Canceled, this, &ThisClass::HandleAimInputEnded);
		AddInputBindingHandle(CanceledBinding.GetHandle());
	}

	if (UInputAction* OpenInfoUiAction = LoadInputAction(Definition.GetOpenInfoUiInputAction()))
	{
		FEnhancedInputActionEventBinding& Binding = EnhancedInputComponent.BindAction(OpenInfoUiAction, ETriggerEvent::Started, this, &ThisClass::HandleOpenInfoUiInputStarted);
		AddInputBindingHandle(Binding.GetHandle());
	}

	if (UInputAction* SelectPandoraAction = LoadInputAction(Definition.GetSelectPandoraInputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding = EnhancedInputComponent.BindAction(SelectPandoraAction, ETriggerEvent::Started, this, &ThisClass::HandleSelectPandoraInputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CompletedBinding = EnhancedInputComponent.BindAction(SelectPandoraAction, ETriggerEvent::Completed, this, &ThisClass::HandleSelectPandoraInputEnded);
		AddInputBindingHandle(CompletedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CanceledBinding = EnhancedInputComponent.BindAction(SelectPandoraAction, ETriggerEvent::Canceled, this, &ThisClass::HandleSelectPandoraInputEnded);
		AddInputBindingHandle(CanceledBinding.GetHandle());
	}

	if (UInputAction* PandoraTreeAction = LoadInputAction(Definition.GetPandoraTreeInputAction()))
	{
		FEnhancedInputActionEventBinding& Binding = EnhancedInputComponent.BindAction(PandoraTreeAction, ETriggerEvent::Started, this, &ThisClass::HandlePandoraTreeInputStarted);
		AddInputBindingHandle(Binding.GetHandle());
	}

	if (UInputAction* Skill1Action = LoadInputAction(Definition.GetSkill1InputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding = EnhancedInputComponent.BindAction(Skill1Action, ETriggerEvent::Started, this, &ThisClass::HandleSkill1InputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CompletedBinding = EnhancedInputComponent.BindAction(Skill1Action, ETriggerEvent::Completed, this, &ThisClass::HandleSkill1InputEnded);
		AddInputBindingHandle(CompletedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CanceledBinding = EnhancedInputComponent.BindAction(Skill1Action, ETriggerEvent::Canceled, this, &ThisClass::HandleSkill1InputEnded);
		AddInputBindingHandle(CanceledBinding.GetHandle());
	}

	if (UInputAction* Skill2Action = LoadInputAction(Definition.GetSkill2InputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding = EnhancedInputComponent.BindAction(Skill2Action, ETriggerEvent::Started, this, &ThisClass::HandleSkill2InputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CompletedBinding = EnhancedInputComponent.BindAction(Skill2Action, ETriggerEvent::Completed, this, &ThisClass::HandleSkill2InputEnded);
		AddInputBindingHandle(CompletedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CanceledBinding = EnhancedInputComponent.BindAction(Skill2Action, ETriggerEvent::Canceled, this, &ThisClass::HandleSkill2InputEnded);
		AddInputBindingHandle(CanceledBinding.GetHandle());
	}

	if (UInputAction* Skill3Action = LoadInputAction(Definition.GetSkill3InputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding = EnhancedInputComponent.BindAction(Skill3Action, ETriggerEvent::Started, this, &ThisClass::HandleSkill3InputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CompletedBinding = EnhancedInputComponent.BindAction(Skill3Action, ETriggerEvent::Completed, this, &ThisClass::HandleSkill3InputEnded);
		AddInputBindingHandle(CompletedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CanceledBinding = EnhancedInputComponent.BindAction(Skill3Action, ETriggerEvent::Canceled, this, &ThisClass::HandleSkill3InputEnded);
		AddInputBindingHandle(CanceledBinding.GetHandle());
	}

	if (UInputAction* Skill4Action = LoadInputAction(Definition.GetSkill4InputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding = EnhancedInputComponent.BindAction(Skill4Action, ETriggerEvent::Started, this, &ThisClass::HandleSkill4InputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CompletedBinding = EnhancedInputComponent.BindAction(Skill4Action, ETriggerEvent::Completed, this, &ThisClass::HandleSkill4InputEnded);
		AddInputBindingHandle(CompletedBinding.GetHandle());
		FEnhancedInputActionEventBinding& CanceledBinding = EnhancedInputComponent.BindAction(Skill4Action, ETriggerEvent::Canceled, this, &ThisClass::HandleSkill4InputEnded);
		AddInputBindingHandle(CanceledBinding.GetHandle());
	}

	if (UInputAction* TargetConfirmAction = LoadInputAction(Definition.GetTargetConfirmInputAction()))
	{
		FEnhancedInputActionEventBinding& StartedBinding =
			EnhancedInputComponent.BindAction(TargetConfirmAction, ETriggerEvent::Started, this, &ThisClass::HandleTargetConfirmInputStarted);
		AddInputBindingHandle(StartedBinding.GetHandle());
	}
}

void UControllerInputComponent::HandleMoveInput(const FInputActionValue& InputValue)
{
	APdPlayerController* Controller = GetPdController();
	APawn* ControlledPawn = Controller ? Controller->GetPawn() : nullptr;
	if (!ControlledPawn)
	{
		return;
	}

	const FVector2D MoveValue = InputValue.Get<FVector2D>();
	if (MoveValue.IsNearlyZero())
	{
		return;
	}

	if (const APdHUD* HUD = GetPdHUD())
	{
		const UInfoWidget* InfoWidget = HUD->GetInfoWidget();
		if (InfoWidget && InfoWidget->IsInViewport())
		{
			return;
		}
	}

	APdPlayer* PlayerCharacter = Cast<APdPlayer>(ControlledPawn);
	const UEquipmentComponent* EquipmentComponent = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
	const UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter ? PlayerCharacter->GetAbilitySystemComponent() : nullptr;
	const FGameplayTag MovementBlockStateTag = LoadedInputDefinition ? LoadedInputDefinition->GetMovementBlockStateTag() : FGameplayTag();
	if (AbilitySystemComponent
		&& MovementBlockStateTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(MovementBlockStateTag)
		&& EquipmentComponent
		&& !EquipmentComponent->AllowsMovementDuringAttack())
	{
		return;
	}

	const FRotator CurrentControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.f, CurrentControlRotation.Yaw, 0.f);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	if (PlayerCharacter)
	{
		PlayerCharacter->StopInteractionMontage();
	}

	ControlledPawn->AddMovementInput(RightDirection, static_cast<float>(MoveValue.X));
	ControlledPawn->AddMovementInput(ForwardDirection, static_cast<float>(MoveValue.Y));
}

void UControllerInputComponent::HandleLookInput(const FInputActionValue& InputValue)
{
	APdPlayerController* Controller = GetPdController();
	if (!Controller)
	{
		return;
	}

	const FVector2D LookValue = InputValue.Get<FVector2D>();
	if (LookValue.IsNearlyZero())
	{
		return;
	}

	Controller->AddYawInput(static_cast<float>(LookValue.X));
	Controller->AddPitchInput(static_cast<float>(LookValue.Y));
}

void UControllerInputComponent::HandleJumpInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	const APdPlayerController* Controller = GetPdController();
	if (ACharacter* PlayerCharacter = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr)
	{
		PlayerCharacter->Jump();
	}
}

void UControllerInputComponent::HandleJumpInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	const APdPlayerController* Controller = GetPdController();
	if (ACharacter* PlayerCharacter = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr)
	{
		PlayerCharacter->StopJumping();
	}
}

void UControllerInputComponent::HandleCrouchInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	const APdPlayerController* Controller = GetPdController();
	if (ACharacter* PlayerCharacter = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr)
	{
		PlayerCharacter->Crouch();
	}
}

void UControllerInputComponent::HandleCrouchInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	const APdPlayerController* Controller = GetPdController();
	if (ACharacter* PlayerCharacter = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr)
	{
		PlayerCharacter->UnCrouch();
	}
}

void UControllerInputComponent::HandleInteractInput(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (IsGameplayInputBlockedByUi())
	{
		UE_LOG(PdControllerInputComponentLog, Warning,
			TEXT("[InteractInput] blocked by UI. controller=%s hud=%s"),
			*GetNameSafe(GetPdController()),
			*GetNameSafe(GetPdHUD()));
		return;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		UE_LOG(PdControllerInputComponentLog, Warning,
			TEXT("[InteractInput] failed: no player character. controller=%s"),
			*GetNameSafe(GetPdController()));
		return;
	}

	AActor* InteractableActor = PlayerCharacter->GetCurrentInteractActor();
	if (!IsValid(InteractableActor))
	{
		UE_LOG(PdControllerInputComponentLog, Warning,
			TEXT("[InteractInput] failed: no current interact target. player=%s"),
			*GetNameSafe(PlayerCharacter));
		return;
	}

	UE_LOG(PdControllerInputComponentLog, Verbose,
		TEXT("[InteractInput] pressed. player=%s target=%s"),
		*GetNameSafe(PlayerCharacter),
		*GetNameSafe(InteractableActor));

	if (PlayerCharacter->InteractWithCurrentTarget())
	{
		return;
	}

	UPlayerRewardComponent* PlayerRewardComponent = GetPlayerRewardComponent();
	if (!PlayerRewardComponent)
	{
		UE_LOG(PdControllerInputComponentLog, Warning,
			TEXT("[InteractInput] failed: target did not handle interaction and reward component is missing. player=%s target=%s"),
			*GetNameSafe(PlayerCharacter),
			*GetNameSafe(InteractableActor));
		return;
	}

	UE_LOG(PdControllerInputComponentLog, Verbose,
		TEXT("[InteractInput] target deferred to reward component. player=%s target=%s rewardComponent=%s"),
		*GetNameSafe(PlayerCharacter),
		*GetNameSafe(InteractableActor),
		*GetNameSafe(PlayerRewardComponent));
	PlayerRewardComponent->ApplyInteractRewards(InteractableActor);
}

void UControllerInputComponent::HandleOpenInfoUiInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OnOpenInfoUiInputStarted(InputValue);
	}
}

void UControllerInputComponent::HandleSelectPandoraInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OnSelectPandoraInputStarted(InputValue);
	}
}

void UControllerInputComponent::HandleSelectPandoraInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OnSelectPandoraInputEnded(InputValue);
	}
}

void UControllerInputComponent::HandlePandoraTreeInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OnPandoraTreeInputStarted(InputValue);
	}
}

void UControllerInputComponent::HandleAttackInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (UCombatComponent* CombatComponent = GetPlayerCombatComponent())
	{
		CombatComponent->StartPrimaryAttack();
	}
}

void UControllerInputComponent::HandleAttackInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (UCombatComponent* CombatComponent = GetPlayerCombatComponent())
	{
		CombatComponent->StopPrimaryAttack();
	}
}

void UControllerInputComponent::HandleAimInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (UCombatComponent* CombatComponent = GetPlayerCombatComponent())
	{
		CombatComponent->StartAim();
	}
}

void UControllerInputComponent::HandleAimInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (UCombatComponent* CombatComponent = GetPlayerCombatComponent())
	{
		CombatComponent->StopAim();
	}
}

void UControllerInputComponent::HandleDashInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	UPdAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter ? PlayerCharacter->GetPdAbilitySystemComponent() : nullptr;
	if (AbilitySystemComponent)
	{
		FVector DashDirection = PlayerCharacter->GetLastMovementInputVector();
		DashDirection.Z = 0.0f;
		if (DashDirection.IsNearlyZero())
		{
			DashDirection = PlayerCharacter->GetActorForwardVector();
			DashDirection.Z = 0.0f;
		}

		if (!DashDirection.IsNearlyZero())
		{
			DashDirection.Normalize();
		}

		FHitResult DirectionPayload;
		DirectionPayload.Location = DashDirection;

		FGameplayEventData EventData;
		EventData.EventTag = LabGameplayTags::Event_ActivateAbility_Dash;
		EventData.Instigator = PlayerCharacter;
		EventData.Target = PlayerCharacter;
		EventData.TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(DirectionPayload));

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(PlayerCharacter, LabGameplayTags::Event_ActivateAbility_Dash, EventData);
	}
}

void UControllerInputComponent::HandleDashInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	UPdAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter ? PlayerCharacter->GetPdAbilitySystemComponent() : nullptr;
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagReleased(LabGameplayTags::Input_Ability_Movement_Dash);
	}
}

void UControllerInputComponent::HandleSkill1InputStarted(const FInputActionValue& InputValue)
{
	HandleAbilityInputStarted(InputValue, LabGameplayTags::Input_Ability_Skill1);
}

void UControllerInputComponent::HandleSkill1InputEnded(const FInputActionValue& InputValue)
{
	HandleAbilityInputEnded(InputValue, LabGameplayTags::Input_Ability_Skill1);
}

void UControllerInputComponent::HandleSkill2InputStarted(const FInputActionValue& InputValue)
{
	HandleAbilityInputStarted(InputValue, LabGameplayTags::Input_Ability_Skill2);
}

void UControllerInputComponent::HandleSkill2InputEnded(const FInputActionValue& InputValue)
{
	HandleAbilityInputEnded(InputValue, LabGameplayTags::Input_Ability_Skill2);
}

void UControllerInputComponent::HandleSkill3InputStarted(const FInputActionValue& InputValue)
{
	HandleAbilityInputStarted(InputValue, LabGameplayTags::Input_Ability_Skill3);
}

void UControllerInputComponent::HandleSkill3InputEnded(const FInputActionValue& InputValue)
{
	HandleAbilityInputEnded(InputValue, LabGameplayTags::Input_Ability_Skill3);
}

void UControllerInputComponent::HandleSkill4InputStarted(const FInputActionValue& InputValue)
{
	HandleAbilityInputStarted(InputValue, LabGameplayTags::Input_Ability_Skill4);
}

void UControllerInputComponent::HandleSkill4InputEnded(const FInputActionValue& InputValue)
{
	HandleAbilityInputEnded(InputValue, LabGameplayTags::Input_Ability_Skill4);
}

void UControllerInputComponent::HandleTargetConfirmInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	UPdAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter ? PlayerCharacter->GetPdAbilitySystemComponent() : nullptr;
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->LocalInputConfirm();
	}
}

void UControllerInputComponent::HandleAbilityInputStarted(const FInputActionValue& InputValue, const FGameplayTag& InputTag)
{
	static_cast<void>(InputValue);

	if (IsGameplayInputBlockedByUi() || !InputTag.IsValid())
	{
		return;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	UPdAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter ? PlayerCharacter->GetPdAbilitySystemComponent() : nullptr;
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagPressed(InputTag);
	}
}

void UControllerInputComponent::HandleAbilityInputEnded(const FInputActionValue& InputValue, const FGameplayTag& InputTag)
{
	static_cast<void>(InputValue);

	if (!InputTag.IsValid())
	{
		return;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	UPdAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter ? PlayerCharacter->GetPdAbilitySystemComponent() : nullptr;
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityInputTagReleased(InputTag);
	}
}

APdPlayerController* UControllerInputComponent::GetPdController() const
{
	return Cast<APdPlayerController>(GetOwner());
}

APdHUD* UControllerInputComponent::GetPdHUD() const
{
	const APdPlayerController* Controller = GetPdController();
	return Controller ? Cast<APdHUD>(Controller->GetHUD()) : nullptr;
}

APdPlayer* UControllerInputComponent::GetPlayerCharacter() const
{
	const APdPlayerController* Controller = GetPdController();
	return Controller ? Cast<APdPlayer>(Controller->GetPawn()) : nullptr;
}


UPlayerRewardComponent* UControllerInputComponent::GetPlayerRewardComponent() const
{
	const APdPlayerController* Controller = GetPdController();
	APdPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APdPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetPlayerRewardComponent() : nullptr;
}

UCombatComponent* UControllerInputComponent::GetPlayerCombatComponent() const
{
	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	return PlayerCharacter ? PlayerCharacter->GetCombatComponent() : nullptr;
}

bool UControllerInputComponent::IsGameplayInputBlockedByUi() const
{
	const APdHUD* HUD = GetPdHUD();
	return HUD && HUD->IsGameplayInputBlockedByUi();
}
