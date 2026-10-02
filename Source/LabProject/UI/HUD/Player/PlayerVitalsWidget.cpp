#include "UI/HUD/Player/PlayerVitalsWidget.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerVitalsWidget)

void UPlayerVitalsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BeginGameSettingContentPreload();
	PossessedCharacterReadySubscription.SubscribeToPossessedCharacter(GetOwningPlayer(),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePossessedCharacterReady));
}

void UPlayerVitalsWidget::NativeDestruct()
{
	PossessedCharacterReadySubscription.Reset();
	ReleaseGameSettingContentPreload();
	UnbindStaminaAttributeDelegates();
	Super::NativeDestruct();
}

// 조종 캐릭터의 ASC가 준비될 때마다(리스폰 포함) 자원 표시를 그 ASC에 다시 연결한다.
void UPlayerVitalsWidget::HandlePossessedCharacterReady(
	ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent)
{
	static_cast<void>(Character);
	UnbindStaminaAttributeDelegates();

	UProgressBar* ResolvedStaminaBar = ResolveStaminaBar();
	if (!ResolvedStaminaBar)
	{
		return;
	}
	BoundAbilitySystemComponent = AbilitySystemComponent;

	if (!bHasCachedNormalStaminaFillTint)
	{
		NormalStaminaFillTint =
			ResolvedStaminaBar->GetWidgetStyle().FillImage.TintColor.GetSpecifiedColor();
		bHasCachedNormalStaminaFillTint = true;
	}

	CurrentStamina =
		BoundAbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetStaminaAttribute());
	CurrentMaxStamina =
		BoundAbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetMaxStaminaAttribute());

	BindStaminaAttributeDelegates();
	RefreshStaminaFillTint();
	RefreshResourceReadouts();
}

void UPlayerVitalsWidget::BindStaminaAttributeDelegates()
{
	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	// Presentation follows the same ASC as the existing MVVM bars, without polling.
	for (const FGameplayAttribute Attribute : {
		UBasicAttributeSet::GetHealthAttribute(), UBasicAttributeSet::GetMaxHealthAttribute(),
		UBasicAttributeSet::GetManaAttribute(), UBasicAttributeSet::GetMaxManaAttribute(),
		UBasicAttributeSet::GetShieldAttribute(), UBasicAttributeSet::GetMaxShieldAttribute(),
		UBasicAttributeSet::GetLevelAttribute()})
	{
		ResourceDelegateHandles.Emplace(Attribute, BoundAbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(Attribute)
			.AddUObject(this, &ThisClass::HandleResourceChanged));
	}

	StaminaChangedDelegateHandle = BoundAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleStaminaChanged);
	MaxStaminaChangedDelegateHandle = BoundAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleMaxStaminaChanged);
}

void UPlayerVitalsWidget::UnbindStaminaAttributeDelegates()
{
	if (BoundAbilitySystemComponent)
	{
		for (const auto& Binding : ResourceDelegateHandles)
		{
			BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Binding.Key).Remove(Binding.Value);
		}
		if (StaminaChangedDelegateHandle.IsValid())
		{
			BoundAbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute())
				.Remove(StaminaChangedDelegateHandle);
		}
		if (MaxStaminaChangedDelegateHandle.IsValid())
		{
			BoundAbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxStaminaAttribute())
				.Remove(MaxStaminaChangedDelegateHandle);
		}
	}

	ResourceDelegateHandles.Reset();
	StaminaChangedDelegateHandle.Reset();
	MaxStaminaChangedDelegateHandle.Reset();
	BoundAbilitySystemComponent = nullptr;
}

void UPlayerVitalsWidget::BeginGameSettingContentPreload()
{
	ReleaseGameSettingContentPreload();
	const int32 PreloadGeneration = ++GameSettingContentPreloadGeneration;

	UGameInstance* GameInstance = GetGameInstance();
	UGameSettingsSubsystem* SettingsSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UGameSettingsSubsystem>() : nullptr;
	if (!SettingsSubsystem)
	{
		return;
	}

	SettingsSubsystem->PreloadRuntimeContentAsync(
		FSimpleDelegate::CreateWeakLambda(
			this,
			[this, PreloadGeneration]()
			{
				if (PreloadGeneration == GameSettingContentPreloadGeneration)
				{
					RefreshStaminaFillTint();
				}
			}));
}

