#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "UI/GameResultTypes.h"
#include "ExperienceGameState.generated.h"

class UExperienceManagerComponent;
class UGameResultWidget;
class UMatchRuleDefinition;
class APlayerController;

UENUM(BlueprintType)
enum class EPdMatchTimerPhase : uint8
{
	Inactive,
	Running,
	Expired,
	Suppressed
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FReplicatedMatchTimerState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Match Rules|Timer")
	EPdMatchTimerPhase Phase = EPdMatchTimerPhase::Inactive;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Match Rules|Timer")
	float EndServerTimeSeconds = 0.0f;

	bool operator==(const FReplicatedMatchTimerState& Other) const
	{
		return Phase == Other.Phase
			&& FMath::IsNearlyEqual(EndServerTimeSeconds, Other.EndServerTimeSeconds);
	}
};

UCLASS()
class LABPROJECT_API AExperienceGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AExperienceGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Experience API
	UExperienceManagerComponent* GetExperienceManagerComponent() const { return ExperienceManagerComponent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetMatchRuleDefinition(UMatchRuleDefinition* InMatchRuleDefinition);
	const UMatchRuleDefinition* GetMatchRuleDefinition() const { return MatchRuleDefinition; }

	void SetMatchTimerState(EPdMatchTimerPhase InPhase, float InEndServerTimeSeconds = 0.0f);
	EPdMatchTimerPhase GetMatchTimerPhase() const { return MatchTimerState.Phase; }
	bool IsMatchTimerSuppressed() const { return MatchTimerState.Phase == EPdMatchTimerPhase::Suppressed; }
	bool TryGetMatchTimerRemainingSeconds(float& OutRemainingSeconds) const;

	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "!GameResult")
	void Multicast_ShowGameResult(
		const FText& WinnerTitle,
		int32 WinnerTeamColorIndex,
		const FText& MaxKillerName,
		int32 MaxKillCount,
		const TArray<FGameResultPlayerStat>& PlayerStats);

private:
	UFUNCTION()
	void OnRep_MatchRuleDefinition();

	UFUNCTION()
	void OnRep_MatchTimerState();

	void RefreshLocalHudTimer() const;
	void SaveLocalMatchRecord(APlayerController* LocalPlayerController, const TArray<FGameResultPlayerStat>& PlayerStats) const;

	//------------------------------------------------------------------------------------------------------------------
	//--- Components
	UPROPERTY(VisibleAnywhere, Category = "!Experience", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExperienceManagerComponent> ExperienceManagerComponent;

	UPROPERTY(ReplicatedUsing = OnRep_MatchRuleDefinition, VisibleInstanceOnly, Category = "!Match Rules", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMatchRuleDefinition> MatchRuleDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_MatchTimerState, VisibleInstanceOnly, Category = "!Match Rules",
		meta = (AllowPrivateAccess = "true"))
	FReplicatedMatchTimerState MatchTimerState;

	UPROPERTY(EditDefaultsOnly, Category = "!GameResult", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameResultWidget> GameResultWidgetClass;
};
