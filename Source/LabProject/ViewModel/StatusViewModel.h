// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ViewModel/CommonViewModelBase.h"
#include "StatusViewModel.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

DECLARE_LOG_CATEGORY_EXTERN(StatusViewModelLog, Log, All);

UCLASS(BlueprintType)
class LABPROJECT_API UStatusViewModel : public UCommonViewModelBase
{
	GENERATED_BODY()

public:
	UStatusViewModel();

	virtual void InitializeViewModel(UObject* SourceObject) override;
	virtual void UninitializeViewModel() override;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float Strength = 10.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float Intelligence = 10.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Offense")
	float Arcane = 10.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float Toughness = 2.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float Recovery = 2.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Defense")
	float MagicResistance = 2.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resistance")
	float Immunity = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resistance")
	float Fortitude = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Resistance")
	float Sanity = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|PandoraForce")
	float FirstPandora = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|PandoraForce")
	float SecondPandora = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|PandoraForce")
	float ThirdPandora = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Agility")
	float AttackSpeed = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Agility")
	float MovementSpeed = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Agility")
	float CriticalChance = 0.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Health")
	float Health = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Health")
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Health")
	float HealthPercent = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Mana")
	float Mana = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Mana")
	float MaxMana = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Mana")
	float ManaPercent = 1.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stamina")
	float Stamina = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stamina")
	float MaxStamina = 100.f;

	UPROPERTY(BlueprintReadWrite, FieldNotify, Category = "!Status ViewModel|Stamina")
	float StaminaPercent = 1.f;

	void UpdateOffenseData();
	void UpdateDefenseData();
	void UpdateResistanceData();
	void UpdatePandoraForceData();
	void UpdateAgilityData();
	void UpdateHealthData();
	void UpdateManaData();
	void UpdateStaminaData();
	void UpdateAllData();

public:
	static const FName ViewModelName;

protected:
	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

private:
	void OnOffenseChanged(const FOnAttributeChangeData& Data);
	void OnDefenseChanged(const FOnAttributeChangeData& Data);
	void OnResistanceChanged(const FOnAttributeChangeData& Data);
	void OnPandoraForceChanged(const FOnAttributeChangeData& Data);
	void OnAgilityChanged(const FOnAttributeChangeData& Data);
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const FOnAttributeChangeData& Data);
	void OnManaChanged(const FOnAttributeChangeData& Data);
	void OnMaxManaChanged(const FOnAttributeChangeData& Data);
	void OnStaminaChanged(const FOnAttributeChangeData& Data);
	void OnMaxStaminaChanged(const FOnAttributeChangeData& Data);
};
