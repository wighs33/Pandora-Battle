#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "LobbyGameState.generated.h"

class UExperienceManagerComponent;

UCLASS()
class LABPROJECT_API ALobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALobbyGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UExperienceManagerComponent* GetExperienceManagerComponent() const { return ExperienceManagerComponent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetSelectedMapOption(const FLobbyMatchMapOption& InMapOption);

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	FLobbyMatchMapOption GetSelectedMapOption() const { return SelectedMapOption; }

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	FName GetSelectedMapKey() const { return SelectedMapOption.MapKey; }

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	int32 GetSelectedMapMaxPlayerCount() const { return FMath::Max(SelectedMapOption.MaxPlayerCount, 1); }

	UFUNCTION(BlueprintPure, Category = "!Lobby|Map")
	bool IsSelectedMapImageReady() const
	{
		return !SelectedMapOption.MapKey.IsNone()
			&& IsValid(SelectedMapOption.Thumbnail);
	}

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
	void RefreshLocalLobbyUI() const;

	UPROPERTY(VisibleAnywhere, Category = "!Experience", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExperienceManagerComponent> ExperienceManagerComponent;

	UPROPERTY(ReplicatedUsing = OnRep_SelectedMapOption, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Lobby|Map", meta = (AllowPrivateAccess = "true"))
	FLobbyMatchMapOption SelectedMapOption;

	UPROPERTY(ReplicatedUsing = OnRep_GameStartState, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Lobby|Start", meta = (AllowPrivateAccess = "true"))
	bool bStartPending = false;

	UPROPERTY(ReplicatedUsing = OnRep_GameStartState, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Lobby|Start", meta = (AllowPrivateAccess = "true"))
	double GameStartEndServerTimeSeconds = 0.0;
};