void UPlayerVitalsWidget::ReleaseGameSettingContentPreload()
{
	++GameSettingContentPreloadGeneration;
}

void UPlayerVitalsWidget::RefreshStaminaFillTint()
{
	UProgressBar* ResolvedStaminaBar = ResolveStaminaBar();
	if (!ResolvedStaminaBar || !bHasCachedNormalStaminaFillTint)
	{
		return;
	}

	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(this);
	if (!SettingDefinition)
	{
		SettingDefinition = GetDefault<UGameSettingDefinition>();
	}

	const float StaminaRatio = CurrentMaxStamina > UE_SMALL_NUMBER
		? FMath::Clamp(CurrentStamina / CurrentMaxStamina, 0.0f, 1.0f)
		: 1.0f;
	const float StaminaPercent = StaminaRatio * 100.0f;
	float TargetTintValue = NormalStaminaFillTint.LinearRGBToHSV().B;

	if (StaminaPercent <= FMath::Clamp(
		SettingDefinition->CriticalStaminaThresholdPercent,
		0.0f,
		100.0f))
	{
		TargetTintValue = FMath::Clamp(SettingDefinition->CriticalStaminaFillTintValue, 0.0f, 1.0f);
	}
	else if (StaminaPercent <= FMath::Clamp(
		SettingDefinition->LowStaminaThresholdPercent,
		0.0f,
		100.0f))
	{
		TargetTintValue = FMath::Clamp(SettingDefinition->LowStaminaFillTintValue, 0.0f, 1.0f);
	}

	FLinearColor TargetTintHsv = NormalStaminaFillTint.LinearRGBToHSV();
	TargetTintHsv.B = TargetTintValue;
	FLinearColor TargetTint = TargetTintHsv.HSVToLinearRGB();
	TargetTint.A = NormalStaminaFillTint.A;

	FProgressBarStyle UpdatedStyle = ResolvedStaminaBar->GetWidgetStyle();
	UpdatedStyle.FillImage.TintColor = FSlateColor(TargetTint);
	ResolvedStaminaBar->SetWidgetStyle(UpdatedStyle);
}

UProgressBar* UPlayerVitalsWidget::ResolveStaminaBar() const
{
	if (StaminaBar)
	{
		return StaminaBar;
	}

	return WidgetTree ? Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("StaminaBar"))) : nullptr;
}

void UPlayerVitalsWidget::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	CurrentStamina = Data.NewValue;
	RefreshResourceReadouts();
	RefreshStaminaFillTint();
}

void UPlayerVitalsWidget::HandleMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
	CurrentMaxStamina = Data.NewValue;
	RefreshResourceReadouts();
	RefreshStaminaFillTint();
}

void UPlayerVitalsWidget::OnMenuLanguageChanged()
{
	RefreshResourceReadouts();
}

void UPlayerVitalsWidget::HandleResourceChanged(const FOnAttributeChangeData& Data)
{
	RefreshResourceReadouts();
}

void UPlayerVitalsWidget::RefreshResourceReadouts()
{
	if (!BoundAbilitySystemComponent) return;
	const auto Number = [this](FGameplayAttribute Attribute)
	{
		return FText::AsNumber(FMath::Max(0, FMath::RoundToInt(BoundAbilitySystemComponent->GetNumericAttribute(Attribute))));
	};
	const auto SetValue = [this, &Number](FName Name, FGameplayAttribute Current, FGameplayAttribute Maximum)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(Name)))
		{
			Text->SetText(FText::Format(NSLOCTEXT("PlayerHUD", "ResourcePair", "{0} / {1}"), Number(Current), Number(Maximum)));
		}
	};
	SetValue(TEXT("HealthValue"), UBasicAttributeSet::GetHealthAttribute(), UBasicAttributeSet::GetMaxHealthAttribute());
	SetValue(TEXT("ManaValue"), UBasicAttributeSet::GetManaAttribute(), UBasicAttributeSet::GetMaxManaAttribute());
	SetValue(TEXT("StaminaValue"), UBasicAttributeSet::GetStaminaAttribute(), UBasicAttributeSet::GetMaxStaminaAttribute());
	SetValue(TEXT("ShieldValue"), UBasicAttributeSet::GetShieldAttribute(), UBasicAttributeSet::GetMaxShieldAttribute());
	if (UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("LevelValue"))))
	{
		Text->SetText(Number(UBasicAttributeSet::GetLevelAttribute()));
	}
}
