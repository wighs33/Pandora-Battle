#include "Component/Player/ControllerInputComponent.h"

#include "AbilitySystemComponent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Character/PdPlayer.h"
#include "Component/Chat/ChatControllerComponent.h"
#include "Common/LabGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Component/Item/InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdHUD.h"
#include "Mode/PdPlayerState.h"
#include "Component/Player/CombatComponent.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/PlayerRewardComponent.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "UI/InfoUiTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControllerInputComponent)

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
	if (bAppliedInputDefinition || bInputPreloadPending)
	{
		return;
	}

	if (!LoadInputDefinition()
		|| !InputContentLoadHandle.IsValid())
	{
		BeginInputDefinitionPreload();
		return;
	}

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
	LoadedCharacterActionDefinition = nullptr;
	RefreshInputDefinition();
}

UControllerInputDefinition* UControllerInputComponent::GetLoadedInputDefinition()
{
	return LoadInputDefinition();
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

	LoadedInputDefinition = ActiveInputDefinition.Get();
	return LoadedInputDefinition;
}

void UControllerInputComponent::BeginInputDefinitionPreload()
{
	if (bInputPreloadPending || ActiveInputDefinition.IsNull())
	{
		return;
	}

	ReleaseInputDefinitionPreload();
	bInputPreloadPending = true;
	const uint32 RequestGeneration = InputPreloadRequestGeneration;
	if (LoadInputDefinition())
	{
		HandleInputDefinitionPreloadComplete(RequestGeneration);
		return;
	}

	InputDefinitionLoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			ActiveInputDefinition.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleInputDefinitionPreloadComplete,
				RequestGeneration));
	if (!InputDefinitionLoadHandle.IsValid())
	{
		bInputPreloadPending = false;
	}
}

void UControllerInputComponent::HandleInputDefinitionPreloadComplete(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != InputPreloadRequestGeneration)
	{
		return;
	}

	LoadedInputDefinition = ActiveInputDefinition.Get();
	if (!LoadedInputDefinition)
	{
		bInputPreloadPending = false;
		return;
	}

	TArray<FSoftObjectPath> RuntimeAssetPaths;
	RuntimeAssetPaths.Add(ActiveInputDefinition.ToSoftObjectPath());
	LoadedInputDefinition->GetRuntimePreloadAssetPaths(RuntimeAssetPaths);
	InputContentLoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			RuntimeAssetPaths,
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleInputContentPreloadComplete,
				RequestGeneration));
	if (InputDefinitionLoadHandle.IsValid())
	{
		InputDefinitionLoadHandle->ReleaseHandle();
		InputDefinitionLoadHandle.Reset();
	}
	if (!InputContentLoadHandle.IsValid())
	{
		HandleInputContentPreloadComplete(RequestGeneration);
	}
}

void UControllerInputComponent::HandleInputContentPreloadComplete(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != InputPreloadRequestGeneration)
	{
		return;
	}

	bInputPreloadPending = false;
	ApplyInputDefinition();
}

void UControllerInputComponent::ReleaseInputDefinitionPreload()
{
	++InputPreloadRequestGeneration;
	bInputPreloadPending = false;
	if (InputDefinitionLoadHandle.IsValid())
	{
		InputDefinitionLoadHandle->CancelHandle();
		InputDefinitionLoadHandle->ReleaseHandle();
		InputDefinitionLoadHandle.Reset();
	}
	if (InputContentLoadHandle.IsValid())
	{
		InputContentLoadHandle->CancelHandle();
		InputContentLoadHandle->ReleaseHandle();
		InputContentLoadHandle.Reset();
	}
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

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(Controller->InputComponent);
	ULocalPlayerSettingsSubsystem* LocalPlayerSettings = ULocalPlayerSettingsSubsystem::Get(Controller);
	if (!EnhancedInputComponent || !LocalPlayerSettings)
	{
		return false;
	}

	UControllerInputDefinition* LoadedDefinition = LoadInputDefinition();
	if (!LoadedDefinition)
	{
		return false;
	}

	const TSoftObjectPtr<UInputMappingContext>& InputMappingAsset = LoadedDefinition->GetInputMapping();
	if (UInputMappingContext* InputMapping = InputMappingAsset.Get())
	{
		if (LocalPlayerSettings->AddInputMappingContext(InputMapping, LoadedDefinition->GetPriority()))
		{
			AppliedInputMapping = InputMapping;
			bAddedInputMapping = true;
		}
	}

	BindNativeInputActions(*EnhancedInputComponent, *LoadedDefinition);

	bAppliedInputDefinition = bAddedInputMapping || !BindingHandles.IsEmpty();

	return bAppliedInputDefinition;
}

