#include "UI/Widget/RightStatusWidget.h"

#include "Definition/Common/ProjectTagConfig.h"
#include "Components/Button.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightStatusWidget)

URightStatusWidget::URightStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URightStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const FGameplayTag StrengthStatTag = GetStrengthStatTag();
	const FGameplayTag IntelligenceStatTag = GetIntelligenceStatTag();
	const FGameplayTag ArcaneStatTag = GetArcaneStatTag();
	const FGameplayTag ArmorStatTag = GetArmorStatTag();
	const FGameplayTag RecoveryStatTag = GetRecoveryStatTag();
	const FGameplayTag MaxShieldStatTag = GetMaxShieldStatTag();
	const FGameplayTag FrostbiteStatTag = GetFrostbiteStatTag();
	const FGameplayTag BurnStatTag = GetBurnStatTag();
	const FGameplayTag ElectricShockStatTag = GetElectricShockStatTag();
	const FGameplayTag FirstPandoraStatTag = GetFirstPandoraStatTag();
	const FGameplayTag SecondPandoraStatTag = GetSecondPandoraStatTag();
	const FGameplayTag ThirdPandoraStatTag = GetThirdPandoraStatTag();
	const FGameplayTag MaxHealthStatTag = GetMaxHealthStatTag();
	const FGameplayTag MaxManaStatTag = GetMaxManaStatTag();
	const FGameplayTag MaxStaminaStatTag = GetMaxStaminaStatTag();
	const FGameplayTag AttackSpeedStatTag = GetAttackSpeedStatTag();
	const FGameplayTag MovementSpeedStatTag = GetMovementSpeedStatTag();
	const FGameplayTag CriticalStatTag = GetCriticalStatTag();



	ValidateConfiguredStatTags();
	BindButtonCallbacks();
}

void URightStatusWidget::NativeDestruct()
{
	UnbindButtonCallbacks();

	Super::NativeDestruct();
}

void URightStatusWidget::BroadcastClickedStatUpButton(FGameplayTag InStatTag)
{

	if (!InStatTag.IsValid())
	{

		return;
	}

	OnClicked_StatUpButton.Broadcast(InStatTag);
}

void URightStatusWidget::BroadcastClickedStatDownButton(FGameplayTag InStatTag)
{

	if (!InStatTag.IsValid())
	{

		return;
	}

	OnClicked_StatDownButton.Broadcast(InStatTag);
}

void URightStatusWidget::HandleStrengthClicked()
{
	HandleStatUpButtonClicked(GetStrengthStatTag(), TEXT("StrengthStatTag"), Button_Up_Strength);
}

void URightStatusWidget::HandleIntelligenceClicked()
{
	HandleStatUpButtonClicked(GetIntelligenceStatTag(), TEXT("IntelligenceStatTag"), Button_Up_Intelligence);
}

void URightStatusWidget::HandleArcaneClicked()
{
	HandleStatUpButtonClicked(GetArcaneStatTag(), TEXT("ArcaneStatTag"), Button_Up_Arcane);
}

void URightStatusWidget::HandleArmorClicked()
{
	HandleStatUpButtonClicked(GetArmorStatTag(), TEXT("ArmorStatTag"), Button_Up_Armor);
}

void URightStatusWidget::HandleRecoveryClicked()
{
	HandleStatUpButtonClicked(GetRecoveryStatTag(), TEXT("RecoveryStatTag"), Button_Up_Recovery);
}

void URightStatusWidget::HandleMaxShieldClicked()
{
	HandleStatUpButtonClicked(GetMaxShieldStatTag(), TEXT("MaxShieldStatTag"), Button_Up_Shield);
}

void URightStatusWidget::HandleFrostbiteClicked()
{
	HandleStatUpButtonClicked(GetFrostbiteStatTag(), TEXT("FrostbiteStatTag"), Button_Up_Freeze);
}

void URightStatusWidget::HandleBurnClicked()
{
	HandleStatUpButtonClicked(GetBurnStatTag(), TEXT("BurnStatTag"), Button_Up_Burn);
}

