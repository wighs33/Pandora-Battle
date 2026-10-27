#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldBossBarWidget.generated.h"

class AWorldBossRaid;
class UProgressBar;
class UTextBlock;

/**
 * 보스 레이드 HUD 위쪽의 긴 월드 보스 체력바. 모든 서버가 함께 깎는 체력과 내 도전의 남은 시간, 이번 도전에서 넣은 딜을 보여 준다.
 *
 * 위젯 블루프린트 없이 코드로 구성하고, AWorldBossRaid의 복제 값을 매 프레임 읽는다. 글자는 바뀐 값이 있을 때만 다시 만든다.
 * 체력이 줄면 밝은 잔상 막대가 뒤따라 줄어 방금 깎인 양이 보인다.
 */
UCLASS()
class LABPROJECT_API UWorldBossBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void SetWorldBossRaid(AWorldBossRaid* InRaid) { Raid = InRaid; }

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleLanguageChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	UTextBlock* MakeText(int32 FontSize, ETextJustify::Type Justification);
	UProgressBar* MakeBar(const FLinearColor& BackgroundColor, const FLinearColor& FillColor);
	void ApplyLanguageFonts();
	void RefreshTexts();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AttemptText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> TrailBar;

	TWeakObjectPtr<AWorldBossRaid> Raid;
	float TrailPercent = 1.0f;

	/** 화면에 그린 값. 같으면 글자를 다시 만들지 않는다. */
	struct FShownValues
	{
		int64 Health = -1;
		int64 MaxHealth = -1;
		int64 SecondsLeft = -1;
		int64 Damage = -1;
		int32 Phase = -1;

		bool operator==(const FShownValues& Other) const = default;
	};
	FShownValues Shown;
};
