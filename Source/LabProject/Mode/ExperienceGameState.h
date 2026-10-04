#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Common/GameResultTypes.h"
#include "ExperienceGameState.generated.h"

class UExperienceManagerComponent;
class UMatchRuleDefinition;
class APlayerController;

UENUM(BlueprintType)
enum class EMatchTimerPhase : uint8
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

public:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Match Rules|Timer")
	EMatchTimerPhase Phase = EMatchTimerPhase::Inactive;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Match Rules|Timer")
	float EndServerTimeSeconds = 0.0f;

	// Public API ------------------------------------------------------------------------------------------------------
	bool operator==(const FReplicatedMatchTimerState& Other) const
	{
		return Phase == Other.Phase
			&& FMath::IsNearlyEqual(EndServerTimeSeconds, Other.EndServerTimeSeconds);
	}
};

DECLARE_MULTICAST_DELEGATE_FiveParams(FPdGameResultReceived, const FText& /*WinnerTitle*/, int32 /*WinnerTeamColorIndex*/,
	const FText& /*MaxKillerName*/, int32 /*MaxKillCount*/, const TArray<FGameResultPlayerStat>& /*PlayerStats*/);

UCLASS()
class LABPROJECT_API AExperienceGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	AExperienceGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UExperienceManagerComponent* GetExperienceManagerComponent() const { return ExperienceManagerComponent; }

	void SetMatchRuleDefinition(const UMatchRuleDefinition* InMatchRuleDefinition);
	const UMatchRuleDefinition* GetMatchRuleDefinition() const { return MatchRuleDefinition; }

	void SetMatchTimerState(EMatchTimerPhase InPhase, float InEndServerTimeSeconds = 0.0f);
	EMatchTimerPhase GetMatchTimerPhase() const { return MatchTimerState.Phase; }
	bool TryGetMatchTimerRemainingSeconds(float& OutRemainingSeconds) const;

	// 화면(HUD)이 구독한다. 타이머 상태나 경기 규칙이 바뀌면 타이머 표시를, 경기가 끝나면 결과 창을 그린다.
	FSimpleMulticastDelegate& OnMatchTimerChanged() { return MatchTimerChanged; }
	FPdGameResultReceived& OnGameResultReceived() { return GameResultReceived; }

	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(NetMulticast, Reliable, BlueprintCallable, Category = "!GameResult")
	void Multicast_ShowGameResult(
		const FText& WinnerTitle,
		int32 WinnerTeamColorIndex,
		const FText& MaxKillerName,
		int32 MaxKillCount,
		const TArray<FGameResultPlayerStat>& PlayerStats);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_MatchRuleDefinition();

	UFUNCTION()
	void OnRep_MatchTimerState();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void SaveLocalMatchRecord(APlayerController* LocalPlayerController, const TArray<FGameResultPlayerStat>& PlayerStats) const;

private:
	UPROPERTY(VisibleAnywhere, Category = "!Experience", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UExperienceManagerComponent> ExperienceManagerComponent;

	UPROPERTY(ReplicatedUsing = OnRep_MatchRuleDefinition, VisibleInstanceOnly, Category = "!Match Rules", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UMatchRuleDefinition> MatchRuleDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_MatchTimerState, VisibleInstanceOnly, Category = "!Match Rules",
		meta = (AllowPrivateAccess = "true"))
	FReplicatedMatchTimerState MatchTimerState;

	FSimpleMulticastDelegate MatchTimerChanged;
	FPdGameResultReceived GameResultReceived;
};