void URightStatusWidget::HandleElectricShockClicked()
{
	HandleStatUpButtonClicked(GetElectricShockStatTag(), TEXT("ElectricShockStatTag"), Button_Up_Shock);
}

void URightStatusWidget::HandleFirstPandoraClicked()
{
	HandleStatUpButtonClicked(GetFirstPandoraStatTag(), TEXT("FirstPandoraStatTag"), Button_Up_FirstPandora);
}

void URightStatusWidget::HandleSecondPandoraClicked()
{
	HandleStatUpButtonClicked(GetSecondPandoraStatTag(), TEXT("SecondPandoraStatTag"), Button_Up_SecondPandora);
}

void URightStatusWidget::HandleThirdPandoraClicked()
{
	HandleStatUpButtonClicked(GetThirdPandoraStatTag(), TEXT("ThirdPandoraStatTag"), Button_Up_ThirdPandora);
}

void URightStatusWidget::HandleMaxHealthClicked()
{
	HandleStatUpButtonClicked(GetMaxHealthStatTag(), TEXT("MaxHealthStatTag"), Button_Up_MaxHealth);
}

void URightStatusWidget::HandleMaxManaClicked()
{
	HandleStatUpButtonClicked(GetMaxManaStatTag(), TEXT("MaxManaStatTag"), Button_Up_MaxMana);
}

void URightStatusWidget::HandleMaxStaminaClicked()
{
	HandleStatUpButtonClicked(GetMaxStaminaStatTag(), TEXT("MaxStaminaStatTag"), Button_Up_MaxStamina);
}

void URightStatusWidget::HandleAttackSpeedClicked()
{
	HandleStatUpButtonClicked(GetAttackSpeedStatTag(), TEXT("AttackSpeedStatTag"), Button_Up_AttackSpeed);
}

void URightStatusWidget::HandleMovementSpeedClicked()
{
	HandleStatUpButtonClicked(GetMovementSpeedStatTag(), TEXT("MovementSpeedStatTag"), Button_Up_MovementSpeed);
}

void URightStatusWidget::HandleCriticalClicked()
{
	HandleStatUpButtonClicked(GetCriticalStatTag(), TEXT("CriticalStatTag"), GetCriticalUpButton());
}

void URightStatusWidget::HandleStrengthDownClicked()
{
	HandleStatDownButtonClicked(GetStrengthStatTag(), TEXT("StrengthStatTag"), Button_Down_Strength);
}

void URightStatusWidget::HandleIntelligenceDownClicked()
{
	HandleStatDownButtonClicked(GetIntelligenceStatTag(), TEXT("IntelligenceStatTag"), Button_Down_Intelligence);
}

void URightStatusWidget::HandleArcaneDownClicked()
{
	HandleStatDownButtonClicked(GetArcaneStatTag(), TEXT("ArcaneStatTag"), Button_Down_Arcane);
}

void URightStatusWidget::HandleArmorDownClicked()
{
	HandleStatDownButtonClicked(GetArmorStatTag(), TEXT("ArmorStatTag"), Button_Down_Armor);
}

void URightStatusWidget::HandleRecoveryDownClicked()
{
	HandleStatDownButtonClicked(GetRecoveryStatTag(), TEXT("RecoveryStatTag"), Button_Down_Recovery);
}

void URightStatusWidget::HandleMaxShieldDownClicked()
{
	HandleStatDownButtonClicked(GetMaxShieldStatTag(), TEXT("MaxShieldStatTag"), Button_Down_Shield);
}

void URightStatusWidget::HandleFrostbiteDownClicked()
{
	HandleStatDownButtonClicked(GetFrostbiteStatTag(), TEXT("FrostbiteStatTag"), Button_Down_Freeze);
}

void URightStatusWidget::HandleBurnDownClicked()
{
	HandleStatDownButtonClicked(GetBurnStatTag(), TEXT("BurnStatTag"), Button_Down_Burn);
}

void URightStatusWidget::HandleElectricShockDownClicked()
{
	HandleStatDownButtonClicked(GetElectricShockStatTag(), TEXT("ElectricShockStatTag"), Button_Down_Shock);
}