void UControllerInputComponent::RemoveAppliedInputDefinition()
{
	ReleaseInputDefinitionPreload();
	if (APdPlayer* PlayerCharacter = GetPlayerCharacter())
	{
		if (UPdAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter->GetPdAbilitySystemComponent())
		{
			AbilitySystemComponent->AbilityInputTagReleased(
				LabGameplayTags::Input_Ability_Movement_Grapple);
		}
	}

	if (!bAppliedInputDefinition && !bAddedInputMapping && BindingHandles.IsEmpty())
	{
		LoadedInputActions.Reset();
		LoadedCharacterActionDefinition = nullptr;
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
			if (ULocalPlayerSettingsSubsystem* LocalPlayerSettings = ULocalPlayerSettingsSubsystem::Get(Controller))
			{
				if (AppliedInputMapping)
				{
					LocalPlayerSettings->RemoveInputMappingContext(AppliedInputMapping);
				}
			}
		}
	}

	BindingHandles.Reset();
	LoadedInputActions.Reset();
	LoadedCharacterActionDefinition = nullptr;
	AppliedInputMapping = nullptr;
	bAppliedInputDefinition = false;
	bAddedInputMapping = false;
	bSelectPandoraActionOpened = false;
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

	UInputAction* LoadedInputAction = InputAction.Get();
	if (LoadedInputAction)
	{
		LoadedInputActions.AddUnique(LoadedInputAction);
	}

	return LoadedInputAction;
}

const UCharacterActionDefinition* UControllerInputComponent::LoadCharacterActionDefinition()
{
	if (LoadedCharacterActionDefinition)
	{
		return LoadedCharacterActionDefinition;
	}

	UControllerInputDefinition* Definition = LoadedInputDefinition.Get();
	if (!Definition)
	{
		Definition = LoadInputDefinition();
	}
	if (!Definition)
	{
		return nullptr;
	}

	const TSoftObjectPtr<UCharacterActionDefinition>& ActionDefinition = Definition->GetCharacterActionDefinition();
	LoadedCharacterActionDefinition = ActionDefinition.IsNull() ? nullptr : ActionDefinition.Get();
	return LoadedCharacterActionDefinition;
}

bool UControllerInputComponent::IsCharacterActionAvailable(APdPlayer* PlayerCharacter, const ECharacterActionType ActionType) const
{
	if (PlayerCharacter && ActionType == ECharacterActionType::PandoraWeaponSwap)
	{
		const UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter->GetAbilitySystemComponent();
		return !AbilitySystemComponent
			|| !AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::Cooldown_EquipWeapon);
	}

	return !PlayerCharacter || !PlayerCharacter->IsCharacterActionOnCooldown(ActionType);
}

