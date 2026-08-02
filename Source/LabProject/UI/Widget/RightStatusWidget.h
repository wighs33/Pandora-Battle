#pragma once

#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "RightStatusWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedStatUpButton, FGameplayTag, StatTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPdOnClickedStatDownButton, FGameplayTag, StatTag);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URightStatusWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "!UI|Status", meta = (Categories = "Status"))
	void BroadcastClickedStatUpButton(FGameplayTag InStatTag);

	UFUNCTION(BlueprintCallable, Category = "!UI|Status", meta = (Categories = "Status"))
	void BroadcastClickedStatDownButton(FGameplayTag InStatTag);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Status")
	FPdOnClickedStatUpButton OnClicked_StatUpButton;

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Status")
	FPdOnClickedStatDownButton OnClicked_StatDownButton;

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
	TObjectPtr<UButton> Button_Up_Armor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Recovery;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Shield;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Burn;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Freeze;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Shock;

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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Up_Critical;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Strength;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Intelligence;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Arcane;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Armor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Recovery;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Shield;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Burn;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Freeze;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Shock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_FirstPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_SecondPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_ThirdPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_MaxHealth;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_MaxMana;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_MaxStamina;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_AttackSpeed;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_MovementSpeed;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Status|Bind")
	TObjectPtr<UButton> Button_Down_Critical;

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
	void HandleMaxShieldClicked();

	UFUNCTION()
	void HandleFrostbiteClicked();

	UFUNCTION()
	void HandleBurnClicked();

	UFUNCTION()
	void HandleElectricShockClicked();

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
	void HandleCriticalClicked();

	UFUNCTION()
	void HandleStrengthDownClicked();

	UFUNCTION()
	void HandleIntelligenceDownClicked();

	UFUNCTION()
	void HandleArcaneDownClicked();

	UFUNCTION()
	void HandleArmorDownClicked();

	UFUNCTION()
	void HandleRecoveryDownClicked();

	UFUNCTION()
	void HandleMaxShieldDownClicked();

	UFUNCTION()
	void HandleFrostbiteDownClicked();

	UFUNCTION()
	void HandleBurnDownClicked();

	UFUNCTION()
	void HandleElectricShockDownClicked();

	UFUNCTION()
	void HandleFirstPandoraDownClicked();

	UFUNCTION()
	void HandleSecondPandoraDownClicked();

	UFUNCTION()
	void HandleThirdPandoraDownClicked();

	UFUNCTION()
	void HandleMaxHealthDownClicked();

	UFUNCTION()
	void HandleMaxManaDownClicked();

	UFUNCTION()
	void HandleMaxStaminaDownClicked();

	UFUNCTION()
	void HandleAttackSpeedDownClicked();

	UFUNCTION()
	void HandleMovementSpeedDownClicked();

	UFUNCTION()
	void HandleCriticalDownClicked();

	void HandleStatUpButtonClicked(FGameplayTag InStatTag, const TCHAR* StatTagPropertyName, const UButton* SourceButton);
	void HandleStatDownButtonClicked(FGameplayTag InStatTag, const TCHAR* StatTagPropertyName, const UButton* SourceButton);
	void ValidateConfiguredStatTags() const;
	void BindButtonCallbacks();
	void UnbindButtonCallbacks();
	FGameplayTag GetStrengthStatTag() const;
	FGameplayTag GetIntelligenceStatTag() const;
	FGameplayTag GetArcaneStatTag() const;
	FGameplayTag GetArmorStatTag() const;
	FGameplayTag GetRecoveryStatTag() const;
	FGameplayTag GetMaxShieldStatTag() const;
	FGameplayTag GetFrostbiteStatTag() const;
	FGameplayTag GetBurnStatTag() const;
	FGameplayTag GetElectricShockStatTag() const;
	FGameplayTag GetFirstPandoraStatTag() const;
	FGameplayTag GetSecondPandoraStatTag() const;
	FGameplayTag GetThirdPandoraStatTag() const;
	FGameplayTag GetMaxHealthStatTag() const;
	FGameplayTag GetMaxManaStatTag() const;
	FGameplayTag GetMaxStaminaStatTag() const;
	FGameplayTag GetAttackSpeedStatTag() const;
	FGameplayTag GetMovementSpeedStatTag() const;
	FGameplayTag GetCriticalStatTag() const;
	UButton* GetCriticalUpButton() const;
	UButton* GetCriticalDownButton() const;
};