void URightStatusWidget::HandleFirstPandoraDownClicked()
{
	HandleStatDownButtonClicked(GetFirstPandoraStatTag(), TEXT("FirstPandoraStatTag"), Button_Down_FirstPandora);
}

void URightStatusWidget::HandleSecondPandoraDownClicked()
{
	HandleStatDownButtonClicked(GetSecondPandoraStatTag(), TEXT("SecondPandoraStatTag"), Button_Down_SecondPandora);
}

void URightStatusWidget::HandleThirdPandoraDownClicked()
{
	HandleStatDownButtonClicked(GetThirdPandoraStatTag(), TEXT("ThirdPandoraStatTag"), Button_Down_ThirdPandora);
}

void URightStatusWidget::HandleMaxHealthDownClicked()
{
	HandleStatDownButtonClicked(GetMaxHealthStatTag(), TEXT("MaxHealthStatTag"), Button_Down_MaxHealth);
}

void URightStatusWidget::HandleMaxManaDownClicked()
{
	HandleStatDownButtonClicked(GetMaxManaStatTag(), TEXT("MaxManaStatTag"), Button_Down_MaxMana);
}

void URightStatusWidget::HandleMaxStaminaDownClicked()
{
	HandleStatDownButtonClicked(GetMaxStaminaStatTag(), TEXT("MaxStaminaStatTag"), Button_Down_MaxStamina);
}

void URightStatusWidget::HandleAttackSpeedDownClicked()
{
	HandleStatDownButtonClicked(GetAttackSpeedStatTag(), TEXT("AttackSpeedStatTag"), Button_Down_AttackSpeed);
}

void URightStatusWidget::HandleMovementSpeedDownClicked()
{
	HandleStatDownButtonClicked(GetMovementSpeedStatTag(), TEXT("MovementSpeedStatTag"), Button_Down_MovementSpeed);
}

void URightStatusWidget::HandleCriticalDownClicked()
{
	HandleStatDownButtonClicked(GetCriticalStatTag(), TEXT("CriticalStatTag"), GetCriticalDownButton());
}

void URightStatusWidget::HandleStatUpButtonClicked(FGameplayTag InStatTag, const TCHAR* StatTagPropertyName, const UButton* SourceButton)
{


	if (!InStatTag.IsValid())
	{

		return;
	}

	BroadcastClickedStatUpButton(InStatTag);
}

void URightStatusWidget::HandleStatDownButtonClicked(FGameplayTag InStatTag, const TCHAR* StatTagPropertyName, const UButton* SourceButton)
{


	if (!InStatTag.IsValid())
	{

		return;
	}

	BroadcastClickedStatDownButton(InStatTag);
}