void UControllerInputComponent::BindNativeInputActions(UEnhancedInputComponent& EnhancedInputComponent,
	const UControllerInputDefinition& Definition)
{
	using FInputActionHandler = void (UControllerInputComponent::*)(const FInputActionValue&);

	const auto BindAction = [this, &EnhancedInputComponent](
		UInputAction* InputAction,
		const ETriggerEvent TriggerEvent,
		const FInputActionHandler Handler,
		const bool bTriggerWhenPaused)
	{
		if (!InputAction || !Handler)
		{
			return;
		}

		if (bTriggerWhenPaused)
		{
			InputAction->bTriggerWhenPaused = true;
		}

		FEnhancedInputActionEventBinding& Binding =
			EnhancedInputComponent.BindAction(InputAction, TriggerEvent, this, Handler);
		AddInputBindingHandle(Binding.GetHandle());
	};

	const auto BindLoadedAction = [this, &BindAction](
		const TSoftObjectPtr<UInputAction>& InputAction,
		const ETriggerEvent TriggerEvent,
		const FInputActionHandler Handler,
		const bool bTriggerWhenPaused)
	{
		BindAction(LoadInputAction(InputAction), TriggerEvent, Handler, bTriggerWhenPaused);
	};

	const auto BindStartedCompletedCanceledAction = [&BindAction](
		UInputAction* InputAction,
		const FInputActionHandler StartedHandler,
		const FInputActionHandler EndedHandler)
	{
		BindAction(InputAction, ETriggerEvent::Started, StartedHandler, false);
		BindAction(InputAction, ETriggerEvent::Completed, EndedHandler, false);
		BindAction(InputAction, ETriggerEvent::Canceled, EndedHandler, false);
	};

	const auto BindLoadedStartedCompletedCanceledAction = [this, &BindStartedCompletedCanceledAction](
		const TSoftObjectPtr<UInputAction>& InputAction,
		const FInputActionHandler StartedHandler,
		const FInputActionHandler EndedHandler)
	{
		BindStartedCompletedCanceledAction(LoadInputAction(InputAction), StartedHandler, EndedHandler);
	};

	BindLoadedAction(Definition.GetMoveInputAction(), ETriggerEvent::Triggered, &ThisClass::HandleMoveInput, false);
	BindLoadedAction(Definition.GetLookInputAction(), ETriggerEvent::Triggered, &ThisClass::HandleLookInput, false);
	BindLoadedStartedCompletedCanceledAction(Definition.GetJumpInputAction(), &ThisClass::HandleJumpInputStarted, &ThisClass::HandleJumpInputEnded);
	BindLoadedStartedCompletedCanceledAction(Definition.GetCrouchInputAction(), &ThisClass::HandleCrouchInputStarted, &ThisClass::HandleCrouchInputEnded);
	BindLoadedAction(Definition.GetInteractInputAction(), ETriggerEvent::Started, &ThisClass::HandleInteractInput, false);
	BindLoadedStartedCompletedCanceledAction(Definition.GetAttackInputAction(), &ThisClass::HandleAttackInputStarted, &ThisClass::HandleAttackInputEnded);
	BindLoadedStartedCompletedCanceledAction(Definition.GetAimInputAction(), &ThisClass::HandleAimInputStarted, &ThisClass::HandleAimInputEnded);
	BindLoadedAction(Definition.GetPaintInputAction(), ETriggerEvent::Triggered, &ThisClass::HandlePaintInputTriggered, true);
	BindLoadedStartedCompletedCanceledAction(Definition.GetGrappleInputAction(), &ThisClass::HandleGrappleInputStarted, &ThisClass::HandleGrappleInputEnded);
	BindLoadedAction(Definition.GetOpenInfoProfileInputAction(), ETriggerEvent::Started, &ThisClass::HandleOpenInfoProfileInputStarted, true);
	BindLoadedAction(Definition.GetOpenInfoItemInputAction(), ETriggerEvent::Started, &ThisClass::HandleOpenInfoItemInputStarted, true);
	BindLoadedAction(Definition.GetOpenInfoSkinInputAction(), ETriggerEvent::Started, &ThisClass::HandleOpenInfoSkinInputStarted, true);
	BindLoadedAction(Definition.GetOpenInfoPandoraInputAction(), ETriggerEvent::Started, &ThisClass::HandleOpenInfoPandoraInputStarted, true);
	BindLoadedAction(Definition.GetOpenInfoMapInputAction(), ETriggerEvent::Started, &ThisClass::HandleOpenInfoMapInputStarted, true);
	BindLoadedAction(Definition.GetOpenSettingUiInputAction(), ETriggerEvent::Started, &ThisClass::HandleOpenSettingUiInputStarted, true);
	BindLoadedAction(Definition.GetEscapeInputAction(), ETriggerEvent::Started, &ThisClass::HandleEscapeInputStarted, true);
	BindLoadedAction(Definition.GetOpenLobbyInputAction(), ETriggerEvent::Started, &ThisClass::HandleOpenLobbyInputStarted, true);
	BindLoadedStartedCompletedCanceledAction(
		Definition.GetSelectPandoraInputAction(),
		&ThisClass::HandleSelectPandoraInputStarted,
		&ThisClass::HandleSelectPandoraInputEnded);
	BindLoadedAction(Definition.GetPandoraTreeInputAction(), ETriggerEvent::Started, &ThisClass::HandlePandoraTreeInputStarted, true);
	BindLoadedStartedCompletedCanceledAction(Definition.GetSkill1InputAction(), &ThisClass::HandleSkill1InputStarted, &ThisClass::HandleSkill1InputEnded);
	BindLoadedStartedCompletedCanceledAction(Definition.GetSkill2InputAction(), &ThisClass::HandleSkill2InputStarted, &ThisClass::HandleSkill2InputEnded);
	BindLoadedStartedCompletedCanceledAction(Definition.GetSkill3InputAction(), &ThisClass::HandleSkill3InputStarted, &ThisClass::HandleSkill3InputEnded);
	BindLoadedStartedCompletedCanceledAction(Definition.GetSkill4InputAction(), &ThisClass::HandleSkill4InputStarted, &ThisClass::HandleSkill4InputEnded);
	BindLoadedAction(Definition.GetQuickSlot1InputAction(), ETriggerEvent::Started, &ThisClass::HandleQuickSlot1InputStarted, false);
	BindLoadedAction(Definition.GetQuickSlot2InputAction(), ETriggerEvent::Started, &ThisClass::HandleQuickSlot2InputStarted, false);
	BindLoadedAction(Definition.GetQuickSlot3InputAction(), ETriggerEvent::Started, &ThisClass::HandleQuickSlot3InputStarted, false);
	BindLoadedAction(Definition.GetQuickSlot4InputAction(), ETriggerEvent::Started, &ThisClass::HandleQuickSlot4InputStarted, false);
	BindLoadedAction(Definition.GetGesture1InputAction(), ETriggerEvent::Started, &ThisClass::HandleGesture1InputStarted, false);
	BindLoadedAction(Definition.GetGesture2InputAction(), ETriggerEvent::Started, &ThisClass::HandleGesture2InputStarted, false);
	BindLoadedAction(Definition.GetGesture3InputAction(), ETriggerEvent::Started, &ThisClass::HandleGesture3InputStarted, false);
	BindLoadedAction(Definition.GetGesture4InputAction(), ETriggerEvent::Started, &ThisClass::HandleGesture4InputStarted, false);
	BindLoadedAction(Definition.GetTargetConfirmInputAction(), ETriggerEvent::Started, &ThisClass::HandleTargetConfirmInputStarted, false);
}


