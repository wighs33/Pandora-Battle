// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Operation.h"
#include "Common/Enum_Direction.h"
#include "InputActionValue.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerController.h"
#include "PdPlayerController.generated.h"

class UStatusViewModel;
class UUiSubsystem;
class UGameplayEffect;
class UAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;
class APdPlayerState;
class AActor;
class UItemInstance;
class UUserWidget;

DECLARE_LOG_CATEGORY_EXTERN(PdPlayerControllerLog, Log, All);

/**
 * 플레이어 입력, UI 바인딩, 상호작용, 장비 선택 요청을 중계하는 플레이어 컨트롤러입니다.
 *
 * APdPlayerController는 캐릭터의 핵심 게임플레이 상태를 직접 소유하기보다,
 * 로컬 플레이어에게 필요한 입력/초기화/UI 연결을 처리하고 실제 처리는 각 전담 객체로 넘깁니다.
 *
 * 주요 책임:
 * - Enhanced Input MappingContext를 로컬 플레이어에 등록하고 입력 액션을 바인딩합니다.
 * - Pawn, PdPlayerState, Input 준비 상태를 감지해 Blueprint 초기화 이벤트를 호출합니다.
 * - StatusViewModel은 PdUiSubsystem에서 가져와 위젯에 연결합니다.
 * - 장비 선택 UI 입력을 EquipmentComponent와 Equip/Unequip Ability 실행 요청으로 변환합니다.
 * - 상호작용 입력을 서버로 전달하고 상호작용 보상을 처리합니다.
 *
 * 주요 사용 흐름:
 * - 입력 초기화는 BeginPlay(), ReceivedPlayer(), SetPawn(), OnRep_PlayerState(), SetupInputComponent()에서 재평가됩니다.
 * - 위젯 생성 후 ApplyStatusViewModelToWidget()을 호출하면 PdUiSubsystem이 관리하는 StatusViewModel이 위젯에 적용됩니다.
 * - 무기 선택 UI는 CachedFirstWeapon/CachedSecondWeapon/CachedThirdWeapon을 채운 뒤 OnSelectedPandoraAndWeapon()을 호출합니다.
 * - 상호작용은 InteractInputAction 입력에서 시작되며, 클라이언트는 ServerHandleInteract()를 통해 서버 처리를 요청합니다.
 */
