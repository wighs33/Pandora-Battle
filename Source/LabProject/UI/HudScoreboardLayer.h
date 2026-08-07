#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/Object.h"

#include "HudScoreboardLayer.generated.h"

class APdHUD;
class UGameResultWidget;
class UHudUiRouter;
struct FGameResultPlayerStat;

/** Owns scoreboard creation, refresh scheduling and stat presentation. */
UCLASS()
class LABPROJECT_API UHudScoreboardLayer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APdHUD* InOwnerHud, UHudUiRouter* InRouter);
	void Show();
	void Hide();
	void Refresh();
	bool IsOpen() const;
	void Shutdown();

private:
	void BuildPlayerStats(TArray<FGameResultPlayerStat>& OutPlayerStats) const;

	TWeakObjectPtr<APdHUD> OwnerHud;
	TWeakObjectPtr<UHudUiRouter> Router;

	UPROPERTY(Transient)
	TObjectPtr<UGameResultWidget> ScoreboardWidget;

	FTimerHandle RefreshTimerHandle;
};
