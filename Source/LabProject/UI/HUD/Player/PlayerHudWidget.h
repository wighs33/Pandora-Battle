#pragma once

#include "CoreMinimal.h"
#include "Online/AchievementNotificationSubscription.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "TimerManager.h"
#include "PlayerHudWidget.generated.h"

class AWorldBossRaid;
class UHorizontalBox;
class UImage;
class UKillBoxWidget;
class UUserWidget;
class UWidget;
class UWidgetClassDefinition;
class UWorldBossBarWidget;

/** HUD가 나타날 때 따로 들어오는 묶음 하나. 화면 가장자리 쪽에서 제자리로 미끄러져 들어온다. */
USTRUCT()
struct FHudEntranceGroup
{
	GENERATED_BODY()

	/** HUD 위젯 트리 안의 위젯 이름. */
	UPROPERTY(EditAnywhere, Category = "Entrance")
	FName WidgetName;

	/** 들어오기 시작하는 자리(제자리 기준 px). 묶음이 붙은 화면 가장자리 쪽으로 둔다. */
	UPROPERTY(EditAnywhere, Category = "Entrance")
	FVector2D StartOffset = FVector2D::ZeroVector;
};

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPlayerHudWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	UPlayerHudWidget(const FObjectInitializer& ObjectInitializer);

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|PlayerHUD")
	void InitializePlayerHud(UWidgetClassDefinition* InWidgetClassDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI|PlayerHUD|KillBox")
	void RebuildKillBox();

	UFUNCTION(BlueprintCallable, Category = "!UI|PlayerHUD|Achievement")
	bool RefreshAchievementAvatar();

	/** 보스 레이드 월드에서 처치 수 자리에 긴 월드 보스 체력바를 놓는다. HUD와 레이드 액터 중 나중에 준비된 쪽이 부른다. */
	void ShowWorldBossBar(AWorldBossRaid& WorldBossRaid);

	/** HUD 묶음들이 화면 가장자리에서 차례로 들어오게 한다. 처음 나타날 때와 메뉴가 닫혀 HUD가 다시 보일 때 부른다. */
	void PlayEntrance();

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|PlayerHUD|KillBox")
	void RefreshKillBox();

protected:
	virtual void OnMenuLanguageChanged() override;

private:
	void HandleAchievementDisplayChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RefreshLobbyTipVisibility();
	void RefreshKillBoxVisibility();
	bool IsTrainingRoomMap() const;
	TSubclassOf<UKillBoxWidget> ResolveKillBoxWidgetClass() const;
	float ResolveKillBoxRefreshInterval() const;
	FText ResolveTeamName(int32 TeamColorIndex) const;
	bool CanRebuildKillBox() const;
	void CenterKillBoxContainer() const;
	void ResolveActiveTeamStats(TArray<int32>& OutTeamColorIndices, TMap<int32, int32>& OutKillCountByTeam) const;
	bool AreKillBoxWidgetsBuiltForTeams(const TArray<int32>& TeamColorIndices) const;
	void BuildKillBoxWidgetsForTeams(const TArray<int32>& TeamColorIndices);
	void StartKillBoxRefreshTimer();
	void ClearKillBoxWidgets();
	void ClearKillBoxTimer();
	void ClearTransactionalFlagsForRuntimeWidget(UUserWidget* Widget) const;
	UImage* FindImageInUserWidget(UUserWidget* RootWidget, FName ImageName) const;
	UImage* FindImageInWidget(UWidget* RootWidget, FName ImageName) const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|PlayerHUD|KillBox")
	TObjectPtr<UHorizontalBox> HorizontalBox_KillBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Txt_LobbyTip;

	/** 들어오는 순서대로 적는다. 앞 묶음보다 EntranceStaggerSeconds만큼 늦게 출발한다. */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|PlayerHUD|Motion")
	TArray<FHudEntranceGroup> EntranceGroups;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|PlayerHUD|Motion", meta = (ClampMin = "0.01"))
	float EntranceSeconds = 0.34f;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|PlayerHUD|Motion", meta = (ClampMin = "0.0"))
	float EntranceStaggerSeconds = 0.045f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWidgetClassDefinition> WidgetClassDefinition = nullptr;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UKillBoxWidget>> KillBoxWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UWorldBossBarWidget> WorldBossBar;

	void StepEntrance(float DeltaSeconds);

	TOptional<float> EntranceElapsed;
	FTimerHandle KillBoxRefreshTimerHandle;
	FAchievementNotificationSubscription AchievementSubscription;
};