void URightStatusWidget::ValidateConfiguredStatTags() const
{
	const auto ValidateTag = [this](const TCHAR* StatTagPropertyName, const FGameplayTag& InStatTag, const UButton* SourceButton)
	{
		if (InStatTag.IsValid())
		{
			return;
		}


	};

	ValidateTag(TEXT("StrengthStatTag"), GetStrengthStatTag(), Button_Up_Strength);
	ValidateTag(TEXT("IntelligenceStatTag"), GetIntelligenceStatTag(), Button_Up_Intelligence);
	ValidateTag(TEXT("ArcaneStatTag"), GetArcaneStatTag(), Button_Up_Arcane);
	ValidateTag(TEXT("ArmorStatTag"), GetArmorStatTag(), Button_Up_Armor);
	ValidateTag(TEXT("RecoveryStatTag"), GetRecoveryStatTag(), Button_Up_Recovery);
	ValidateTag(TEXT("MaxShieldStatTag"), GetMaxShieldStatTag(), Button_Up_Shield);
	ValidateTag(TEXT("BurnStatTag"), GetBurnStatTag(), Button_Up_Burn);
	ValidateTag(TEXT("FrostbiteStatTag"), GetFrostbiteStatTag(), Button_Up_Freeze);
	ValidateTag(TEXT("ElectricShockStatTag"), GetElectricShockStatTag(), Button_Up_Shock);
	ValidateTag(TEXT("FirstPandoraStatTag"), GetFirstPandoraStatTag(), Button_Up_FirstPandora);
	ValidateTag(TEXT("SecondPandoraStatTag"), GetSecondPandoraStatTag(), Button_Up_SecondPandora);
	ValidateTag(TEXT("ThirdPandoraStatTag"), GetThirdPandoraStatTag(), Button_Up_ThirdPandora);
	ValidateTag(TEXT("MaxHealthStatTag"), GetMaxHealthStatTag(), Button_Up_MaxHealth);
	ValidateTag(TEXT("MaxManaStatTag"), GetMaxManaStatTag(), Button_Up_MaxMana);
	ValidateTag(TEXT("MaxStaminaStatTag"), GetMaxStaminaStatTag(), Button_Up_MaxStamina);
	ValidateTag(TEXT("AttackSpeedStatTag"), GetAttackSpeedStatTag(), Button_Up_AttackSpeed);
	ValidateTag(TEXT("MovementSpeedStatTag"), GetMovementSpeedStatTag(), Button_Up_MovementSpeed);
	ValidateTag(TEXT("CriticalStatTag"), GetCriticalStatTag(), GetCriticalUpButton());

	ValidateTag(TEXT("StrengthStatTag"), GetStrengthStatTag(), Button_Down_Strength);
	ValidateTag(TEXT("IntelligenceStatTag"), GetIntelligenceStatTag(), Button_Down_Intelligence);
	ValidateTag(TEXT("ArcaneStatTag"), GetArcaneStatTag(), Button_Down_Arcane);
	ValidateTag(TEXT("ArmorStatTag"), GetArmorStatTag(), Button_Down_Armor);
	ValidateTag(TEXT("RecoveryStatTag"), GetRecoveryStatTag(), Button_Down_Recovery);
	ValidateTag(TEXT("MaxShieldStatTag"), GetMaxShieldStatTag(), Button_Down_Shield);
	ValidateTag(TEXT("BurnStatTag"), GetBurnStatTag(), Button_Down_Burn);
	ValidateTag(TEXT("FrostbiteStatTag"), GetFrostbiteStatTag(), Button_Down_Freeze);
	ValidateTag(TEXT("ElectricShockStatTag"), GetElectricShockStatTag(), Button_Down_Shock);
	ValidateTag(TEXT("FirstPandoraStatTag"), GetFirstPandoraStatTag(), Button_Down_FirstPandora);
	ValidateTag(TEXT("SecondPandoraStatTag"), GetSecondPandoraStatTag(), Button_Down_SecondPandora);
	ValidateTag(TEXT("ThirdPandoraStatTag"), GetThirdPandoraStatTag(), Button_Down_ThirdPandora);
	ValidateTag(TEXT("MaxHealthStatTag"), GetMaxHealthStatTag(), Button_Down_MaxHealth);
	ValidateTag(TEXT("MaxManaStatTag"), GetMaxManaStatTag(), Button_Down_MaxMana);
	ValidateTag(TEXT("MaxStaminaStatTag"), GetMaxStaminaStatTag(), Button_Down_MaxStamina);
	ValidateTag(TEXT("AttackSpeedStatTag"), GetAttackSpeedStatTag(), Button_Down_AttackSpeed);
	ValidateTag(TEXT("MovementSpeedStatTag"), GetMovementSpeedStatTag(), Button_Down_MovementSpeed);
	ValidateTag(TEXT("CriticalStatTag"), GetCriticalStatTag(), GetCriticalDownButton());
}

