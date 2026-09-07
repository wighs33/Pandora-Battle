#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Definition/Level/LevelDefinition.h"
#include "UObject/PrimaryAssetId.h"
#include "LobbyGameState.generated.h"

class UExperienceManagerComponent;
class UExperienceDefinition;

DECLARE_MULTICAST_DELEGATE(FOnLobbyStateChanged);

/**
 * 복제되는 로비 설정과 참가자 목록의 변경을 알린다. 화면 갱신은 HUD가 구독해서 처리한다.
 */

UCLASS()
class LABPROJECT_API ALobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALobbyGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UExperienceManagerComponent* GetExperienceManagerComponent() const { return ExperienceManagerComponent; }

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//------------------------------------------------------------------------------------------------------------------

	FOnLobbyStateChanged OnLobbyStateChanged;
	void SetExperienceLoadFailed(bool bFailed);
	bool HasExperienceLoadFailed() const;

	void SetSelectedMapOption(const FLobbyMatchMapOption& InMapOption);

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	FLobbyMatchMapOption GetSelectedMapOption() const;

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	FName GetSelectedMapKey() const { return SelectedMapOption.MapKey; }

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	int32 GetSelectedMapMaxPlayerCount() const { return FMath::Max(SelectedMapOption.MaxPlayerCount, 1); }

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	bool IsSelectedMapImageReady() const;

	void SetGameStartPending(bool bInStartPending, double InStartEndServerTimeSeconds);

	UFUNCTION(BlueprintPure, Category = "!Lobby|Start")
	bool IsGameStartPending() const { return bStartPending; }

	UFUNCTION(BlueprintPure, Category = "!Lobby|Start")
	double GetGameStartEndServerTimeSeconds() const { return GameStartEndServerTimeSeconds; }

	UFUNCTION(BlueprintPure, Category = "!Lobby|Start")
	float GetGameStartRemainingSeconds() const;

protected:
	UFUNCTION()
	void OnRep_SelectedMapOption();

	UFUNCTION()
	void OnRep_GameStartState();

private:
	UFUNCTION()
	void OnRep_ExperienceLoadFailed();

	void HandleExperienceLoaded(const UExperienceDefinition* Experience);
	void HandleExperienceLoadFailed(FPrimaryAssetId ExperienceId, const FString& FailureMessage);
	void NotifyLobbyStateChanged();
	void RefreshGameEntryContentPreload() const;

	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPROPERTY(VisibleAnywhere, Category = "!Experience", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExperienceManagerComponent> ExperienceManagerComponent;

	//------------------------------------------------------------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_ExperienceLoadFailed)
	bool bExperienceLoadFailed = false;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedMapOption, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Lobby|Map", meta = (AllowPrivateAccess = "true"))
	FLobbyMatchMapOption SelectedMapOption;

	UPROPERTY(ReplicatedUsing = OnRep_GameStartState, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Lobby|Start", meta = (AllowPrivateAccess = "true"))
	bool bStartPending = false;

	UPROPERTY(ReplicatedUsing = OnRep_GameStartState, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Lobby|Start", meta = (AllowPrivateAccess = "true"))
	double GameStartEndServerTimeSeconds = 0.0;
};
