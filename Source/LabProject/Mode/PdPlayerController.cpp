// Fill out your copyright notice in the Description page of Project Settings.


#include "PdPlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Ability/AttackAbility.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/GameFrameworkComponentManager.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "Interface/InteractableInterface.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Item/ItemInstance.h"
#include "Character/PdPlayer.h"
#include "Mode/PdPlayerState.h"
#include "UI/UiSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayerController)

DEFINE_LOG_CATEGORY(PdPlayerControllerLog);
namespace
{
	constexpr float OffenseStatUpMagnitude = 1.3f;
	constexpr float DefenseStatUpMagnitude = 1.2f;
	constexpr float ResistanceStatUpMagnitude = 1.0f;
	constexpr float PandoraStatUpMagnitude = 1.0f;
	constexpr float ResourceStatUpMagnitude = 1.4f;
	constexpr float AgilityStatUpMagnitude = 2.0f;

	UAttackAbility* ResolveActiveAttackAbility(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags)
	{
		if (!AbilitySystemComponent || AbilityTags.IsEmpty())
		{
			return nullptr;
		}

		for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (!AbilitySpec.IsActive() || !AbilitySpec.Ability)
			{
				continue;
			}

			if (!AbilitySpec.Ability->GetAssetTags().HasAny(AbilityTags))
			{
				continue;
			}

			return Cast<UAttackAbility>(AbilitySpec.GetPrimaryInstance());
		}

		return nullptr;
	}

	bool ResolveStatUpButtonSettings(const FGameplayTag& StatTag, float& OutMagnitude, EEnum_Operation& OutOperation)
	{
		static const FGameplayTag OffenseRootTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Offense"));
		static const FGameplayTag DefenseRootTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Defense"));
		static const FGameplayTag ResistanceRootTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Resistance"));
		static const FGameplayTag PandoraRootTag = FGameplayTag::RequestGameplayTag(TEXT("Status.PandoraForce"));
		static const FGameplayTag ResourceRootTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Resource"));
		static const FGameplayTag AgilityRootTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Agility"));

		if (StatTag.MatchesTag(OffenseRootTag))
		{
			OutMagnitude = OffenseStatUpMagnitude;
			OutOperation = EEnum_Operation::Multiply;
			return true;
		}

		if (StatTag.MatchesTag(DefenseRootTag))
		{
			OutMagnitude = DefenseStatUpMagnitude;
			OutOperation = EEnum_Operation::Multiply;
			return true;
		}

		if (StatTag.MatchesTag(ResistanceRootTag))
		{
			OutMagnitude = ResistanceStatUpMagnitude;
			OutOperation = EEnum_Operation::Add;
			return true;
		}

		if (StatTag.MatchesTag(PandoraRootTag))
		{
			OutMagnitude = PandoraStatUpMagnitude;
			OutOperation = EEnum_Operation::Add;
			return true;
		}

		if (StatTag.MatchesTag(ResourceRootTag))
		{
			OutMagnitude = ResourceStatUpMagnitude;
			OutOperation = EEnum_Operation::Multiply;
			return true;
		}

		if (StatTag.MatchesTag(AgilityRootTag))
		{
			OutMagnitude = AgilityStatUpMagnitude;
			OutOperation = EEnum_Operation::Add;
			return true;
		}

		return false;
	}
}


// 생성자에서 기본 입력 매핑과 입력 액션 에셋을 로드합니다.
APdPlayerController::APdPlayerController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

// GameFrameworkComponentManager가 이 컨트롤러를 인식할 수 있도록 수신자로 등록합니다.
void APdPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// =================================================================================================================
// ### BeginPlay 시점에 GameActorReady 이벤트를 보내고 입력/초기화 상태를 평가합니다.
void APdPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
	AddInputMapping();
	EvaluateInitializationState();
}

// =================================================================================================================
// ### 컨트롤러 종료 시 GameFrameworkComponentManager 수신자 등록을 해제합니다.
void APdPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

// =================================================================================================================
// ### 로컬 플레이어 수신이 끝난 뒤 입력/초기화 상태를 다시 평가합니다.
void APdPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	AddInputMapping();
	EvaluateInitializationState();
}

// =================================================================================================================
// ### Pawn이 바뀔 때 Pawn 의존 초기화 상태를 다시 평가합니다.
void APdPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	AddInputMapping();
	EvaluateInitializationState();
}

