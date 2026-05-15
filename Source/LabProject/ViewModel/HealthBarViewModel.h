#pragma once

#include "CoreMinimal.h"
#include "ViewModel/CommonViewModelBase.h"
#include "HealthBarViewModel.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/** 체력바 ViewModel 로그 카테고리입니다. */
DECLARE_LOG_CATEGORY_EXTERN(HealthBarViewModelLog, Log, All);

/**
 * <체력바 ViewModel>
 * - 체력 UI 데이터를 보관합니다.
 * - ASC 변경 알림을 구독합니다.
 * - MVVM 바인딩 값을 갱신합니다.
 */
UCLASS(BlueprintType)
class LABPROJECT_API UHealthBarViewModel : public UCommonViewModelBase
{
	GENERATED_BODY()

public:
	/** 체력바 ViewModel 기본 상태를 초기화합니다. */
	UHealthBarViewModel();

	// Timing hooks
	/** SourceObject로부터 ASC를 찾아 ViewModel을 초기화합니다. */
	virtual void InitializeViewModel(UObject* SourceObject) override;

	/** ASC 바인딩을 해제하고 ViewModel을 초기화합니다. */
	virtual void UninitializeViewModel() override;

private:
	// Attribute delegate callbacks
	/** 체력 변경 시 데이터를 갱신합니다. */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	/** 최대 체력 변경 시 데이터를 갱신합니다. */
	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);

public:
	/** 현재 체력입니다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	float Health = 0.f;

	/** 최대 체력입니다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	float MaxHealth = 0.f;

	/** 체력 비율입니다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	float HealthPercent = 0.f;

	/** 체력 텍스트입니다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	FText HealthText;

	/** 생존 여부입니다. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!HealthBar ViewModel")
	bool bIsAlive = false;

	/** 체력 관련 값만 갱신합니다. */
	void UpdateHealthData();

	/** 모든 표시값을 갱신합니다. */
	void UpdateAllData();

	/** MVVM에서 사용할 ViewModel 이름입니다. */
	static const FName ViewModelName;

private:
	/** SourceObject에서 ASC를 찾습니다. */
	UAbilitySystemComponent* ResolveAbilitySystemComponent(UObject* SourceObject) const;

	/** 표시값을 기본값으로 초기화합니다. */
	void ResetViewData();

	/** 현재 바인딩된 ASC입니다. */
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
};
