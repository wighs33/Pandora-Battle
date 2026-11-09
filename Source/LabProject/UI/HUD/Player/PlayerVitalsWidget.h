#pragma once

#include "CoreMinimal.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "AttributeSet.h"
#include "Component/Character/AbilitySystemReadySubscription.h"
#include "UI/Common/UiMotion.h"
#include "PlayerVitalsWidget.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UPlayerVitalsWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnMenuLanguageChanged() override;

private:
	void HandlePossessedCharacterReady(ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent);
	void HandleResourceChanged(const FOnAttributeChangeData& Data);
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void HandleMaxStaminaChanged(const FOnAttributeChangeData& Data);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BindStaminaAttributeDelegates();
	void UnbindStaminaAttributeDelegates();
	void BeginGameSettingContentPreload();
	void ReleaseGameSettingContentPreload();
	void RefreshStaminaFillTint();
	UProgressBar* ResolveStaminaBar() const;
	void RefreshResourceReadouts();

	// 체력과 레벨의 움직임 -----------------------------------------------------------------------------------------
	/** 체력 목표를 다시 읽는다. bAnimate가 false면(처음 연결, 부활) 꼬리와 숫자를 바로 맞춘다. */
	void RefreshHealthMotion(bool bAnimate);
	/** 레벨을 다시 읽고, 올랐으면 레벨 표시를 튀게 한다. */
	void RefreshLevelMotion(bool bAnimate);
	void StepHealthMotion(float DeltaSeconds);
	void StepLevelMotion(float DeltaSeconds);
	void WriteHealthText() const;
	bool IsHealthLow() const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Player Vitals")
	TObjectPtr<UProgressBar> StaminaBar;

	/** 체력 막대. 채움 비율은 뷰모델이 정하고, 여기서는 맞았을 때 채움 색만 잠깐 바꾼다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Player Vitals")
	TObjectPtr<UProgressBar> HealthBar;

	/** 체력 막대 뒤에서 방금 깎인 양을 잠시 보여 주고 따라 내려오는 막대. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Player Vitals")
	TObjectPtr<UProgressBar> HealthTrailBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Player Vitals")
	TObjectPtr<UTextBlock> HealthValue;

	/** 체력 최대값("/ 80"). 없으면 HealthValue 하나에 "50 / 80"으로 쓴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Player Vitals")
	TObjectPtr<UTextBlock> HealthMaxValue;

	/** 보호막("+20"). 보호막이 없으면 접힌다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Player Vitals")
	TObjectPtr<UTextBlock> ShieldValue;

	/** 레벨이 오르면 튀어 오르는 레벨 표시 묶음. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Player Vitals")
	TObjectPtr<UWidget> LevelBadge;

	/** 깎인 꼬리가 따라 내려오기 전에 머무는 시간. 연타를 맞는 동안은 계속 머문다. */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|Player Vitals|Motion", meta = (ClampMin = "0.0"))
	float DamageTrailHoldSeconds = 0.35f;

	/** 깎인 꼬리가 따라 내려오는 빠르기. 클수록 빨리 닫힌다. */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|Player Vitals|Motion", meta = (ClampMin = "0.1"))
	float DamageTrailCatchUpSpeed = 6.f;

	/** 체력 숫자가 새 값으로 굴러가는 빠르기. */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|Player Vitals|Motion", meta = (ClampMin = "0.1"))
	float HealthCountSpeed = 14.f;

	/** 맞았을 때 체력 막대가 밝게 번쩍이는 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|Player Vitals|Motion", meta = (ClampMin = "0.0"))
	float HitFlashSeconds = 0.16f;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Player Vitals|Motion")
	FLinearColor HitFlashColor = FLinearColor(0.86f, 0.80f, 0.69f, 1.f);

	/** 체력이 이 비율 아래면 체력 숫자가 붉게 맥박친다. */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|Player Vitals|Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthFraction = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Player Vitals|Motion", meta = (ClampMin = "0.1"))
	float LowHealthPulseSeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "!UI|Player Vitals|Motion")
	FLinearColor LowHealthTextColor = FLinearColor(0.70f, 0.07f, 0.05f, 1.f);

private:
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> ResourceDelegateHandles;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;

	FDelegateHandle StaminaChangedDelegateHandle;
	FDelegateHandle MaxStaminaChangedDelegateHandle;
	FAbilitySystemReadySubscription PossessedCharacterReadySubscription;
	FLinearColor NormalStaminaFillTint = FLinearColor::White;
	float CurrentStamina = 0.0f;
	float CurrentMaxStamina = 0.0f;
	int32 GameSettingContentPreloadGeneration = 0;
	bool bHasCachedNormalStaminaFillTint = false;

	UiMotion::FTrailingValue HealthTrail;
	UiMotion::FOneShot HitFlash;
	UiMotion::FSpring LevelScale{1.f, 0.f};
	FLinearColor HealthFillBaseColor = FLinearColor::White;
	FSlateColor HealthTextBaseColor;
	float TargetHealth = 0.f;
	float DisplayedHealth = 0.f;
	float MaxHealth = 0.f;
	float LowHealthElapsed = 0.f;
	int32 CurrentLevel = INDEX_NONE;
	bool bHasHealthBaseColors = false;
	bool bLowHealthShown = false;
	bool bLevelSettling = false;
};