// =================================================================================================================
// ### PlayerState 복제 완료 후 PlayerState 의존 초기화 상태를 다시 평가합니다.
void APdPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	AddInputMapping();
	EvaluateInitializationState();
}

// =================================================================================================================
// ### Enhanced Input 액션을 실제 입력 처리 함수에 바인딩합니다.
void APdPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// =================================================================================================================
	// ### EnhancedInputComponent가 아니면 입력 바인딩을 진행할 수 없습니다.
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	// =================================================================================================================
	// ### 에디터나 생성자에서 설정된 InputAction만 바인딩합니다.
	if (MoveInputAction)
	{
		EnhancedInputComponent->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &APdPlayerController::HandleMoveInput);
	}

	if (LookInputAction)
	{
		EnhancedInputComponent->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &APdPlayerController::HandleLookInput);
	}

	if (InteractInputAction)
	{
		EnhancedInputComponent->BindAction(InteractInputAction, ETriggerEvent::Started, this, &APdPlayerController::HandleInteractInput);
	}

	if (AttackInputAction)
	{
		EnhancedInputComponent->BindAction(AttackInputAction, ETriggerEvent::Started, this, &APdPlayerController::HandleAttackInput);
	}

	AddInputMapping();
	EvaluateInitializationState();
}

// =================================================================================================================
// ### 로컬 플레이어의 Enhanced Input Subsystem에 기본 MappingContext를 한 번만 등록합니다.
void APdPlayerController::AddInputMapping()
{
	// =================================================================================================================
	// ### 로컬 컨트롤러가 아니거나 이미 등록된 경우 중복 등록을 막습니다.
	if (!IsLocalController() || bHasAddedDefaultInputMapping || !DefaultInputMapping)
	{
		return;
	}

	// =================================================================================================================
	// ### LocalPlayer에서 EnhancedInputLocalPlayerSubsystem을 찾아 MappingContext를 추가합니다.
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!InputSubsystem)
	{
		return;
	}

	InputSubsystem->AddMappingContext(DefaultInputMapping, 0);
	bHasAddedDefaultInputMapping = true;
}

// =================================================================================================================
// ### 장비 선택 UI의 방향 입력을 장착/해제 Ability 실행 요청으로 변환합니다.
void APdPlayerController::OnSelectedPandoraAndWeapon(EEnum_Direction Direction)
{
	// =================================================================================================================
	// ### 장비 요청에 필요한 Pawn, EquipmentComponent, AbilitySystemComponent를 확보합니다.
	APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	UEquipmentComponent* EquipmentComponent = PlayerCharacter->GetEquipmentComponent();
	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter);
	if (!EquipmentComponent || !AbilitySystemComponent)
	{
		return;
	}

	// =================================================================================================================
	// ### 아래 방향 입력은 현재 장비 해제 Ability를 실행합니다.
	if (Direction == EEnum_Direction::Down)
	{
		if (UnequipAbilityTag.IsValid())
		{
			FGameplayTagContainer UnequipTagContainer;
			UnequipTagContainer.AddTag(UnequipAbilityTag);
			AbilitySystemComponent->TryActivateAbilitiesByTag(UnequipTagContainer, true);
		}
		return;
	}

	// =================================================================================================================
	// ### 방향 입력을 UI에 캐시된 무기 슬롯으로 변환합니다.
	UItemInstance* CurrentWeapon = nullptr;
	switch (Direction)
	{
	case EEnum_Direction::Up:
		CurrentWeapon = CachedSecondWeapon;
		break;
	case EEnum_Direction::Right:
		CurrentWeapon = CachedThirdWeapon;
		break;
	case EEnum_Direction::Left:
		CurrentWeapon = CachedFirstWeapon;
		break;
	case EEnum_Direction::Center:
	default:
		return;
	}

	if (!IsValid(CurrentWeapon))
	{
		return;
	}

	// =================================================================================================================
	// ### 선택된 무기를 RequestedItem으로 지정한 뒤 장착 Ability를 실행합니다.
	EquipmentComponent->SetRequestedItemInstance(CurrentWeapon);

	if (EquipAbilityTag.IsValid())
	{
		FGameplayTagContainer EquipTagContainer;
		EquipTagContainer.AddTag(EquipAbilityTag);
		AbilitySystemComponent->TryActivateAbilitiesByTag(EquipTagContainer, true);
	}
}

// =================================================================================================================
// ### 프로젝트 전용 PlayerState 타입을 반환합니다.
APdPlayerState* APdPlayerController::GetPdPlayerState() const
{
	return GetPlayerState<APdPlayerState>();
}

