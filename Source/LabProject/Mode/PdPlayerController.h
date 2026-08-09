#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/GameResultTypes.h"
#include "UI/KillLogTypes.h"
#include "UI/NotificationData.h"
#include "PdPlayerController.generated.h"

class UChatControllerComponent;
class UControllerInputComponent;
class UControllerInputDefinition;
class UControllerPresentationComponent;
class UControllerProfileSyncComponent;
class UControllerSessionComponent;
class UPlayerControllerDefinition;
struct FStreamableHandle;

DECLARE_LOG_CATEGORY_EXTERN(PdPlayerControllerLog, Log, All);

/**
 * Stable network and Blueprint facade for player-controller features.
 *
 * Runtime responsibility is composed from focused components. Keeping RPCs on
 * the controller preserves existing network ownership and external call sites.
 */
UCLASS()
class LABPROJECT_API APdPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APdPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void AcknowledgePossession(APawn* P) override;

	UFUNCTION(BlueprintCallable, Category = "!Camera|Clamp")
	void ApplyCameraViewPitchClamp();

	UFUNCTION(Client, Reliable, Category = "!UI|Notification")
	void Client_ShowRightNotification(const FPdNotificationData& NotificationData);

	UFUNCTION(Client, Reliable, Category = "!UI|KillLog")
	void Client_AddKillLogEntry(const FKillLogEntry& KillLogEntry);

	UFUNCTION(Client, Reliable, Category = "!UI|GoldenKill")
	void Client_ShowGoldenKillAnnouncement(const FText& AnnouncementText);

	UFUNCTION(Client, Reliable, Category = "!Reward|Gold")
	void Client_AddGameVictoryGoldReward(const FString& PlayerId, int32 GoldReward);

	UFUNCTION(Client, Reliable, Category = "!Achievement")
	void Client_AddCollectedItemCount(const FString& PlayerId, int32 ItemCount);

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
	void Client_ResetRespawnedPawnStateAtTransform(const FTransform& RespawnTransform);

	UFUNCTION(Client, Reliable, Category = "!Portal")
	void Client_ApplyPortalTeleport(
		const FVector& TargetLocation,
		const FRotator& TargetRotation,
		const FVector& TargetVelocity,
		const FRotator& TargetControlRotation);

	void RequestLocalCosmeticProfileSync();

	UFUNCTION(Client, Reliable, Category = "!Skin|Profile")
	void Client_RequestLocalCosmeticProfileSync();

	/**
	 * Submits an unverified local cosmetic profile to the listen server.
	 * The server applies the configured claim policy and resolves every name
	 * against its own canonical skin catalog before granting anything.
	 */
	UFUNCTION(Server, Reliable, Category = "!Cosmetic|Profile")
	void Server_SubmitLocalCosmeticProfile(
		const TArray<FName>& OwnedSkinNames,
		FName SelectedAchievementId);

	UFUNCTION(BlueprintPure, Category = "!Input")
	UControllerInputDefinition* GetLoadedInputDefinition() const;

	bool RequestExitMatchToTitle();

	UFUNCTION(BlueprintPure, Category = "!Components")
	UControllerPresentationComponent* GetControllerPresentationComponent() const
	{
		return ControllerPresentationComponent;
	}

	UFUNCTION(BlueprintPure, Category = "!Components")
	UControllerProfileSyncComponent* GetControllerProfileSyncComponent() const
	{
		return ControllerProfileSyncComponent;
	}

	UFUNCTION(BlueprintPure, Category = "!Components")
	UControllerSessionComponent* GetControllerSessionComponent() const
	{
		return ControllerSessionComponent;
	}

protected:
	UControllerInputComponent* GetControllerInputComponent() const;

private:
	friend class UControllerSessionComponent;

	void ApplyControllerDefinition();
	UPlayerControllerDefinition* LoadControllerDefinition();
	void BeginControllerDefinitionPreload();
	void HandleControllerDefinitionPreloaded(uint32 RequestGeneration);
	void ReleaseControllerDefinitionPreload();
	void ApplyDefaultInputDefinitionIfMissing();

	//------------------------------------------------------------------------------------------------------------------
	//--- Composition
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Controller|Definition",
		meta = (AllowPrivateAccess = "true", AllowedTypes = "PlayerControllerDefinition"))
	TSoftObjectPtr<UPlayerControllerDefinition> PlayerControllerDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerControllerDefinition> LoadedPlayerControllerDefinition;

	TSharedPtr<FStreamableHandle> PlayerControllerDefinitionLoadHandle;
	uint32 PlayerControllerDefinitionLoadGeneration = 0;
};
