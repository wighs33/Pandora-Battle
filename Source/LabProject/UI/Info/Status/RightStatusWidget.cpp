#include "UI/Info/Status/RightStatusWidget.h"

#include "Definition/Common/ProjectTagDefinition.h"
#include "Components/Button.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RightStatusWidget)

DEFINE_LOG_CATEGORY_STATIC(LogRightStatusWidget, Log, All);

URightStatusWidget::URightStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URightStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

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
	HandleStatUpButtonClicked(GetStrengthStatTag());
}

void URightStatusWidget::HandleIntelligenceClicked()
{
	HandleStatUpButtonClicked(GetIntelligenceStatTag());
}

void URightStatusWidget::HandleArcaneClicked()
{
	HandleStatUpButtonClicked(GetArcaneStatTag());
}

void URightStatusWidget::HandleArmorClicked()
{
	HandleStatUpButtonClicked(GetArmorStatTag());
}

void URightStatusWidget::HandleRecoveryClicked()
{
	HandleStatUpButtonClicked(GetRecoveryStatTag());
}

void URightStatusWidget::HandleMaxShieldClicked()
{
	HandleStatUpButtonClicked(GetMaxShieldStatTag());
}

void URightStatusWidget::HandleFrostbiteClicked()
{
	HandleStatUpButtonClicked(GetFrostbiteStatTag());
}

void URightStatusWidget::HandleBurnClicked()
{
	HandleStatUpButtonClicked(GetBurnStatTag());
}

void URightStatusWidget::HandleElectricShockClicked()
{
	HandleStatUpButtonClicked(GetElectricShockStatTag());
}

void URightStatusWidget::HandleFirstPandoraClicked()
{
	HandleStatUpButtonClicked(GetFirstPandoraStatTag());
}

void URightStatusWidget::HandleSecondPandoraClicked()
{
	HandleStatUpButtonClicked(GetSecondPandoraStatTag());
}

void URightStatusWidget::HandleThirdPandoraClicked()
{
	HandleStatUpButtonClicked(GetThirdPandoraStatTag());
}

void URightStatusWidget::HandleMaxHealthClicked()
{
	HandleStatUpButtonClicked(GetMaxHealthStatTag());
}

void URightStatusWidget::HandleMaxManaClicked()
{
	HandleStatUpButtonClicked(GetMaxManaStatTag());
}

void URightStatusWidget::HandleMaxStaminaClicked()
{
	HandleStatUpButtonClicked(GetMaxStaminaStatTag());
}

void URightStatusWidget::HandleAttackSpeedClicked()
{
	HandleStatUpButtonClicked(GetAttackSpeedStatTag());
}

void URightStatusWidget::HandleMovementSpeedClicked()
{
	HandleStatUpButtonClicked(GetMovementSpeedStatTag());
}

void URightStatusWidget::HandleCriticalClicked()
{
	HandleStatUpButtonClicked(GetCriticalStatTag());
}

void URightStatusWidget::HandleStrengthDownClicked()
{
	HandleStatDownButtonClicked(GetStrengthStatTag());
}

void URightStatusWidget::HandleIntelligenceDownClicked()
{
	HandleStatDownButtonClicked(GetIntelligenceStatTag());
}

void URightStatusWidget::HandleArcaneDownClicked()
{
	HandleStatDownButtonClicked(GetArcaneStatTag());
}

void URightStatusWidget::HandleArmorDownClicked()
{
	HandleStatDownButtonClicked(GetArmorStatTag());
}

void URightStatusWidget::HandleRecoveryDownClicked()
{
	HandleStatDownButtonClicked(GetRecoveryStatTag());
}

void URightStatusWidget::HandleMaxShieldDownClicked()
{
	HandleStatDownButtonClicked(GetMaxShieldStatTag());
}

void URightStatusWidget::HandleFrostbiteDownClicked()
{
	HandleStatDownButtonClicked(GetFrostbiteStatTag());
}

void URightStatusWidget::HandleBurnDownClicked()
{
	HandleStatDownButtonClicked(GetBurnStatTag());
}

void URightStatusWidget::HandleElectricShockDownClicked()
{
	HandleStatDownButtonClicked(GetElectricShockStatTag());
}

void URightStatusWidget::HandleFirstPandoraDownClicked()
{
	HandleStatDownButtonClicked(GetFirstPandoraStatTag());
}

void URightStatusWidget::HandleSecondPandoraDownClicked()
{
	HandleStatDownButtonClicked(GetSecondPandoraStatTag());
}

void URightStatusWidget::HandleThirdPandoraDownClicked()
{
	HandleStatDownButtonClicked(GetThirdPandoraStatTag());
}

void URightStatusWidget::HandleMaxHealthDownClicked()
{
	HandleStatDownButtonClicked(GetMaxHealthStatTag());
}

void URightStatusWidget::HandleMaxManaDownClicked()
{
	HandleStatDownButtonClicked(GetMaxManaStatTag());
}

void URightStatusWidget::HandleMaxStaminaDownClicked()
{
	HandleStatDownButtonClicked(GetMaxStaminaStatTag());
}