// =================================================================================================================
// ### 로컬 입력 처리가 가능한 상태인지 확인합니다.
bool APdPlayerController::IsInputReady() const
{
	return IsLocalController()
		&& bHasAddedDefaultInputMapping
		&& GetLocalPlayer() != nullptr
		&& Cast<UEnhancedInputComponent>(InputComponent) != nullptr;
}

// =================================================================================================================
// ### Pawn, PlayerState, Input이 모두 준비됐는지 확인합니다.
bool APdPlayerController::IsInitializationReady() const
{
	return IsInputReady()
		&& GetPawn() != nullptr
		&& GetPdPlayerState() != nullptr;
}

// =================================================================================================================
// ### 컨트롤러 초기화에 필요한 Pawn, PlayerState, Input 상태를 평가하고 Blueprint 이벤트를 호출합니다.
void APdPlayerController::EvaluateInitializationState()
{
	// =================================================================================================================
	// ### 로컬 플레이어가 아니면 UI 캐시와 초기화 브로드캐스트 상태를 초기화합니다.
	if (!IsLocalController() || !GetLocalPlayer())
	{
		StatusViewModel = nullptr;
		bHasBroadcastInputReady = false;
		bHasBroadcastInitializationReady = false;
		return;
	}

	// =================================================================================================================
	// ### UI ViewModel 바인딩 상태를 최신 상태로 갱신합니다.
	RefreshUiBindings();

	// =================================================================================================================
	// ### Pawn 변경을 감지해 Pawn 준비 이벤트를 한 번 호출합니다.
	APawn* CurrentPawn = GetPawn();
	if (LastReadyPawn.Get() != CurrentPawn)
	{
		LastReadyPawn = CurrentPawn;
		bHasBroadcastInitializationReady = false;

		if (CurrentPawn)
		{
			ReceivePawnReady(CurrentPawn);
		}
	}

	// =================================================================================================================
	// ### PlayerState 변경을 감지해 PdPlayerState 준비 이벤트를 한 번 호출합니다.
	APdPlayerState* CurrentPdPlayerState = GetPdPlayerState();
	if (LastReadyPlayerState.Get() != CurrentPdPlayerState)
	{
		LastReadyPlayerState = CurrentPdPlayerState;
		bHasBroadcastInitializationReady = false;

		if (CurrentPdPlayerState)
		{
			ReceivePdPlayerStateReady(CurrentPdPlayerState);
		}
	}

	// =================================================================================================================
	// ### 입력 준비 상태가 처음 완성되는 시점에 Input 준비 이벤트를 호출합니다.
	if (IsInputReady())
	{
		if (!bHasBroadcastInputReady)
		{
			bHasBroadcastInputReady = true;
			bHasBroadcastInitializationReady = false;
			ReceiveInputReady();
		}
	}
	else
	{
		bHasBroadcastInputReady = false;
		bHasBroadcastInitializationReady = false;
	}

	// =================================================================================================================
	// ### Pawn, PlayerState, Input이 모두 준비되면 컨트롤러 최종 초기화 이벤트를 호출합니다.
	if (IsInitializationReady() && !bHasBroadcastInitializationReady)
	{
		bHasBroadcastInitializationReady = true;
		ReceiveControllerInitializationReady();
	}
}

// =================================================================================================================
// ### 위젯에 PdUiSubsystem이 소유한 StatusViewModel을 적용합니다.
void APdPlayerController::ApplyStatusViewModelToWidget(UUserWidget* InWidget)
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->ApplyStatusViewModelToWidget(InWidget);
		StatusViewModel = UiSubsystem->GetStatusViewModel();
	}
}

// =================================================================================================================
// ### UI에서 사용할 StatusViewModel을 반환합니다.
UStatusViewModel* APdPlayerController::GetStatusViewModel() const
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		return UiSubsystem->GetStatusViewModel();
	}

	return StatusViewModel;
}

// =================================================================================================================
// ### 스탯 변경 GameplayEffect 적용 요청을 권한에 맞게 서버 또는 내부 처리로 분기합니다.
bool APdPlayerController::ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EEnum_Operation Operation, float Level)
{
	if (!GameplayEffectClass || !StatTag.IsValid())
	{
		return false;
	}

	if (!HasAuthority())
	{
		ServerApplyStatUpEffectByTag(GameplayEffectClass, StatTag, Magnitude, Operation, Level);
		return true;
	}

	return ApplyStatUpEffectByTagInternal(GameplayEffectClass, StatTag, Magnitude, Operation, Level);
}

