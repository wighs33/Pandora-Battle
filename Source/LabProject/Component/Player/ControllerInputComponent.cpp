#include "Component/Player/ControllerInputComponent.h"

#include "AbilitySystemComponent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "EnhancedInputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Component/Item/InventoryComponent.h"
#include "Mode/PdPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Component/Player/CombatComponent.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/PlayerActionComponent.h"
#include "Component/Player/PlayerInteractionComponent.h"
#include "Component/Player/PlayerRewardComponent.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Interface/HudInputInterface.h"
#include "Engine/LocalPlayer.h"

#include <type_traits>

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

	if (!GetLoadedInputDefinition() || !InputContentLease.IsValid())
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
	RefreshInputDefinition();
}

UControllerInputDefinition* UControllerInputComponent::GetLoadedInputDefinition()
{
	if (!LoadedInputDefinition && !ActiveInputDefinition.IsNull())
	{
		LoadedInputDefinition = ActiveInputDefinition.Get();
	}
	return LoadedInputDefinition;
}

void UControllerInputComponent::BeginInputDefinitionPreload()
{
	if (bInputPreloadPending || ActiveInputDefinition.IsNull())
	{
		return;
	}

	ReleaseInputDefinitionPreload();
	// OnRegister는 GameInstance가 없는 에디터 월드에서도 불리므로 그때는 조용히 건너뛴다.
	UContentDataSubsystem* ContentSubsystem = FindContentDataSubsystem();
	if (!ContentSubsystem)
	{
		return;
	}

	bInputPreloadPending = true;
	if (GetLoadedInputDefinition())
	{
		HandleInputDefinitionPreloadComplete();
		return;
	}

	InputDefinitionLease = ContentSubsystem->AcquireContent(
		TArray<FSoftObjectPath>{ActiveInputDefinition.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleInputDefinitionPreloadComplete));
}

void UControllerInputComponent::HandleInputDefinitionPreloadComplete()
{
	LoadedInputDefinition = ActiveInputDefinition.Get();
	if (!LoadedInputDefinition)
	{
		bInputPreloadPending = false;
		return;
	}

	UContentDataSubsystem* ContentSubsystem = FindContentDataSubsystem();
	if (!ContentSubsystem)
	{
		InputDefinitionLease.Reset();
		HandleInputContentPreloadComplete();
		return;
	}

	TArray<FSoftObjectPath> RuntimeAssetPaths;
	RuntimeAssetPaths.Add(ActiveInputDefinition.ToSoftObjectPath());
	LoadedInputDefinition->GetRuntimePreloadAssetPaths(RuntimeAssetPaths);
	InputContentLease = ContentSubsystem->AcquireContent(RuntimeAssetPaths,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleInputContentPreloadComplete));
	InputDefinitionLease.Reset();
}

void UControllerInputComponent::HandleInputContentPreloadComplete()
{
	bInputPreloadPending = false;
	ApplyInputDefinition();
}

void UControllerInputComponent::ReleaseInputDefinitionPreload()
{
	bInputPreloadPending = false;
	InputDefinitionLease.Reset();
	InputContentLease.Reset();
}

UContentDataSubsystem* UControllerInputComponent::FindContentDataSubsystem() const
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
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

	UControllerInputDefinition* LoadedDefinition = GetLoadedInputDefinition();
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
		}
	}

	BindNativeInputActions(*EnhancedInputComponent, *LoadedDefinition);

	bAppliedInputDefinition = AppliedInputMapping || !BindingHandles.IsEmpty();
	if (bAppliedInputDefinition)
	{
		InputDefinitionApplied.Broadcast();
	}

	return bAppliedInputDefinition;
}

void UControllerInputComponent::RemoveAppliedInputDefinition()
{
	ReleaseInputDefinitionPreload();
	if (APdPlayer* PlayerCharacter = GetPlayerCharacter())
	{
		if (UPdAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter->GetPdAbilitySystemComponent())
		{
			AbilitySystemComponent->HandleAbilityInputReleased(LabGameplayTags::Input_Ability_Movement_Grapple);
		}
	}

	if (!bAppliedInputDefinition && !AppliedInputMapping && BindingHandles.IsEmpty())
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

		if (Controller->IsLocalController() && AppliedInputMapping)
		{
			if (ULocalPlayerSettingsSubsystem* LocalPlayerSettings = ULocalPlayerSettingsSubsystem::Get(Controller))
			{
				LocalPlayerSettings->RemoveInputMappingContext(AppliedInputMapping);
			}
		}
	}

	BindingHandles.Reset();
	LoadedInputActions.Reset();
	AppliedInputMapping = nullptr;
	bAppliedInputDefinition = false;
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

