#pragma once

#include "CoreMinimal.h"
#include "Common/GameResultTypes.h"
#include "Common/KillLogTypes.h"
#include "Common/RewardNotificationTypes.h"
#include "Component/Character/AbilitySystemReadySubscription.h"
#include "GameFramework/PlayerController.h"
#include "PdPlayerController.generated.h"

class ACharacterBase;
class UChatControllerComponent;
class UControllerInputComponent;
class UControllerInputDefinition;
class UControllerLagCompensationComponent;
class UControllerPresentationComponent;
class UControllerProfileSyncComponent;
class UControllerSessionComponent;
class UPlayerControllerDefinition;
class UPdAbilitySystemComponent;
class UPlayerNotificationComponent;
class FContentLease;

DECLARE_LOG_CATEGORY_EXTERN(PdPlayerControllerLog, Log, All);

/**
 * 플레이어의 네트워크 요청과 컨트롤러 생명주기를 연결한다.
 *
 * 입력·화면 표시·프로필·세션 처리는 각 컴포넌트가 맡고,
 * 컨트롤러는 소유 클라이언트의 RPC 진입점과 초기화 순서를 관리한다.
 */
UCLASS()
class LABPROJECT_API APdPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void SetPawn(APawn* InPawn) override;

	// Public API ------------------------------------------------------------------------------------------------------
	APdPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 조종 중인 캐릭터의 ASC 준비 계약. Pawn 교체·리스폰 뒤 새 캐릭터가 준비될 때마다 다시 알린다.
	// 구독은 FAbilitySystemReadySubscription::SubscribeToPossessedCharacter로 한다.
	ACharacterBase* GetReadyPossessedCharacter() const;
	FDelegateHandle RegisterOnPossessedCharacterAbilitySystemReady(const FPdAbilitySystemReadyDelegate::FDelegate& Delegate);
	void UnregisterOnPossessedCharacterAbilitySystemReady(FDelegateHandle Handle);

	void RequestLocalCosmeticProfileSync();

	UFUNCTION(BlueprintPure, Category = "!Input")
	UControllerInputDefinition* GetLoadedInputDefinition() const;

	// 입력 정의는 비동기로 적용된다. 입력 키를 표시하는 HUD는 적용될 때마다 이 알림으로 다시 그린다.
	FDelegateHandle RegisterOnInputDefinitionApplied(const FSimpleDelegate& Delegate);
	void UnregisterOnInputDefinitionApplied(FDelegateHandle Handle);

	bool RequestExitMatchToTitle();

	UPlayerNotificationComponent* GetPlayerNotificationComponent() const { return NotificationComponent.Get(); }

	UControllerPresentationComponent* GetControllerPresentationComponent() const
	{
		return ControllerPresentationComponent;
	}

	// 로비의 입력 모드와 로딩 화면 정책은 구체적인 자식 타입 대신 이 정책으로 구분한다.
	virtual bool UsesLobbyPresentation() const { return false; }

	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(Client, Reliable, Category = "!UI|Notification")
	void Client_ShowRewardNotifications(const TArray<FPdRewardNotification>& Rewards);

	UFUNCTION(Client, Reliable, Category = "!UI|KillLog")
	void Client_AddKillLogEntry(const FKillLogEntry& KillLogEntry);

	UFUNCTION(Client, Reliable, Category = "!UI|GoldenKill")
	void Client_ShowGoldenKillAnnouncement(const FText& AnnouncementText);

	UFUNCTION(Client, Reliable, Category = "!Reward|Gold")
	void Client_AddGameVictoryGoldReward(int32 GoldReward);

	UFUNCTION(Client, Reliable, Category = "!Achievement")
	void Client_AddCollectedItemCount(int32 ItemCount);

	UFUNCTION(Client, Reliable, Category = "!Match")
	void Client_TravelToTitleWithGameResult(
		const FGameResultPresentationData& GameResultData,
		const FString& TitleMapName);

	UFUNCTION(Client, Reliable, Category = "!Match")
	void Client_TravelToTitleWithoutGameResult(
		const FString& TitleMapName);

	UFUNCTION(Client, Reliable, Category = "!UI|Respawn")
	void Client_StartRespawnDelayCountdown(float DelaySeconds);

	UFUNCTION(Client, Reliable, Category = "!UI|Respawn")
	void Client_HideRespawnDelayCountdown();

	UFUNCTION(Client, Reliable, Category = "!Respawn")
	void Client_ResetRespawnedPawnStateAtTransform(APawn* RespawnedPawn, const FTransform& RespawnTransform);

	UFUNCTION(Client, Reliable, Category = "!Portal")
	void Client_ApplyPortalTeleport(
		const FVector& TargetLocation,
		const FRotator& TargetRotation,
		const FVector& TargetVelocity,
		const FRotator& TargetControlRotation);

	UFUNCTION(Client, Reliable, Category = "!Skin|Profile")
	void Client_RequestLocalCosmeticProfileSync();

	/**
	 * 로컬 저장 데이터의 스킨·업적 주장을 서버에 제출한다.
	 * 서버의 카탈로그 대조는 존재 여부 검증이며, 실제 소유권 인증은 아니다.
	 */
	UFUNCTION(Server, Reliable, Category = "!Cosmetic|Profile")
	void Server_SubmitLocalCosmeticProfile(
		const TArray<FName>& OwnedSkinNames,
		FName SelectedAchievementId);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleControllerDefinitionPreloaded();
	void HandlePossessedCharacterAbilitySystemReady(ACharacterBase* ReadyCharacter, UPdAbilitySystemComponent* AbilitySystemComponent);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyControllerDefinition();
	const UPlayerControllerDefinition* GetControllerDefinition() const;
	void BeginControllerDefinitionPreload();
	void RefreshControllerInput();
	void ObservePossessedCharacter(ACharacterBase* NewPossessedCharacter);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UControllerInputComponent> ControllerInputComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UChatControllerComponent> ChatControllerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UControllerPresentationComponent> ControllerPresentationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UControllerProfileSyncComponent> ControllerProfileSyncComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UControllerSessionComponent> ControllerSessionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UControllerLagCompensationComponent> ControllerLagCompensationComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Components")
	TObjectPtr<UPlayerNotificationComponent> NotificationComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Definition",
		meta = (AllowPrivateAccess = "true", AllowedTypes = "PlayerControllerDefinition"))
	TSoftObjectPtr<UPlayerControllerDefinition> PlayerControllerDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerControllerDefinition> LoadedPlayerControllerDefinition;

	TSharedPtr<FContentLease> PlayerControllerDefinitionLease;

	TWeakObjectPtr<ACharacterBase> ObservedPossessedCharacter;
	FAbilitySystemReadySubscription PossessedCharacterReadySubscription;
	FPdAbilitySystemReadyDelegate OnPossessedCharacterAbilitySystemReady;
};