UCLASS()
class LABPROJECT_API APdPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	APdPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	// GameFrameworkComponentManager에 컨트롤러를 등록합니다.
	virtual void PreInitializeComponents() override;

	// 입력 매핑과 초기화 상태를 다시 평가합니다.
	virtual void BeginPlay() override;

	// GameFrameworkComponentManager 등록을 해제합니다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 로컬 플레이어가 연결된 뒤 입력 매핑과 초기화 상태를 다시 평가합니다.
	virtual void ReceivedPlayer() override;

	// Pawn 변경 시 Pawn 준비 이벤트와 전체 초기화 준비 상태를 다시 평가합니다.
	virtual void SetPawn(APawn* InPawn) override;

	// PlayerState 복제 완료 시 PdPlayerState 준비 이벤트와 전체 초기화 준비 상태를 다시 평가합니다.
	virtual void OnRep_PlayerState() override;

	// Enhanced Input 액션을 C++ 핸들러에 바인딩합니다.
	virtual void SetupInputComponent() override;
	
	// 위젯에 StatusViewModel을 적용합니다. ViewModel 소유권은 PdUiSubsystem에 있습니다.
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void ApplyStatusViewModelToWidget(UUserWidget* InWidget);

	// UI에서 현재 로컬 플레이어의 StatusViewModel을 조회할 때 사용합니다.
	UFUNCTION(BlueprintPure, Category = "!ViewModel")
	UStatusViewModel* GetStatusViewModel() const;

	// 스탯 태그와 수치를 기반으로 GameplayEffect를 적용합니다. 클라이언트 호출은 서버 RPC로 전달됩니다.
	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Stat", meta = (GameplayTagFilter = "Status"))
	bool ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EEnum_Operation Operation = EEnum_Operation::Add, float Level = 1.f);

	// 스탯 업 버튼 클릭 시 태그 카테고리에 맞는 기본 배율과 연산으로 GameplayEffect를 적용합니다.
	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Stat", meta = (GameplayTagFilter = "Status"))
	void OnClicked_StatUpButton(FGameplayTag StatTag);

	// 로컬 플레이어의 Enhanced Input Subsystem에 기본 MappingContext를 한 번만 추가합니다.
	UFUNCTION(BlueprintCallable, Category = "!Input")
	void AddInputMapping();

	// 장비 선택 UI의 방향 입력을 실제 장착/해제 Ability 요청으로 변환합니다.
	UFUNCTION(BlueprintCallable, Category = "!Equip", meta = (DisplayName = "OnSelected_PandoraAndWeapon"))
	void OnSelectedPandoraAndWeapon(EEnum_Direction Direction);

	// 프로젝트 전용 PlayerState를 안전하게 조회합니다.
	UFUNCTION(BlueprintPure, Category = "!Initialization")
	APdPlayerState* GetPdPlayerState() const;

	// 로컬 플레이어, InputComponent, 기본 MappingContext 등록이 모두 준비됐는지 확인합니다.
	UFUNCTION(BlueprintPure, Category = "!Initialization")
	bool IsInputReady() const;

	// Pawn, PdPlayerState, Input이 모두 준비됐는지 확인합니다.
	UFUNCTION(BlueprintPure, Category = "!Initialization")
	bool IsInitializationReady() const;

	// Pawn이 새로 준비될 때 Blueprint에서 초기화 후처리를 연결합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "!Initialization", meta = (DisplayName = "On Pawn Ready"))
	void ReceivePawnReady(APawn* InPawn);

	// PdPlayerState가 새로 준비될 때 Blueprint에서 초기화 후처리를 연결합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "!Initialization", meta = (DisplayName = "On PdPlayerState Ready"))
	void ReceivePdPlayerStateReady(APdPlayerState* InPlayerState);

	// 입력 매핑과 EnhancedInputComponent가 준비됐을 때 Blueprint에서 입력 의존 초기화를 연결합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "!Initialization", meta = (DisplayName = "On Input Ready"))
	void ReceiveInputReady();

	// Pawn, PdPlayerState, Input이 모두 준비된 뒤 Blueprint에서 최종 초기화를 연결합니다.
	UFUNCTION(BlueprintImplementableEvent, Category = "!Initialization", meta = (DisplayName = "On Controller Initialization Ready"))
	void ReceiveControllerInitializationReady();

public:
	// 로컬 플레이어에 기본으로 추가할 Enhanced Input MappingContext입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input")
	TObjectPtr<UInputMappingContext> DefaultInputMapping;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tag", meta = (Categories = "Action"))
	FGameplayTag EquipAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tag", meta = (Categories = "Action"))
	FGameplayTag UnequipAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Tag", meta = (Categories = "Action"))
	FGameplayTag AttackAbilityTag;

	// 이동 입력 액션입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input")
	TObjectPtr<UInputAction> MoveInputAction;

	// 카메라 회전 입력 액션입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input")
	TObjectPtr<UInputAction> LookInputAction;

	// 상호작용 입력 액션입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input")
	TObjectPtr<UInputAction> InteractInputAction;

	// 공격 입력 액션입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Input")
	TObjectPtr<UInputAction> AttackInputAction;

	// 스탯 업 버튼 클릭 시 사용할 기본 GameplayEffect 클래스입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AbilitySystem|Stat")
	TSubclassOf<UGameplayEffect> StatUpGameplayEffectClass;

	// 하위 호환용 캐시입니다. 신규 코드는 GetStatusViewModel() 또는 ApplyStatusViewModelToWidget()을 사용합니다.
	UPROPERTY(Transient, BlueprintReadOnly, Category = "!ViewModel", meta = (DeprecatedProperty, DeprecationMessage = "StatusViewModel is owned by PdUiSubsystem. Use GetStatusViewModel() or ApplyStatusViewModelToWidget() instead."))
	TObjectPtr<UStatusViewModel> StatusViewModel;

	// 장비 선택 UI에서 왼쪽 방향으로 선택할 무기입니다.
	UPROPERTY(Transient, BlueprintReadWrite, Category = "!Equip")
	TObjectPtr<UItemInstance> CachedFirstWeapon;

	// 장비 선택 UI에서 위쪽 방향으로 선택할 무기입니다.
	UPROPERTY(Transient, BlueprintReadWrite, Category = "!Equip")
	TObjectPtr<UItemInstance> CachedSecondWeapon;

	// 장비 선택 UI에서 오른쪽 방향으로 선택할 무기입니다.
	UPROPERTY(Transient, BlueprintReadWrite, Category = "!Equip")
	TObjectPtr<UItemInstance> CachedThirdWeapon;