void URightStatusWidget::HandleAttackSpeedDownClicked()
{
	HandleStatDownButtonClicked(GetAttackSpeedStatTag());
}

void URightStatusWidget::HandleMovementSpeedDownClicked()
{
	HandleStatDownButtonClicked(GetMovementSpeedStatTag());
}

void URightStatusWidget::HandleCriticalDownClicked()
{
	HandleStatDownButtonClicked(GetCriticalStatTag());
}

void URightStatusWidget::HandleStatUpButtonClicked(FGameplayTag InStatTag)
{
	if (!InStatTag.IsValid())
	{
		return;
	}

	BroadcastClickedStatUpButton(InStatTag);
}

void URightStatusWidget::HandleStatDownButtonClicked(FGameplayTag InStatTag)
{
	if (!InStatTag.IsValid())
	{
		return;
	}

	BroadcastClickedStatDownButton(InStatTag);
}

// 디자이너가 비워 둔 능력치 태그가 있으면 그 줄의 올리기·내리기 버튼이 아무 일도 하지 않으므로 생성 시점에 알린다.
void URightStatusWidget::ValidateConfiguredStatTags() const
{
	const TPair<const TCHAR*, FGameplayTag> ConfiguredStatTags[] =
	{
		{ TEXT("StrengthStatTag"), GetStrengthStatTag() },
		{ TEXT("IntelligenceStatTag"), GetIntelligenceStatTag() },
		{ TEXT("ArcaneStatTag"), GetArcaneStatTag() },
		{ TEXT("ArmorStatTag"), GetArmorStatTag() },
		{ TEXT("RecoveryStatTag"), GetRecoveryStatTag() },
		{ TEXT("MaxShieldStatTag"), GetMaxShieldStatTag() },
		{ TEXT("FrostbiteStatTag"), GetFrostbiteStatTag() },
		{ TEXT("BurnStatTag"), GetBurnStatTag() },
		{ TEXT("ElectricShockStatTag"), GetElectricShockStatTag() },
		{ TEXT("FirstPandoraStatTag"), GetFirstPandoraStatTag() },
		{ TEXT("SecondPandoraStatTag"), GetSecondPandoraStatTag() },
		{ TEXT("ThirdPandoraStatTag"), GetThirdPandoraStatTag() },
		{ TEXT("MaxHealthStatTag"), GetMaxHealthStatTag() },
		{ TEXT("MaxManaStatTag"), GetMaxManaStatTag() },
		{ TEXT("MaxStaminaStatTag"), GetMaxStaminaStatTag() },
		{ TEXT("AttackSpeedStatTag"), GetAttackSpeedStatTag() },
		{ TEXT("MovementSpeedStatTag"), GetMovementSpeedStatTag() },
		{ TEXT("CriticalStatTag"), GetCriticalStatTag() },
	};

	for (const TPair<const TCHAR*, FGameplayTag>& ConfiguredStatTag : ConfiguredStatTags)
	{
		UE_CLOG(!ConfiguredStatTag.Value.IsValid(), LogRightStatusWidget, Warning,
			TEXT("%s: %s is not set, so its stat up and down buttons do nothing."),
			*GetNameSafe(this), ConfiguredStatTag.Key);
	}
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
	return UProjectTagDefinition::Get(this)->GetStatusStrengthTag();
}

FGameplayTag URightStatusWidget::GetIntelligenceStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusIntelligenceTag();
}

FGameplayTag URightStatusWidget::GetArcaneStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusArcaneTag();
}

FGameplayTag URightStatusWidget::GetArmorStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusArmorTag();
}

FGameplayTag URightStatusWidget::GetRecoveryStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusRecoveryTag();
}

FGameplayTag URightStatusWidget::GetMaxShieldStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusMaxShieldTag();
}

FGameplayTag URightStatusWidget::GetFrostbiteStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusFrostbiteTag();
}

FGameplayTag URightStatusWidget::GetBurnStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusBurnTag();
}

FGameplayTag URightStatusWidget::GetElectricShockStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusElectricShockTag();
}

FGameplayTag URightStatusWidget::GetFirstPandoraStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusFirstPandoraTag();
}

FGameplayTag URightStatusWidget::GetSecondPandoraStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusSecondPandoraTag();
}

FGameplayTag URightStatusWidget::GetThirdPandoraStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusThirdPandoraTag();
}

FGameplayTag URightStatusWidget::GetMaxHealthStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusMaxHealthTag();
}

FGameplayTag URightStatusWidget::GetMaxManaStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusMaxManaTag();
}

FGameplayTag URightStatusWidget::GetMaxStaminaStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusMaxStaminaTag();
}

FGameplayTag URightStatusWidget::GetAttackSpeedStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusAttackSpeedTag();
}

FGameplayTag URightStatusWidget::GetMovementSpeedStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusMovementSpeedTag();
}

FGameplayTag URightStatusWidget::GetCriticalStatTag() const
{
	return UProjectTagDefinition::Get(this)->GetStatusCriticalTag();
}