void URightStatusWidget::BindButtonCallbacks()
{
	Button_Up_Strength->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStrengthClicked);
	Button_Up_Intelligence->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleIntelligenceClicked);
	Button_Up_Arcane->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleArcaneClicked);
	Button_Up_Armor->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleArmorClicked);
	Button_Up_Recovery->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRecoveryClicked);
	Button_Up_Shield->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxShieldClicked);
	Button_Up_Burn->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBurnClicked);
	Button_Up_Freeze->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFrostbiteClicked);
	Button_Up_Shock->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleElectricShockClicked);
	Button_Up_FirstPandora->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFirstPandoraClicked);
	Button_Up_SecondPandora->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSecondPandoraClicked);
	Button_Up_ThirdPandora->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleThirdPandoraClicked);
	Button_Up_MaxHealth->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxHealthClicked);
	Button_Up_MaxMana->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxManaClicked);
	Button_Up_MaxStamina->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxStaminaClicked);
	Button_Up_AttackSpeed->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAttackSpeedClicked);
	Button_Up_MovementSpeed->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMovementSpeedClicked);
	if (Button_Up_Critical)
	{
		Button_Up_Critical->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCriticalClicked);
	}

	Button_Down_Strength->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStrengthDownClicked);
	Button_Down_Intelligence->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleIntelligenceDownClicked);
	Button_Down_Arcane->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleArcaneDownClicked);
	Button_Down_Armor->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleArmorDownClicked);
	Button_Down_Recovery->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRecoveryDownClicked);
	Button_Down_Shield->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxShieldDownClicked);
	Button_Down_Burn->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBurnDownClicked);
	Button_Down_Freeze->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFrostbiteDownClicked);
	Button_Down_Shock->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleElectricShockDownClicked);
	Button_Down_FirstPandora->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFirstPandoraDownClicked);
	Button_Down_SecondPandora->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSecondPandoraDownClicked);
	Button_Down_ThirdPandora->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleThirdPandoraDownClicked);
	Button_Down_MaxHealth->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxHealthDownClicked);
	Button_Down_MaxMana->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxManaDownClicked);
	Button_Down_MaxStamina->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxStaminaDownClicked);
	Button_Down_AttackSpeed->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAttackSpeedDownClicked);
	Button_Down_MovementSpeed->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMovementSpeedDownClicked);
	if (Button_Down_Critical)
	{
		Button_Down_Critical->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCriticalDownClicked);
	}
}

void URightStatusWidget::UnbindButtonCallbacks()
{
	Button_Up_Strength->OnClicked.RemoveDynamic(this, &ThisClass::HandleStrengthClicked);
	Button_Up_Intelligence->OnClicked.RemoveDynamic(this, &ThisClass::HandleIntelligenceClicked);
	Button_Up_Arcane->OnClicked.RemoveDynamic(this, &ThisClass::HandleArcaneClicked);
	Button_Up_Armor->OnClicked.RemoveDynamic(this, &ThisClass::HandleArmorClicked);
	Button_Up_Recovery->OnClicked.RemoveDynamic(this, &ThisClass::HandleRecoveryClicked);
	Button_Up_Shield->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxShieldClicked);
	Button_Up_Burn->OnClicked.RemoveDynamic(this, &ThisClass::HandleBurnClicked);
	Button_Up_Freeze->OnClicked.RemoveDynamic(this, &ThisClass::HandleFrostbiteClicked);
	Button_Up_Shock->OnClicked.RemoveDynamic(this, &ThisClass::HandleElectricShockClicked);
	Button_Up_FirstPandora->OnClicked.RemoveDynamic(this, &ThisClass::HandleFirstPandoraClicked);
	Button_Up_SecondPandora->OnClicked.RemoveDynamic(this, &ThisClass::HandleSecondPandoraClicked);
	Button_Up_ThirdPandora->OnClicked.RemoveDynamic(this, &ThisClass::HandleThirdPandoraClicked);
	Button_Up_MaxHealth->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxHealthClicked);
	Button_Up_MaxMana->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxManaClicked);
	Button_Up_MaxStamina->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxStaminaClicked);
	Button_Up_AttackSpeed->OnClicked.RemoveDynamic(this, &ThisClass::HandleAttackSpeedClicked);
	Button_Up_MovementSpeed->OnClicked.RemoveDynamic(this, &ThisClass::HandleMovementSpeedClicked);
	if (Button_Up_Critical)
	{
		Button_Up_Critical->OnClicked.RemoveDynamic(this, &ThisClass::HandleCriticalClicked);
	}

	Button_Down_Strength->OnClicked.RemoveDynamic(this, &ThisClass::HandleStrengthDownClicked);
	Button_Down_Intelligence->OnClicked.RemoveDynamic(this, &ThisClass::HandleIntelligenceDownClicked);
	Button_Down_Arcane->OnClicked.RemoveDynamic(this, &ThisClass::HandleArcaneDownClicked);
	Button_Down_Armor->OnClicked.RemoveDynamic(this, &ThisClass::HandleArmorDownClicked);
	Button_Down_Recovery->OnClicked.RemoveDynamic(this, &ThisClass::HandleRecoveryDownClicked);
	Button_Down_Shield->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxShieldDownClicked);
	Button_Down_Burn->OnClicked.RemoveDynamic(this, &ThisClass::HandleBurnDownClicked);
	Button_Down_Freeze->OnClicked.RemoveDynamic(this, &ThisClass::HandleFrostbiteDownClicked);
	Button_Down_Shock->OnClicked.RemoveDynamic(this, &ThisClass::HandleElectricShockDownClicked);
	Button_Down_FirstPandora->OnClicked.RemoveDynamic(this, &ThisClass::HandleFirstPandoraDownClicked);
	Button_Down_SecondPandora->OnClicked.RemoveDynamic(this, &ThisClass::HandleSecondPandoraDownClicked);
	Button_Down_ThirdPandora->OnClicked.RemoveDynamic(this, &ThisClass::HandleThirdPandoraDownClicked);
	Button_Down_MaxHealth->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxHealthDownClicked);
	Button_Down_MaxMana->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxManaDownClicked);
	Button_Down_MaxStamina->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxStaminaDownClicked);
	Button_Down_AttackSpeed->OnClicked.RemoveDynamic(this, &ThisClass::HandleAttackSpeedDownClicked);
	Button_Down_MovementSpeed->OnClicked.RemoveDynamic(this, &ThisClass::HandleMovementSpeedDownClicked);
	if (Button_Down_Critical)
	{
		Button_Down_Critical->OnClicked.RemoveDynamic(this, &ThisClass::HandleCriticalDownClicked);
	}
}