protected:
	// 스탯 변경 GameplayEffect 적용을 서버 권한에서 처리합니다.
	UFUNCTION(Server, Reliable)
	void ServerApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EEnum_Operation Operation, float Level);

	// 클라이언트 상호작용 요청을 서버 권한에서 처리합니다.
	UFUNCTION(Server, Reliable)
	void ServerHandleInteract(AActor* InteractableActor);
	UFUNCTION(Server, Reliable)
	void ServerRequestAttackJumpSection(FName RequestedSectionName);

	// 이동 입력 값을 Pawn의 이동 입력으로 변환합니다.
	void HandleMoveInput(const FInputActionValue& InputValue);

	// 시점 입력 값을 컨트롤러 Yaw/Pitch 입력으로 변환합니다.
	void HandleLookInput(const FInputActionValue& InputValue);

	// 현재 상호작용 대상을 찾아 서버 처리로 전달합니다.
	void HandleInteractInput(const FInputActionValue& InputValue);

	// 공격 입력을 공격 Ability 활성화 요청으로 변환합니다.
	void HandleAttackInput(const FInputActionValue& InputValue);
	bool HasActiveAbilityWithTags(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTagContainer& AbilityTags) const;

	// 실제 스탯 변경 GameplayEffect 적용을 수행합니다. 서버 권한에서 호출됩니다.
	bool ApplyStatUpEffectByTagInternal(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude, EEnum_Operation Operation, float Level);

	// Pawn, PdPlayerState, Input 준비 상태를 검사하고 준비 이벤트를 필요한 시점에 한 번씩 호출합니다.
	void EvaluateInitializationState();

	// PdUiSubsystem의 StatusViewModel을 갱신하고 하위 호환 캐시를 동기화합니다.
	void RefreshUiBindings();

	// 현재 LocalPlayer가 소유한 UI Subsystem을 조회합니다.
	UUiSubsystem* GetUiSubsystem() const;

	// 기본 MappingContext를 중복 등록하지 않기 위한 플래그입니다.
	UPROPERTY(Transient)
	bool bHasAddedDefaultInputMapping = false;

	// ReceiveInputReady() 중복 호출을 막기 위한 플래그입니다.
	UPROPERTY(Transient)
	bool bHasBroadcastInputReady = false;

	// ReceiveControllerInitializationReady() 중복 호출을 막기 위한 플래그입니다.
	UPROPERTY(Transient)
	bool bHasBroadcastInitializationReady = false;

	// Pawn 교체를 감지해 ReceivePawnReady()를 다시 호출하기 위한 캐시입니다.
	TWeakObjectPtr<APawn> LastReadyPawn;

	// PlayerState 교체를 감지해 ReceivePdPlayerStateReady()를 다시 호출하기 위한 캐시입니다.
	TWeakObjectPtr<APdPlayerState> LastReadyPlayerState;
};