bool UControllerInputComponent::CancelHitReactForMoveInput(APdPlayer* PlayerCharacter) const
{
	return PlayerCharacter && PlayerCharacter->RequestCancelHitReactForMovement();
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

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

	APdPlayer* PlayerCharacter = Cast<APdPlayer>(ControlledPawn);
	const UEquipmentComponent* EquipmentComponent = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
	UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter ? PlayerCharacter->GetAbilitySystemComponent() : nullptr;
	const bool bCancelledHitReactForMovement = CancelHitReactForMoveInput(PlayerCharacter);
	const FGameplayTag MovementBlockStateTag = LoadedInputDefinition ? LoadedInputDefinition->GetMovementBlockStateTag() : FGameplayTag();
	if (!bCancelledHitReactForMovement
		&& AbilitySystemComponent
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
		if (USkinEquipmentComponent* SkinEquipmentComponent = PlayerCharacter->GetSkinEquipmentComponent())
		{
			SkinEquipmentComponent->RequestCancelActiveGestureMontage();
		}
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

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

	if (const APdPlayer* PlayerCharacter = Cast<APdPlayer>(Controller->GetPawn()); PlayerCharacter && PlayerCharacter->IsStatusFrozen())
	{
		return;
	}

	Controller->AddYawInput(static_cast<float>(LookValue.X));
	Controller->AddPitchInput(static_cast<float>(LookValue.Y));
}

void UControllerInputComponent::HandleJumpInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

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

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

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
		return;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter)
	{
		return;
	}

	AActor* InteractableActor = PlayerCharacter->GetCurrentInteractActor();
	if (!IsValid(InteractableActor))
	{
		return;
	}

	if (PlayerCharacter->InteractWithCurrentTarget())
	{
		return;
	}

	UPlayerRewardComponent* PlayerRewardComponent = GetPlayerRewardComponent();
	if (!PlayerRewardComponent)
	{
		return;
	}

	PlayerRewardComponent->ApplyInteractRewards(InteractableActor);
}

