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
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusStrengthTag());
}

void URightStatusWidget::HandleIntelligenceClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusIntelligenceTag());
}

void URightStatusWidget::HandleArcaneClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusArcaneTag());
}

void URightStatusWidget::HandleArmorClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusArmorTag());
}

void URightStatusWidget::HandleRecoveryClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusRecoveryTag());
}

void URightStatusWidget::HandleMaxShieldClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusMaxShieldTag());
}

void URightStatusWidget::HandleFrostbiteClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusFrostbiteTag());
}

void URightStatusWidget::HandleBurnClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusBurnTag());
}

void URightStatusWidget::HandleElectricShockClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusElectricShockTag());
}

void URightStatusWidget::HandleFirstPandoraClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusFirstPandoraTag());
}

void URightStatusWidget::HandleSecondPandoraClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusSecondPandoraTag());
}

void URightStatusWidget::HandleThirdPandoraClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusThirdPandoraTag());
}

void URightStatusWidget::HandleMaxHealthClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusMaxHealthTag());
}

void URightStatusWidget::HandleMaxManaClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusMaxManaTag());
}

void URightStatusWidget::HandleMaxStaminaClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusMaxStaminaTag());
}

void URightStatusWidget::HandleAttackSpeedClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusAttackSpeedTag());
}

void URightStatusWidget::HandleMovementSpeedClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusMovementSpeedTag());
}

void URightStatusWidget::HandleCriticalClicked()
{
	BroadcastClickedStatUpButton(UProjectTagDefinition::Get(this)->GetStatusCriticalTag());
}

void URightStatusWidget::HandleStrengthDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusStrengthTag());
}

void URightStatusWidget::HandleIntelligenceDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusIntelligenceTag());
}

void URightStatusWidget::HandleArcaneDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusArcaneTag());
}

void URightStatusWidget::HandleArmorDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusArmorTag());
}

void URightStatusWidget::HandleRecoveryDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusRecoveryTag());
}

void URightStatusWidget::HandleMaxShieldDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusMaxShieldTag());
}

void URightStatusWidget::HandleFrostbiteDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusFrostbiteTag());
}

void URightStatusWidget::HandleBurnDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusBurnTag());
}

void URightStatusWidget::HandleElectricShockDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusElectricShockTag());
}

void URightStatusWidget::HandleFirstPandoraDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusFirstPandoraTag());
}

void URightStatusWidget::HandleSecondPandoraDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusSecondPandoraTag());
}

void URightStatusWidget::HandleThirdPandoraDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusThirdPandoraTag());
}

void URightStatusWidget::HandleMaxHealthDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusMaxHealthTag());
}

void URightStatusWidget::HandleMaxManaDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusMaxManaTag());
}

void URightStatusWidget::HandleMaxStaminaDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusMaxStaminaTag());
}

void URightStatusWidget::HandleAttackSpeedDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusAttackSpeedTag());
}

void URightStatusWidget::HandleMovementSpeedDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusMovementSpeedTag());
}

void URightStatusWidget::HandleCriticalDownClicked()
{
	BroadcastClickedStatDownButton(UProjectTagDefinition::Get(this)->GetStatusCriticalTag());
}

// 디자이너가 비워 둔 능력치 태그가 있으면 그 줄의 올리기·내리기 버튼이 아무 일도 하지 않으므로 생성 시점에 알린다.
void URightStatusWidget::ValidateConfiguredStatTags() const
{
	const UProjectTagDefinition* Tags = UProjectTagDefinition::Get(this);
	const TPair<const TCHAR*, FGameplayTag> ConfiguredStatTags[] = {
		{ TEXT("StrengthStatTag"), Tags->GetStatusStrengthTag() },
		{ TEXT("IntelligenceStatTag"), Tags->GetStatusIntelligenceTag() },
		{ TEXT("ArcaneStatTag"), Tags->GetStatusArcaneTag() },
		{ TEXT("ArmorStatTag"), Tags->GetStatusArmorTag() },
		{ TEXT("RecoveryStatTag"), Tags->GetStatusRecoveryTag() },
		{ TEXT("MaxShieldStatTag"), Tags->GetStatusMaxShieldTag() },
		{ TEXT("FrostbiteStatTag"), Tags->GetStatusFrostbiteTag() },
		{ TEXT("BurnStatTag"), Tags->GetStatusBurnTag() },
		{ TEXT("ElectricShockStatTag"), Tags->GetStatusElectricShockTag() },
		{ TEXT("FirstPandoraStatTag"), Tags->GetStatusFirstPandoraTag() },
		{ TEXT("SecondPandoraStatTag"), Tags->GetStatusSecondPandoraTag() },
		{ TEXT("ThirdPandoraStatTag"), Tags->GetStatusThirdPandoraTag() },
		{ TEXT("MaxHealthStatTag"), Tags->GetStatusMaxHealthTag() },
		{ TEXT("MaxManaStatTag"), Tags->GetStatusMaxManaTag() },
		{ TEXT("MaxStaminaStatTag"), Tags->GetStatusMaxStaminaTag() },
		{ TEXT("AttackSpeedStatTag"), Tags->GetStatusAttackSpeedTag() },
		{ TEXT("MovementSpeedStatTag"), Tags->GetStatusMovementSpeedTag() },
		{ TEXT("CriticalStatTag"), Tags->GetStatusCriticalTag() },
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

