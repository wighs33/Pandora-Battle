#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/Object.h"

#include "HudScoreboardLayer.generated.h"

class APdHUD;
class UGameResultWidget;
class UUiScreen;
class UHudUiRouter;
struct FGameResultPlayerStat;

/** 점수판 생성·갱신 예약·능력치 표시를 관리한다. */
UCLASS()
class LABPROJECT_API UHudScoreboardLayer : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(APdHUD* InOwnerHud, UHudUiRouter* InRouter);
	void Show();
	void Hide();
	bool IsOpen() const;
	void Shutdown();

	// Event Handlers --------------------------------------------------------------------------------------------------
	void Refresh();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BuildPlayerStats(TArray<FGameResultPlayerStat>& OutPlayerStats) const;

private:
	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UHudUiRouter> Router;

	UPROPERTY(Transient)
	TObjectPtr<UGameResultWidget> ScoreboardWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUiScreen> ScoreboardScreen;
	FTimerHandle RefreshTimerHandle;
};