void UControllerInputComponent::HandleOpenInfoProfileInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OpenInfoUiFocused(EPdInfoUiSection::Profile);
	}
}

void UControllerInputComponent::HandleOpenInfoItemInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OpenInfoUiFocused(EPdInfoUiSection::Item);
	}
}

void UControllerInputComponent::HandleOpenInfoSkinInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OpenInfoUiFocused(EPdInfoUiSection::Skin);
	}
}

void UControllerInputComponent::HandleOpenInfoPandoraInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OpenInfoUiFocused(EPdInfoUiSection::Pandora);
	}
}

void UControllerInputComponent::HandleOpenInfoMapInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);
	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OpenInfoUiFocused(EPdInfoUiSection::Map);
	}
}

void UControllerInputComponent::HandleOpenSettingUiInputStarted(const FInputActionValue& InputValue)
{
	const UControllerInputDefinition* Definition = LoadedInputDefinition.Get();
	if (Definition
		&& Definition->GetOpenSettingUiInputAction().ToSoftObjectPath()
			== Definition->GetEscapeInputAction().ToSoftObjectPath())
	{
		// A legacy data asset may point both fields at IA_Escape. Let the Escape
		// handler own that shared action so a single key press is not processed twice.
		return;
	}

	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OnOpenSettingsMenuInputStarted(InputValue);
	}
}

void UControllerInputComponent::HandleEscapeInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	APdPlayerController* Controller = GetPdController();
	if (Controller)
	{
		if (UChatControllerComponent* ChatController =
			Controller->FindComponentByClass<UChatControllerComponent>();
			ChatController && ChatController->IsChatFocused())
		{
			ChatController->ExitChat();
			return;
		}
	}

	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->HandleEscapeInput();
		bSelectPandoraActionOpened = false;
	}
}

void UControllerInputComponent::HandleOpenLobbyInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (!IsOpenLobbyInputAllowed())
	{
		return;
	}

	if (ALobbyHUD* LobbyHUD = Cast<ALobbyHUD>(GetPdHUD()))
	{
		LobbyHUD->CreateLobbyUI();
	}
}

void UControllerInputComponent::HandleSelectPandoraInputStarted(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	if (!IsCharacterActionAvailable(PlayerCharacter, ECharacterActionType::PandoraWeaponSwap))
	{
		return;
	}

	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->OnSelectPandoraInputStarted(InputValue);
		if (HUD->IsSelectPandoraUiOpen())
		{
			bSelectPandoraActionOpened = true;
		}
	}
}