// =================================================================================================================
// ### 스탯 업 버튼 클릭 시 태그 카테고리에 맞는 기본 수치로 StatUp GameplayEffect를 적용합니다.
void APdPlayerController::OnClicked_StatUpButton(FGameplayTag StatTag)
{
	if (!StatTag.IsValid())
	{
		return;
	}

	if (!StatUpGameplayEffectClass)
	{
		UE_LOG(PdPlayerControllerLog, Warning, TEXT("OnClicked_StatUpButton failed: '%s' has no StatUp gameplay effect class."), *GetNameSafe(this));
		return;
	}

	float Magnitude = 0.f;
	EEnum_Operation Operation = EEnum_Operation::Add;
	if (!ResolveStatUpButtonSettings(StatTag, Magnitude, Operation))
	{
		UE_LOG(PdPlayerControllerLog, Warning, TEXT("OnClicked_StatUpButton skipped unsupported stat tag '%s'."), *StatTag.ToString());
		return;
	}

	if (!ApplyStatUpEffectByTag(StatUpGameplayEffectClass, StatTag, Magnitude, Operation, 1.f))
	{
		UE_LOG(PdPlayerControllerLog, Warning, TEXT("OnClicked_StatUpButton failed to apply StatUp effect for tag '%s'."), *StatTag.ToString());
	}
}

// =================================================================================================================
// ### 클라이언트에서 요청한 스탯 변경 GameplayEffect를 서버 권한으로 적용합니다.
void APdPlayerController::ServerApplyStatUpEffectByTag_Implementation(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EEnum_Operation Operation, float Level)
{
	ApplyStatUpEffectByTagInternal(GameplayEffectClass, StatTag, Magnitude, Operation, Level);
}

// =================================================================================================================
// ### 클라이언트에서 요청한 상호작용 보상 처리를 서버 권한으로 실행합니다.
void APdPlayerController::ServerHandleInteract_Implementation(AActor* InteractableActor)
{
	APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetPawn());
	if (!PlayerCharacter || !PlayerCharacter->CanInteractWithActor(InteractableActor))
	{
		return;
	}

	if (APdPlayerState* PdPlayerState = GetPdPlayerState())
	{
		PdPlayerState->ApplyInteractRewards(InteractableActor);
	}
}

void APdPlayerController::ServerRequestAttackJumpSection_Implementation(FName RequestedSectionName)
{
	if (RequestedSectionName.IsNone())
	{
		return;
	}

	APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter);
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!AttackAbilityTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer AttackTagContainer;
	AttackTagContainer.AddTag(AttackAbilityTag);

	if (UAttackAbility* ActiveAttackAbility = ResolveActiveAttackAbility(AbilitySystemComponent, AttackTagContainer))
	{
		ActiveAttackAbility->RequestJumpToSection(RequestedSectionName);
	}
}

// =================================================================================================================
// ### PdPlayerState의 AbilitySystemComponent를 통해 스탯 변경 GameplayEffect를 실제로 적용합니다.
bool APdPlayerController::ApplyStatUpEffectByTagInternal(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EEnum_Operation Operation, float Level)
{
	APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>();
	if (!PdPlayerState || !GameplayEffectClass || !StatTag.IsValid())
	{
		return false;
	}

	UPdAbilitySystemComponent* ASC = PdPlayerState->GetPdAbilitySystemComponent();
	if (!ASC)
	{
		return false;
	}

	return ASC->ApplyStatUpEffectByTag(GameplayEffectClass, StatTag, Magnitude, Operation, Level);
}

// 이동 입력을 컨트롤 회전 기준의 전후좌우 이동 입력으로 변환합니다.
void APdPlayerController::HandleMoveInput(const FInputActionValue& InputValue)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const FVector2D MoveValue = InputValue.Get<FVector2D>();
	if (MoveValue.IsNearlyZero())
	{
		return;
	}

	// =================================================================================================================
	// ### 공격 중에 이동하면 공격 어빌리티 취소
	if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ControlledPawn))
	{
		if (AttackAbilityTag.IsValid())
		{
			FGameplayTagContainer AttackTagContainer;
			AttackTagContainer.AddTag(AttackAbilityTag);
			AbilitySystemComponent->CancelAbilities(&AttackTagContainer);
		}
	}

	const FRotator CurrentControlRotation = GetControlRotation();
	const FRotator YawRotation(0.f, CurrentControlRotation.Yaw, 0.f);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

	ControlledPawn->AddMovementInput(RightDirection, static_cast<float>(MoveValue.X));
	ControlledPawn->AddMovementInput(ForwardDirection, static_cast<float>(MoveValue.Y));
}

