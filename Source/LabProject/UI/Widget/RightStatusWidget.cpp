#include "UI/Widget/RightStatusWidget.h"

#include "Common/ProjectTagConfig.h"
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

	const FGameplayTag StrengthStatTag = GetStrengthStatTag();
	const FGameplayTag IntelligenceStatTag = GetIntelligenceStatTag();
	const FGameplayTag ArcaneStatTag = GetArcaneStatTag();
	const FGameplayTag ArmorStatTag = GetArmorStatTag();
	const FGameplayTag RecoveryStatTag = GetRecoveryStatTag();
	const FGameplayTag MagicResistanceStatTag = GetMagicResistanceStatTag();
	const FGameplayTag ImmunityStatTag = GetImmunityStatTag();
	const FGameplayTag FortitudeStatTag = GetFortitudeStatTag();
	const FGameplayTag SanityStatTag = GetSanityStatTag();
	const FGameplayTag FirstPandoraStatTag = GetFirstPandoraStatTag();
	const FGameplayTag SecondPandoraStatTag = GetSecondPandoraStatTag();
	const FGameplayTag ThirdPandoraStatTag = GetThirdPandoraStatTag();
	const FGameplayTag MaxHealthStatTag = GetMaxHealthStatTag();
	const FGameplayTag MaxManaStatTag = GetMaxManaStatTag();
	const FGameplayTag MaxStaminaStatTag = GetMaxStaminaStatTag();
	const FGameplayTag AttackSpeedStatTag = GetAttackSpeedStatTag();
	const FGameplayTag MovementSpeedStatTag = GetMovementSpeedStatTag();
	const FGameplayTag CriticalChanceStatTag = GetCriticalChanceStatTag();

	UE_LOG(LogRightStatusWidget, Log, TEXT("[StatUpgrade] RightStatus NativeConstruct: widget=%s tags=(str:%s int:%s arc:%s armor:%s rec:%s mr:%s imm:%s fort:%s san:%s p1:%s p2:%s p3:%s maxHp:%s maxMana:%s maxSta:%s atkSpd:%s moveSpd:%s crit:%s)"),
		*GetNameSafe(this),
		*StrengthStatTag.ToString(),
		*IntelligenceStatTag.ToString(),
		*ArcaneStatTag.ToString(),
		*ArmorStatTag.ToString(),
		*RecoveryStatTag.ToString(),
		*MagicResistanceStatTag.ToString(),
		*ImmunityStatTag.ToString(),
		*FortitudeStatTag.ToString(),
		*SanityStatTag.ToString(),
		*FirstPandoraStatTag.ToString(),
		*SecondPandoraStatTag.ToString(),
		*ThirdPandoraStatTag.ToString(),
		*MaxHealthStatTag.ToString(),
		*MaxManaStatTag.ToString(),
		*MaxStaminaStatTag.ToString(),
		*AttackSpeedStatTag.ToString(),
		*MovementSpeedStatTag.ToString(),
		*CriticalChanceStatTag.ToString());

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
	UE_LOG(LogRightStatusWidget, Log, TEXT("[StatUpgrade] Broadcast stat up: widget=%s tag=%s valid=%s"),
		*GetNameSafe(this),
		*InStatTag.ToString(),
		InStatTag.IsValid() ? TEXT("true") : TEXT("false"));
	if (!InStatTag.IsValid())
	{
		UE_LOG(LogRightStatusWidget, Warning, TEXT("[StatUpgrade] Broadcast skipped: stat tag is None. Configure the stat tag on %s."),
			*GetNameSafe(this));
		return;
	}

	OnClicked_StatUpButton.Broadcast(InStatTag);
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
	HandleStatUpButtonClicked(GetArmorStatTag(), TEXT("ArmorStatTag"), Button_Up_Toughness);
}

void URightStatusWidget::HandleRecoveryClicked()
{
	HandleStatUpButtonClicked(GetRecoveryStatTag(), TEXT("RecoveryStatTag"), Button_Up_Recovery);
}

void URightStatusWidget::HandleMagicResistanceClicked()
{
	HandleStatUpButtonClicked(GetMagicResistanceStatTag(), TEXT("MagicResistanceStatTag"), Button_Up_MagicResistance);
}