bool UControllerInputComponent::CanSwapPandoraAndWeapon(APdPlayer* PlayerCharacter) const
{
	if (!PlayerCharacter)
	{
		return true;
	}

	const UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter->GetAbilitySystemComponent();
	return !AbilitySystemComponent
		|| !AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::Cooldown_EquipWeapon);
}

void UControllerInputComponent::BindNativeInputActions(UEnhancedInputComponent& EnhancedInputComponent,
	const UControllerInputDefinition& Definition)
{
	// 창을 여닫는 입력은 일시 정지 중에도 받아야 하므로 액션 자체에 표시한다.
	const auto Action = [this](const TSoftObjectPtr<UInputAction>& InputAction)
	{
		return LoadInputAction(InputAction);
	};
	const auto ActionWhilePaused = [this](const TSoftObjectPtr<UInputAction>& InputAction)
	{
		UInputAction* LoadedInputAction = LoadInputAction(InputAction);
		if (LoadedInputAction)
		{
			LoadedInputAction->bTriggerWhenPaused = true;
		}
		return LoadedInputAction;
	};

	// 슬롯 번호·능력 태그·정보 탭처럼 같은 처리기를 여러 입력이 나눠 쓰면 그 값을 바인딩에 함께 싣는다.
	const auto Bind = [this, &EnhancedInputComponent]<typename... TPayload>(UInputAction* InputAction,
		const ETriggerEvent TriggerEvent,
		void (UControllerInputComponent::*Handler)(const FInputActionValue&, TPayload...),
		const std::type_identity_t<TPayload>... Payload)
	{
		if (InputAction)
		{
			AddInputBindingHandle(
				EnhancedInputComponent.BindAction(InputAction, TriggerEvent, this, Handler, Payload...).GetHandle());
		}
	};

	// 누르는 동안 유지되는 입력은 떼거나 취소될 때 같은 끝 처리를 받는다.
	const auto BindPressAndRelease = [&Bind]<typename... TPayload>(UInputAction* InputAction,
		void (UControllerInputComponent::*PressedHandler)(const FInputActionValue&, TPayload...),
		void (UControllerInputComponent::*ReleasedHandler)(const FInputActionValue&, TPayload...),
		const std::type_identity_t<TPayload>... Payload)
	{
		Bind(InputAction, ETriggerEvent::Started, PressedHandler, Payload...);
		Bind(InputAction, ETriggerEvent::Completed, ReleasedHandler, Payload...);
		Bind(InputAction, ETriggerEvent::Canceled, ReleasedHandler, Payload...);
	};

	Bind(Action(Definition.GetMoveInputAction()), ETriggerEvent::Triggered, &ThisClass::HandleMoveInput);
	Bind(Action(Definition.GetLookInputAction()), ETriggerEvent::Triggered, &ThisClass::HandleLookInput);
	BindPressAndRelease(Action(Definition.GetJumpInputAction()), &ThisClass::HandleJumpInputStarted, &ThisClass::HandleJumpInputEnded);
	BindPressAndRelease(Action(Definition.GetCrouchInputAction()), &ThisClass::HandleCrouchInputStarted, &ThisClass::HandleCrouchInputEnded);
	Bind(Action(Definition.GetInteractInputAction()), ETriggerEvent::Started, &ThisClass::HandleInteractInput);
	BindPressAndRelease(Action(Definition.GetAttackInputAction()), &ThisClass::HandleAttackInputStarted, &ThisClass::HandleAttackInputEnded);
	BindPressAndRelease(Action(Definition.GetAimInputAction()), &ThisClass::HandleAimInputStarted, &ThisClass::HandleAimInputEnded);
	BindPressAndRelease(Action(Definition.GetGrappleInputAction()),
		&ThisClass::HandleAbilityInputStarted, &ThisClass::HandleAbilityInputEnded, LabGameplayTags::Input_Ability_Movement_Grapple);

	Bind(ActionWhilePaused(Definition.GetOpenInfoProfileInputAction()), ETriggerEvent::Started, &ThisClass::HandleOpenInfoInputStarted, EInfoUiSection::Profile);
	Bind(ActionWhilePaused(Definition.GetOpenInfoItemInputAction()), ETriggerEvent::Started, &ThisClass::HandleOpenInfoInputStarted, EInfoUiSection::Item);
	Bind(ActionWhilePaused(Definition.GetOpenInfoSkinInputAction()), ETriggerEvent::Started, &ThisClass::HandleOpenInfoInputStarted, EInfoUiSection::Skin);
	Bind(ActionWhilePaused(Definition.GetOpenInfoPandoraInputAction()), ETriggerEvent::Started, &ThisClass::HandleOpenInfoInputStarted, EInfoUiSection::Pandora);
	Bind(ActionWhilePaused(Definition.GetOpenInfoMapInputAction()), ETriggerEvent::Started, &ThisClass::HandleOpenInfoInputStarted, EInfoUiSection::Map);
	Bind(ActionWhilePaused(Definition.GetOpenSettingUiInputAction()), ETriggerEvent::Started, &ThisClass::HandleOpenSettingUiInputStarted);
	Bind(ActionWhilePaused(Definition.GetEscapeInputAction()), ETriggerEvent::Started, &ThisClass::HandleEscapeInputStarted);
	Bind(ActionWhilePaused(Definition.GetOpenLobbyInputAction()), ETriggerEvent::Started, &ThisClass::HandleOpenLobbyInputStarted);
	BindPressAndRelease(Action(Definition.GetSelectPandoraInputAction()),
		&ThisClass::HandleSelectPandoraInputStarted, &ThisClass::HandleSelectPandoraInputEnded);
	Bind(ActionWhilePaused(Definition.GetPandoraTreeInputAction()), ETriggerEvent::Started, &ThisClass::HandlePandoraTreeInputStarted);
	BindPressAndRelease(Action(Definition.GetScoreboardInputAction()),
		&ThisClass::HandleScoreboardInputStarted, &ThisClass::HandleScoreboardInputEnded);
	Bind(Action(Definition.GetChatInputAction()), ETriggerEvent::Started, &ThisClass::HandleChatInputStarted);
	Bind(Action(Definition.GetChatScrollInputAction()), ETriggerEvent::Triggered, &ThisClass::HandleChatScrollInputTriggered);

	BindPressAndRelease(Action(Definition.GetSkill1InputAction()),
		&ThisClass::HandleAbilityInputStarted, &ThisClass::HandleAbilityInputEnded, LabGameplayTags::Input_Ability_Skill1);
	BindPressAndRelease(Action(Definition.GetSkill2InputAction()),
		&ThisClass::HandleAbilityInputStarted, &ThisClass::HandleAbilityInputEnded, LabGameplayTags::Input_Ability_Skill2);
	BindPressAndRelease(Action(Definition.GetSkill3InputAction()),
		&ThisClass::HandleAbilityInputStarted, &ThisClass::HandleAbilityInputEnded, LabGameplayTags::Input_Ability_Skill3);
	BindPressAndRelease(Action(Definition.GetSkill4InputAction()),
		&ThisClass::HandleAbilityInputStarted, &ThisClass::HandleAbilityInputEnded, LabGameplayTags::Input_Ability_Skill4);
	Bind(Action(Definition.GetQuickSlot1InputAction()), ETriggerEvent::Started, &ThisClass::HandleQuickSlotInputStarted, 0);
	Bind(Action(Definition.GetQuickSlot2InputAction()), ETriggerEvent::Started, &ThisClass::HandleQuickSlotInputStarted, 1);
	Bind(Action(Definition.GetQuickSlot3InputAction()), ETriggerEvent::Started, &ThisClass::HandleQuickSlotInputStarted, 2);
	Bind(Action(Definition.GetQuickSlot4InputAction()), ETriggerEvent::Started, &ThisClass::HandleQuickSlotInputStarted, 3);
	Bind(Action(Definition.GetGesture1InputAction()), ETriggerEvent::Started, &ThisClass::HandleGestureInputStarted, 0);
	Bind(Action(Definition.GetGesture2InputAction()), ETriggerEvent::Started, &ThisClass::HandleGestureInputStarted, 1);
	Bind(Action(Definition.GetGesture3InputAction()), ETriggerEvent::Started, &ThisClass::HandleGestureInputStarted, 2);
	Bind(Action(Definition.GetGesture4InputAction()), ETriggerEvent::Started, &ThisClass::HandleGestureInputStarted, 3);
	Bind(Action(Definition.GetTargetConfirmInputAction()), ETriggerEvent::Started, &ThisClass::HandleTargetConfirmInputStarted);
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

	if (Controller->IsMoveInputIgnored())
	{
		return;
	}

	APdPlayer* PlayerCharacter = Cast<APdPlayer>(ControlledPawn);
	const UEquipmentComponent* EquipmentComponent = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
	UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter ? PlayerCharacter->GetAbilitySystemComponent() : nullptr;
	const bool bCancelledHitReactForMovement = PlayerCharacter && PlayerCharacter->GetPlayerActionComponent()->RequestCancelHitReactForMovement(0.08f);
	const FGameplayTag MovementBlockStateTag = LoadedInputDefinition ? LoadedInputDefinition->GetMovementBlockStateTag() : FGameplayTag();
	if (!bCancelledHitReactForMovement && AbilitySystemComponent && MovementBlockStateTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(MovementBlockStateTag) && EquipmentComponent
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

	if (Controller->IsLookInputIgnored())
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

	UPlayerInteractionComponent* Interaction = PlayerCharacter->GetPlayerInteractionComponent();
	AActor* InteractableActor = Interaction->GetCurrentInteractActor();
	if (!IsValid(InteractableActor))
	{
		return;
	}

	if (Interaction->InteractWithCurrentTarget())
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

void UControllerInputComponent::HandleAbilityInputStarted(const FInputActionValue& InputValue, const FGameplayTag InputTag)
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

	AbilitySystemComponent->HandleAbilityInputPressed(InputTag);
}

void UControllerInputComponent::HandleAbilityInputEnded(const FInputActionValue& InputValue, const FGameplayTag InputTag)
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

	AbilitySystemComponent->HandleAbilityInputReleased(InputTag);
}