void UControllerInputComponent::HandleSelectPandoraInputEnded(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	if (!bSelectPandoraActionOpened)
	{
		return;
	}

	bSelectPandoraActionOpened = false;
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

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

	if (UCombatComponent* CombatComponent = GetPlayerCombatComponent())
	{
		CombatComponent->StartPrimaryAttack();
		return;
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

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

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

void UControllerInputComponent::HandlePaintInputTriggered(const FInputActionValue& InputValue)
{
	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	if (!PlayerCharacter
		|| !PlayerCharacter->HasActivePaintCanvas()
		|| InputValue.GetMagnitude() <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	PlayerCharacter->TryPaintAtCursor();
}

void UControllerInputComponent::HandleGrappleInputStarted(const FInputActionValue& InputValue)
{
	HandleAbilityInputStarted(InputValue, LabGameplayTags::Input_Ability_Movement_Grapple);
}

void UControllerInputComponent::HandleGrappleInputEnded(const FInputActionValue& InputValue)
{
	HandleAbilityInputEnded(InputValue, LabGameplayTags::Input_Ability_Movement_Grapple);
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

void UControllerInputComponent::HandleQuickSlot1InputStarted(const FInputActionValue& InputValue)
{
	HandleQuickSlotInputStarted(InputValue, 0);
}

void UControllerInputComponent::HandleQuickSlot2InputStarted(const FInputActionValue& InputValue)
{
	HandleQuickSlotInputStarted(InputValue, 1);
}

void UControllerInputComponent::HandleQuickSlot3InputStarted(const FInputActionValue& InputValue)
{
	HandleQuickSlotInputStarted(InputValue, 2);
}

void UControllerInputComponent::HandleQuickSlot4InputStarted(const FInputActionValue& InputValue)
{
	HandleQuickSlotInputStarted(InputValue, 3);
}

void UControllerInputComponent::HandleQuickSlotInputStarted(const FInputActionValue& InputValue, const int32 SlotIndex)
{
	static_cast<void>(InputValue);

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

	UInventoryComponent* InventoryComponent = GetPlayerInventoryComponent();
	if (!InventoryComponent)
	{
		return;
	}

	InventoryComponent->UseConsumableQuickSlot(SlotIndex);
}

void UControllerInputComponent::HandleGesture1InputStarted(const FInputActionValue& InputValue)
{
	HandleGestureInputStarted(InputValue, 0);
}

void UControllerInputComponent::HandleGesture2InputStarted(const FInputActionValue& InputValue)
{
	HandleGestureInputStarted(InputValue, 1);
}

void UControllerInputComponent::HandleGesture3InputStarted(const FInputActionValue& InputValue)
{
	HandleGestureInputStarted(InputValue, 2);
}

void UControllerInputComponent::HandleGesture4InputStarted(const FInputActionValue& InputValue)
{
	HandleGestureInputStarted(InputValue, 3);
}

void UControllerInputComponent::HandleGestureInputStarted(const FInputActionValue& InputValue, const int32 GestureSlotIndex)
{
	static_cast<void>(InputValue);

	if (IsGameplayInputBlockedByUi())
	{
		return;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	USkinEquipmentComponent* SkinEquipmentComponent = PlayerCharacter ? PlayerCharacter->GetSkinEquipmentComponent() : nullptr;
	if (!SkinEquipmentComponent)
	{
		return;
	}

	SkinEquipmentComponent->RequestPlayGestureSlot(GestureSlotIndex);
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

	if (!InputTag.IsValid())
	{
		return;
	}

	const bool bBlockedByUi = IsGameplayInputBlockedByUi();
	if (bBlockedByUi)
	{
		return;
	}

	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	UPdAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter ? PlayerCharacter->GetPdAbilitySystemComponent() : nullptr;
	if (!PlayerCharacter || !AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->AbilityInputTagPressed(InputTag);
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
	if (!PlayerCharacter || !AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->AbilityInputTagReleased(InputTag);
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

UInventoryComponent* UControllerInputComponent::GetPlayerInventoryComponent() const
{
	const APdPlayerController* Controller = GetPdController();
	APdPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APdPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
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

bool UControllerInputComponent::IsOpenLobbyInputAllowed() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UControllerInputDefinition* Definition = LoadedInputDefinition.Get();
	if (!Definition)
	{
		Definition = const_cast<UControllerInputComponent*>(this)->LoadInputDefinition();
	}
	if (!Definition)
	{
		return false;
	}

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	return Definition->IsOpenLobbyInputAllowedForMap(CurrentLevelName);
}