void URightStatusWidget::HandleImmunityClicked()
{
	HandleStatUpButtonClicked(GetImmunityStatTag(), TEXT("ImmunityStatTag"), Button_Up_Immunity);
}

void URightStatusWidget::HandleFortitudeClicked()
{
	HandleStatUpButtonClicked(GetFortitudeStatTag(), TEXT("FortitudeStatTag"), Button_Up_Fortitude);
}

void URightStatusWidget::HandleSanityClicked()
{
	HandleStatUpButtonClicked(GetSanityStatTag(), TEXT("SanityStatTag"), Button_Up_Sanity);
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

void URightStatusWidget::HandleCriticalChanceClicked()
{
	HandleStatUpButtonClicked(GetCriticalChanceStatTag(), TEXT("CriticalChanceStatTag"), Button_Up_CriticalChance);
}

void URightStatusWidget::HandleStatUpButtonClicked(FGameplayTag InStatTag, const TCHAR* StatTagPropertyName, const UButton* SourceButton)
{
	UE_LOG(LogRightStatusWidget, Log, TEXT("[StatUpgrade] Stat up button clicked: widget=%s property=%s button=%s tag=%s valid=%s"),
		*GetNameSafe(this),
		StatTagPropertyName,
		*GetNameSafe(SourceButton),
		*InStatTag.ToString(),
		InStatTag.IsValid() ? TEXT("true") : TEXT("false"));

	if (!InStatTag.IsValid())
	{
		UE_LOG(LogRightStatusWidget, Warning, TEXT("[StatUpgrade] Stat up button ignored: %s.%s is None. button=%s"),
			*GetNameSafe(this),
			StatTagPropertyName,
			*GetNameSafe(SourceButton));
		return;
	}

	BroadcastClickedStatUpButton(InStatTag);
}

void URightStatusWidget::ValidateConfiguredStatTags() const
{
	const auto ValidateTag = [this](const TCHAR* StatTagPropertyName, const FGameplayTag& InStatTag, const UButton* SourceButton)
	{
		if (InStatTag.IsValid())
		{
			return;
		}

		UE_LOG(LogRightStatusWidget, Warning, TEXT("[StatUpgrade] Missing RightStatus stat tag: widget=%s property=%s button=%s"),
			*GetNameSafe(this),
			StatTagPropertyName,
			*GetNameSafe(SourceButton));
	};

	ValidateTag(TEXT("StrengthStatTag"), GetStrengthStatTag(), Button_Up_Strength);
	ValidateTag(TEXT("IntelligenceStatTag"), GetIntelligenceStatTag(), Button_Up_Intelligence);
	ValidateTag(TEXT("ArcaneStatTag"), GetArcaneStatTag(), Button_Up_Arcane);
	ValidateTag(TEXT("ArmorStatTag"), GetArmorStatTag(), Button_Up_Toughness);
	ValidateTag(TEXT("RecoveryStatTag"), GetRecoveryStatTag(), Button_Up_Recovery);
	ValidateTag(TEXT("MagicResistanceStatTag"), GetMagicResistanceStatTag(), Button_Up_MagicResistance);
	ValidateTag(TEXT("ImmunityStatTag"), GetImmunityStatTag(), Button_Up_Immunity);
	ValidateTag(TEXT("FortitudeStatTag"), GetFortitudeStatTag(), Button_Up_Fortitude);
	ValidateTag(TEXT("SanityStatTag"), GetSanityStatTag(), Button_Up_Sanity);
	ValidateTag(TEXT("FirstPandoraStatTag"), GetFirstPandoraStatTag(), Button_Up_FirstPandora);
	ValidateTag(TEXT("SecondPandoraStatTag"), GetSecondPandoraStatTag(), Button_Up_SecondPandora);
	ValidateTag(TEXT("ThirdPandoraStatTag"), GetThirdPandoraStatTag(), Button_Up_ThirdPandora);
	ValidateTag(TEXT("MaxHealthStatTag"), GetMaxHealthStatTag(), Button_Up_MaxHealth);
	ValidateTag(TEXT("MaxManaStatTag"), GetMaxManaStatTag(), Button_Up_MaxMana);
	ValidateTag(TEXT("MaxStaminaStatTag"), GetMaxStaminaStatTag(), Button_Up_MaxStamina);
	ValidateTag(TEXT("AttackSpeedStatTag"), GetAttackSpeedStatTag(), Button_Up_AttackSpeed);
	ValidateTag(TEXT("MovementSpeedStatTag"), GetMovementSpeedStatTag(), Button_Up_MovementSpeed);
	ValidateTag(TEXT("CriticalChanceStatTag"), GetCriticalChanceStatTag(), Button_Up_CriticalChance);
}

void URightStatusWidget::BindButtonCallbacks()
{
	Button_Up_Strength->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStrengthClicked);
	Button_Up_Intelligence->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleIntelligenceClicked);
	Button_Up_Arcane->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleArcaneClicked);
	Button_Up_Toughness->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleArmorClicked);
	Button_Up_Recovery->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRecoveryClicked);
	Button_Up_MagicResistance->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMagicResistanceClicked);
	Button_Up_Immunity->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleImmunityClicked);
	Button_Up_Fortitude->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFortitudeClicked);
	Button_Up_Sanity->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSanityClicked);
	Button_Up_FirstPandora->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFirstPandoraClicked);
	Button_Up_SecondPandora->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSecondPandoraClicked);
	Button_Up_ThirdPandora->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleThirdPandoraClicked);
	Button_Up_MaxHealth->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxHealthClicked);
	Button_Up_MaxMana->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxManaClicked);
	Button_Up_MaxStamina->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMaxStaminaClicked);
	Button_Up_AttackSpeed->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAttackSpeedClicked);
	Button_Up_MovementSpeed->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMovementSpeedClicked);
	Button_Up_CriticalChance->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCriticalChanceClicked);
}