APdPlayerController* UControllerInputComponent::GetPdController() const
{
	return Cast<APdPlayerController>(GetOwner());
}

IHudInputInterface* UControllerInputComponent::GetHudInput() const
{
	const APdPlayerController* Controller = GetPdController();
	return Controller ? Cast<IHudInputInterface>(Controller->GetHUD()) : nullptr;
}

APdPlayer* UControllerInputComponent::GetPlayerCharacter() const
{
	const APdPlayerController* Controller = GetPdController();
	return Controller ? Cast<APdPlayer>(Controller->GetPawn()) : nullptr;
}

UPlayerRewardComponent* UControllerInputComponent::GetPlayerRewardComponent() const
{
	const APdPlayerController* Controller = GetPdController();
	APlayerState* PlayerState = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
	return PlayerState ? PlayerState->FindComponentByClass<UPlayerRewardComponent>() : nullptr;
}

UInventoryComponent* UControllerInputComponent::GetPlayerInventoryComponent() const
{
	const APdPlayerController* Controller = GetPdController();
	APlayerState* PlayerState = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
	return PlayerState ? PlayerState->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UCombatComponent* UControllerInputComponent::GetPlayerCombatComponent() const
{
	APdPlayer* PlayerCharacter = GetPlayerCharacter();
	return PlayerCharacter ? PlayerCharacter->GetCombatComponent() : nullptr;
}

bool UControllerInputComponent::IsGameplayInputBlockedByUi() const
{
    const IHudInputInterface* HUD = GetHudInput();
    return HUD && HUD->IsGameplayInputBlockedByUi();
}

void UControllerInputComponent::ReleaseGameplayInput()
{
    if (UCombatComponent* Combat = GetPlayerCombatComponent())
    {
        Combat->StopPrimaryAttack();
        Combat->StopAim();
    }
    if (APdPlayer* Character = GetPlayerCharacter())
    {
        Character->StopJumping();
        Character->UnCrouch();
        Character->ConsumeMovementInputVector();
        if (UPdAbilitySystemComponent* AbilitySystem = Character->GetPdAbilitySystemComponent())
        {
            const FGameplayTag InputTags[] = {LabGameplayTags::Input_Ability_Movement_Grapple,
                LabGameplayTags::Input_Ability_Skill1, LabGameplayTags::Input_Ability_Skill2,
                LabGameplayTags::Input_Ability_Skill3, LabGameplayTags::Input_Ability_Skill4};
            for (const FGameplayTag Tag : InputTags)
            {
                AbilitySystem->HandleAbilityInputReleased(Tag);
            }
        }
    }
}
