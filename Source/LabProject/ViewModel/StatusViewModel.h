#pragma once

#include "CoreMinimal.h"
#include "ViewModel/CommonViewModelBase.h"
#include "StatusViewModel.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

/** 상태창 ViewModel 로그 카테고리입니다. */
DECLARE_LOG_CATEGORY_EXTERN(StatusViewModelLog, Log, All);

/**
 * <상태창 ViewModel>
 * - 상태창 수치를 보관합니다.
 * - ASC 변경 알림을 구독합니다.
 * - MVVM 바인딩 값을 갱신합니다.
 */
UCLASS(BlueprintType)
class LABPROJECT_API UStatusViewModel : public UCommonViewModelBase
{
	GENERATED_BODY()

public:
	/** 상태창 ViewModel 기본 상태를 초기화합니다. */
	UStatusViewModel();

	/** SourceObject로부터 ASC를 찾아 초기화합니다. */
	virtual void InitializeViewModel(UObject* SourceObject) override;

	/** ASC 바인딩을 해제합니다. */
	virtual void UninitializeViewModel() override;

	/** 힘 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float Strength = 10.f;

	/** 지능 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float Intelligence = 10.f;

	/** 신비 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float Arcane = 10.f;

	/** 강인함 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float Toughness = 2.f;

	/** 회복 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float Recovery = 2.f;

	/** 마법 저항 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float MagicResistance = 2.f;

	/** 면역 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resistance")
	float Immunity = 0.f;

	/** 인내 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resistance")
	float Fortitude = 0.f;

	/** 정신 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resistance")
	float Sanity = 0.f;

	/** 첫 번째 판도라 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|PandoraForce")
	float FirstPandora = 1.f;

	/** 두 번째 판도라 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|PandoraForce")
	float SecondPandora = 1.f;

	/** 세 번째 판도라 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|PandoraForce")
	float ThirdPandora = 1.f;

	/** 공격 속도 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Agility")
	float AttackSpeed = 0.f;

	/** 이동 속도 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Agility")
	float MovementSpeed = 0.f;

	/** 치명타 확률 수치입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Agility")
	float CriticalChance = 0.f;

	/** 현재 체력입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Health")
	float Health = 100.f;

	/** 최대 체력입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Health")
	float MaxHealth = 100.f;

	/** 체력 비율입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Health")
	float HealthPercent = 1.f;

	/** 현재 마나입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Mana")
	float Mana = 100.f;

	/** 최대 마나입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Mana")
	float MaxMana = 100.f;

	/** 마나 비율입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Mana")
	float ManaPercent = 1.f;

	/** 현재 스태미나입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stamina")
	float Stamina = 100.f;

	/** 최대 스태미나입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stamina")
	float MaxStamina = 100.f;

	/** 스태미나 비율입니다. */
	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stamina")
	float StaminaPercent = 1.f;

	/** 공격 관련 값을 갱신합니다. */
	void UpdateOffenseData();

	/** 방어 관련 값을 갱신합니다. */
	void UpdateDefenseData();

	/** 저항 관련 값을 갱신합니다. */
	void UpdateResistanceData();

	/** 판도라 관련 값을 갱신합니다. */
	void UpdatePandoraForceData();

	/** 민첩 관련 값을 갱신합니다. */
	void UpdateAgilityData();

	/** 체력 관련 값을 갱신합니다. */
	void UpdateHealthData();

	/** 마나 관련 값을 갱신합니다. */
	void UpdateManaData();

	/** 스태미나 관련 값을 갱신합니다. */
	void UpdateStaminaData();

	/** 모든 값을 갱신합니다. */
	void UpdateAllData();

public:
	/** MVVM에서 사용할 ViewModel 이름입니다. */
	static const FName ViewModelName;

protected:
	/** 현재 바인딩된 ASC입니다. */
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

private:
	/** 공격 수치 변경을 처리합니다. */
	void OnOffenseChanged(const FOnAttributeChangeData& Data);

	/** 방어 수치 변경을 처리합니다. */
	void OnDefenseChanged(const FOnAttributeChangeData& Data);

	/** 저항 수치 변경을 처리합니다. */
	void OnResistanceChanged(const FOnAttributeChangeData& Data);

	/** 판도라 수치 변경을 처리합니다. */
	void OnPandoraForceChanged(const FOnAttributeChangeData& Data);

	/** 민첩 수치 변경을 처리합니다. */
	void OnAgilityChanged(const FOnAttributeChangeData& Data);

	/** 체력 변경을 처리합니다. */
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	/** 최대 체력 변경을 처리합니다. */
	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);

	/** 마나 변경을 처리합니다. */
	void OnManaChanged(const FOnAttributeChangeData& Data);

	/** 최대 마나 변경을 처리합니다. */
	void OnMaxManaChanged(const FOnAttributeChangeData& Data);

	/** 스태미나 변경을 처리합니다. */
	void OnStaminaChanged(const FOnAttributeChangeData& Data);

	/** 최대 스태미나 변경을 처리합니다. */
	void OnMaxStaminaChanged(const FOnAttributeChangeData& Data);
};