void URightStatusWidget::UnbindButtonCallbacks()
{
	Button_Up_Strength->OnClicked.RemoveDynamic(this, &ThisClass::HandleStrengthClicked);
	Button_Up_Intelligence->OnClicked.RemoveDynamic(this, &ThisClass::HandleIntelligenceClicked);
	Button_Up_Arcane->OnClicked.RemoveDynamic(this, &ThisClass::HandleArcaneClicked);
	Button_Up_Toughness->OnClicked.RemoveDynamic(this, &ThisClass::HandleArmorClicked);
	Button_Up_Recovery->OnClicked.RemoveDynamic(this, &ThisClass::HandleRecoveryClicked);
	Button_Up_MagicResistance->OnClicked.RemoveDynamic(this, &ThisClass::HandleMagicResistanceClicked);
	Button_Up_Immunity->OnClicked.RemoveDynamic(this, &ThisClass::HandleImmunityClicked);
	Button_Up_Fortitude->OnClicked.RemoveDynamic(this, &ThisClass::HandleFortitudeClicked);
	Button_Up_Sanity->OnClicked.RemoveDynamic(this, &ThisClass::HandleSanityClicked);
	Button_Up_FirstPandora->OnClicked.RemoveDynamic(this, &ThisClass::HandleFirstPandoraClicked);
	Button_Up_SecondPandora->OnClicked.RemoveDynamic(this, &ThisClass::HandleSecondPandoraClicked);
	Button_Up_ThirdPandora->OnClicked.RemoveDynamic(this, &ThisClass::HandleThirdPandoraClicked);
	Button_Up_MaxHealth->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxHealthClicked);
	Button_Up_MaxMana->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxManaClicked);
	Button_Up_MaxStamina->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaxStaminaClicked);
	Button_Up_AttackSpeed->OnClicked.RemoveDynamic(this, &ThisClass::HandleAttackSpeedClicked);
	Button_Up_MovementSpeed->OnClicked.RemoveDynamic(this, &ThisClass::HandleMovementSpeedClicked);
	Button_Up_CriticalChance->OnClicked.RemoveDynamic(this, &ThisClass::HandleCriticalChanceClicked);
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

FGameplayTag URightStatusWidget::GetMagicResistanceStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusMagicResistanceTag();
}

FGameplayTag URightStatusWidget::GetImmunityStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusImmunityTag();
}

FGameplayTag URightStatusWidget::GetFortitudeStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusFortitudeTag();
}

FGameplayTag URightStatusWidget::GetSanityStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusSanityTag();
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

FGameplayTag URightStatusWidget::GetCriticalChanceStatTag() const
{
	return UProjectTagConfig::Get(this)->GetStatusCriticalChanceTag();
}
