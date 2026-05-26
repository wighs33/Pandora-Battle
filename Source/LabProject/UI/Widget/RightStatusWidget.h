#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "RightStatusWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedStatUpButton, FGameplayTag, StatTag);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URightStatusWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|Status", meta = (Categories = "Status"))
	void BroadcastClickedStatUpButton(FGameplayTag InStatTag);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Status")
	FPdOnClickedStatUpButton OnClicked_StatUpButton;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Strength;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Intelligence;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Arcane;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Toughness;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Recovery;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_MagicResistance;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Immunity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Fortitude;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Sanity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_FirstPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_SecondPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_ThirdPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_MaxHealth;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_MaxMana;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_MaxStamina;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_AttackSpeed;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_MovementSpeed;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_CriticalChance;

private:
	UFUNCTION()
	void HandleStrengthClicked();

	UFUNCTION()
	void HandleIntelligenceClicked();

	UFUNCTION()
	void HandleArcaneClicked();

	UFUNCTION()
	void HandleArmorClicked();

	UFUNCTION()
	void HandleRecoveryClicked();

	UFUNCTION()
	void HandleMagicResistanceClicked();

	UFUNCTION()
	void HandleImmunityClicked();

	UFUNCTION()
	void HandleFortitudeClicked();

	UFUNCTION()
	void HandleSanityClicked();

	UFUNCTION()
	void HandleFirstPandoraClicked();

	UFUNCTION()
	void HandleSecondPandoraClicked();

	UFUNCTION()
	void HandleThirdPandoraClicked();

	UFUNCTION()
	void HandleMaxHealthClicked();

	UFUNCTION()
	void HandleMaxManaClicked();

	UFUNCTION()
	void HandleMaxStaminaClicked();

	UFUNCTION()
	void HandleAttackSpeedClicked();

	UFUNCTION()
	void HandleMovementSpeedClicked();

	UFUNCTION()
	void HandleCriticalChanceClicked();

	void HandleStatUpButtonClicked(FGameplayTag InStatTag, const TCHAR* StatTagPropertyName, const UButton* SourceButton);
	void ValidateConfiguredStatTags() const;
	void BindButtonCallbacks();
	void UnbindButtonCallbacks();
	FGameplayTag GetStrengthStatTag() const;
	FGameplayTag GetIntelligenceStatTag() const;
	FGameplayTag GetArcaneStatTag() const;
	FGameplayTag GetArmorStatTag() const;
	FGameplayTag GetRecoveryStatTag() const;
	FGameplayTag GetMagicResistanceStatTag() const;
	FGameplayTag GetImmunityStatTag() const;
	FGameplayTag GetFortitudeStatTag() const;
	FGameplayTag GetSanityStatTag() const;
	FGameplayTag GetFirstPandoraStatTag() const;
	FGameplayTag GetSecondPandoraStatTag() const;
	FGameplayTag GetThirdPandoraStatTag() const;
	FGameplayTag GetMaxHealthStatTag() const;
	FGameplayTag GetMaxManaStatTag() const;
	FGameplayTag GetMaxStaminaStatTag() const;
	FGameplayTag GetAttackSpeedStatTag() const;
	FGameplayTag GetMovementSpeedStatTag() const;
	FGameplayTag GetCriticalChanceStatTag() const;
};