// ### 시점 입력을 컨트롤러 Yaw/Pitch 입력으로 변환합니다.
void APdPlayerController::HandleLookInput(const FInputActionValue& InputValue)
{
	const FVector2D LookValue = InputValue.Get<FVector2D>();
	if (LookValue.IsNearlyZero())
	{
		return;
	}

	AddYawInput(static_cast<float>(LookValue.X));
	AddPitchInput(static_cast<float>(LookValue.Y));
}

// =================================================================================================================
// ### 상호작용 입력 시 현재 감지된 상호작용 대상을 서버 처리로 전달합니다.
void APdPlayerController::HandleInteractInput(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	// =================================================================================================================
	// ### Pawn에서 현재 상호작용 가능한 대상 목록을 가져옵니다.
	APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	TArray<TScriptInterface<IInteractableInterface>> CurrentInteractActors;
	if (!PlayerCharacter->HasCurrentInteractActors(CurrentInteractActors) || CurrentInteractActors.IsEmpty())
	{
		return;
	}

	// =================================================================================================================
	// ### 첫 번째 상호작용 대상을 Actor로 변환하고 유효성을 확인합니다.
	AActor* InteractableActor = Cast<AActor>(CurrentInteractActors[0].GetObject());
	if (!IsValid(InteractableActor))
	{
		return;
	}

	// =================================================================================================================
	// ### 서버 권한이면 즉시 처리하고, 클라이언트면 서버 RPC로 요청합니다.
	if (HasAuthority())
	{
		if (!PlayerCharacter->CanInteractWithActor(InteractableActor))
		{
			return;
		}

		if (APdPlayerState* PdPlayerState = GetPdPlayerState())
		{
			PdPlayerState->ApplyInteractRewards(InteractableActor);
		}
		return;
	}

	ServerHandleInteract(InteractableActor);
}

// =================================================================================================================
// ### 공격 입력 시 현재 Pawn의 공격 Ability를 태그 기반으로 활성화합니다.
void APdPlayerController::HandleAttackInput(const FInputActionValue& InputValue)
{
	static_cast<void>(InputValue);

	APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetPawn());
	if (!PlayerCharacter)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter);
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!AttackAbilityTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer AttackTagContainer;
	AttackTagContainer.AddTag(AttackAbilityTag);

	if (UAttackAbility* ActiveAttackAbility = ResolveActiveAttackAbility(AbilitySystemComponent, AttackTagContainer))
	{
		const FName RequestedSectionName = ActiveAttackAbility->GetNextAttackSectionName();
		if (RequestedSectionName.IsNone() || !ActiveAttackAbility->RequestJumpToSection(RequestedSectionName))
		{
			return;
		}

		if (!HasAuthority())
		{
			ServerRequestAttackJumpSection(RequestedSectionName);
		}
		return;
	}

	AbilitySystemComponent->TryActivateAbilitiesByTag(AttackTagContainer, true);
}

bool APdPlayerController::HasActiveAbilityWithTags(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const
{
	if (!AbilitySystemComponent || AbilityTags.IsEmpty())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!AbilitySpec.IsActive() || !AbilitySpec.Ability)
		{
			continue;
		}

		if (AbilitySpec.Ability->GetAssetTags().HasAny(AbilityTags))
		{
			return true;
		}
	}

	return false;
}

// =================================================================================================================
// ### PdUiSubsystem의 StatusViewModel을 갱신하고 컨트롤러의 하위 호환 캐시를 동기화합니다.
void APdPlayerController::RefreshUiBindings()
{
	if (UUiSubsystem* UiSubsystem = GetUiSubsystem())
	{
		UiSubsystem->RefreshStatusViewModel();
		StatusViewModel = UiSubsystem->GetStatusViewModel();
	}
	else
	{
		StatusViewModel = nullptr;
	}
}

// =================================================================================================================
// ### 현재 LocalPlayer가 소유한 UI Subsystem을 조회합니다.
UUiSubsystem* APdPlayerController::GetUiSubsystem() const
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
}