FGameplayTag URightStatusWidget::GetStrengthStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusStrengthTag();
}

FGameplayTag URightStatusWidget::GetIntelligenceStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusIntelligenceTag();
}

FGameplayTag URightStatusWidget::GetArcaneStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusArcaneTag();
}

FGameplayTag URightStatusWidget::GetArmorStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusArmorTag();
}

FGameplayTag URightStatusWidget::GetRecoveryStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusRecoveryTag();
}

FGameplayTag URightStatusWidget::GetMaxShieldStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusMaxShieldTag();
}

FGameplayTag URightStatusWidget::GetFrostbiteStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusFrostbiteTag();
}

FGameplayTag URightStatusWidget::GetBurnStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusBurnTag();
}

FGameplayTag URightStatusWidget::GetElectricShockStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusElectricShockTag();
}

FGameplayTag URightStatusWidget::GetFirstPandoraStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusFirstPandoraTag();
}

FGameplayTag URightStatusWidget::GetSecondPandoraStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusSecondPandoraTag();
}

FGameplayTag URightStatusWidget::GetThirdPandoraStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusThirdPandoraTag();
}

FGameplayTag URightStatusWidget::GetMaxHealthStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusMaxHealthTag();
}

FGameplayTag URightStatusWidget::GetMaxManaStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusMaxManaTag();
}

FGameplayTag URightStatusWidget::GetMaxStaminaStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusMaxStaminaTag();
}

FGameplayTag URightStatusWidget::GetAttackSpeedStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusAttackSpeedTag();
}

FGameplayTag URightStatusWidget::GetMovementSpeedStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusMovementSpeedTag();
}

FGameplayTag URightStatusWidget::GetCriticalStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusCriticalTag();
}

UButton* URightStatusWidget::GetCriticalUpButton() const
{
	return Button_Up_Critical.Get();
}

UButton* URightStatusWidget::GetCriticalDownButton() const
{
	return Button_Down_Critical.Get();
}
