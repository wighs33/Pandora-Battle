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

namespace
{
	// 숫자가 목표에 이만큼 가까우면 굴리기를 끝낸다.
	constexpr float HealthCountSnapDistance = 0.5f;

	// 레벨이 오르면 레벨 표시가 이 속도로 커졌다가 스프링으로 돌아온다(감쇠비 약 0.4).
	constexpr float LevelUpKickVelocity = 4.5f;
	const UiMotion::FSpringSettings LevelSpring{380.f, 16.f};
}

void UPlayerVitalsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BeginGameSettingContentPreload();
	PossessedCharacterReadySubscription.SubscribeToPossessedCharacter(GetOwningPlayer(),
		FOnAbilitySystemAvailabilityChanged::FDelegate::CreateUObject(this, &ThisClass::HandlePossessedCharacterReady));
}

void UPlayerVitalsWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	StepHealthMotion(InDeltaTime);
	StepLevelMotion(InDeltaTime);
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
		NormalStaminaFillTint = ResolvedStaminaBar->GetWidgetStyle().FillImage.TintColor.GetSpecifiedColor();
		bHasCachedNormalStaminaFillTint = true;
	}

	CurrentStamina = BoundAbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetStaminaAttribute());
	CurrentMaxStamina = BoundAbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetMaxStaminaAttribute());

	BindStaminaAttributeDelegates();
	RefreshStaminaFillTint();
	RefreshResourceReadouts();
	RefreshHealthMotion(false);
	RefreshLevelMotion(false);
}

void UPlayerVitalsWidget::BindStaminaAttributeDelegates()
{
	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	// 표시는 기존 MVVM 막대와 같은 ASC를 따르고, 주기적으로 값을 묻지 않는다.
	for (const FGameplayAttribute Attribute : {
		UBasicAttributeSet::GetHealthAttribute(), UBasicAttributeSet::GetMaxHealthAttribute(),
		UBasicAttributeSet::GetManaAttribute(), UBasicAttributeSet::GetMaxManaAttribute(),
		UBasicAttributeSet::GetShieldAttribute(), UBasicAttributeSet::GetMaxShieldAttribute(),
		UBasicAttributeSet::GetLevelAttribute()})
	{
		ResourceDelegateHandles.Emplace(Attribute, BoundAbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleResourceChanged));
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

	SettingsSubsystem->PreloadRuntimeContentAsync(FSimpleDelegate::CreateWeakLambda(this, [this, PreloadGeneration]()
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

	const UGameSettingDefinition* SettingDefinition = UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(this);
	if (!SettingDefinition)
	{
		SettingDefinition = GetDefault<UGameSettingDefinition>();
	}

	const float StaminaRatio = CurrentMaxStamina > UE_SMALL_NUMBER
		? FMath::Clamp(CurrentStamina / CurrentMaxStamina, 0.0f, 1.0f)
		: 1.0f;
	const float StaminaPercent = StaminaRatio * 100.0f;
	float TargetTintValue = NormalStaminaFillTint.LinearRGBToHSV().B;

	if (StaminaPercent <= FMath::Clamp(SettingDefinition->CriticalStaminaThresholdPercent, 0.0f, 100.0f))
	{
		TargetTintValue = FMath::Clamp(SettingDefinition->CriticalStaminaFillTintValue, 0.0f, 1.0f);
	}
	else if (StaminaPercent <= FMath::Clamp(SettingDefinition->LowStaminaThresholdPercent, 0.0f, 100.0f))
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
	WriteHealthText();
}

void UPlayerVitalsWidget::HandleResourceChanged(const FOnAttributeChangeData& Data)
{
	if (Data.Attribute == UBasicAttributeSet::GetHealthAttribute() || Data.Attribute == UBasicAttributeSet::GetMaxHealthAttribute())
	{
		RefreshHealthMotion(true);
	}
	else if (Data.Attribute == UBasicAttributeSet::GetLevelAttribute())
	{
		RefreshLevelMotion(true);
	}
	RefreshResourceReadouts();
}

void UPlayerVitalsWidget::RefreshResourceReadouts()
{
	if (!BoundAbilitySystemComponent)
	{
		return;
	}
	const auto Number = [this](FGameplayAttribute Attribute)
	{
		return FText::AsNumber(FMath::Max(0, FMath::RoundToInt(BoundAbilitySystemComponent->GetNumericAttribute(Attribute))));
	};
	const auto SetValue = [this, &Number](FName Name, FGameplayAttribute Current, FGameplayAttribute Maximum)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(Name)))
		{
			Text->SetText(FText::Format(INVTEXT("{0} / {1}"), Number(Current), Number(Maximum)));
		}
	};
	// 체력 숫자는 굴러가며 바뀌므로 WriteHealthText가 따로 쓴다.
	SetValue(TEXT("ManaValue"), UBasicAttributeSet::GetManaAttribute(), UBasicAttributeSet::GetMaxManaAttribute());
	SetValue(TEXT("StaminaValue"), UBasicAttributeSet::GetStaminaAttribute(), UBasicAttributeSet::GetMaxStaminaAttribute());
	if (ShieldValue)
	{
		const int32 Shield = FMath::Max(0, FMath::RoundToInt(BoundAbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetShieldAttribute())));
		ShieldValue->SetText(FText::Format(INVTEXT("+{0}"), FText::AsNumber(Shield)));
		ShieldValue->SetVisibility(Shield > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("LevelValue"))))
	{
		Text->SetText(Number(UBasicAttributeSet::GetLevelAttribute()));
	}
}

void UPlayerVitalsWidget::RefreshHealthMotion(const bool bAnimate)
{
	if (!BoundAbilitySystemComponent)
	{
		return;
	}
	if (!bHasHealthBaseColors && HealthBar && HealthValue)
	{
		HealthFillBaseColor = HealthBar->GetFillColorAndOpacity();
		HealthTextBaseColor = HealthValue->GetColorAndOpacity();
		bHasHealthBaseColors = true;
	}

	const float PreviousHealth = TargetHealth;
	TargetHealth = FMath::Max(0.f, BoundAbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()));
	MaxHealth = FMath::Max(0.f, BoundAbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetMaxHealthAttribute()));
	const float Fraction = MaxHealth > UE_SMALL_NUMBER ? FMath::Clamp(TargetHealth / MaxHealth, 0.f, 1.f) : 0.f;

	if (!bAnimate)
	{
		HealthTrail.Snap(Fraction);
		DisplayedHealth = TargetHealth;
	}
	else
	{
		HealthTrail.SetTarget(Fraction, DamageTrailHoldSeconds);
		if (TargetHealth < PreviousHealth)
		{
			HitFlash.Start();
		}
	}
	StepHealthMotion(0.f);
	WriteHealthText();
}

void UPlayerVitalsWidget::StepHealthMotion(const float DeltaSeconds)
{
	const bool bLow = IsHealthLow();
	const bool bCounting = DisplayedHealth != TargetHealth;
	if (HealthTrail.IsSettled() && !bCounting && !HitFlash.IsPlaying() && !bLow && !bLowHealthShown)
	{
		return;
	}

	HealthTrail.Step(DeltaSeconds, DamageTrailCatchUpSpeed);
	if (HealthTrailBar)
	{
		HealthTrailBar->SetPercent(HealthTrail.Value);
	}

	if (bCounting)
	{
		const int32 ShownBefore = FMath::RoundToInt(DisplayedHealth);
		DisplayedHealth = FMath::FInterpTo(DisplayedHealth, TargetHealth, DeltaSeconds, HealthCountSpeed);
		if (FMath::Abs(DisplayedHealth - TargetHealth) < HealthCountSnapDistance)
		{
			DisplayedHealth = TargetHealth;
		}
		if (FMath::RoundToInt(DisplayedHealth) != ShownBefore)
		{
			WriteHealthText();
		}
	}

	if (!bHasHealthBaseColors)
	{
		return;
	}
	if (HealthBar)
	{
		const float Flash = HitFlash.IsPlaying() ? 1.f - UiMotion::EaseOutCubic(HitFlash.Advance(DeltaSeconds, HitFlashSeconds)) : 0.f;
		HealthBar->SetFillColorAndOpacity(FMath::Lerp(HealthFillBaseColor, HitFlashColor, Flash));
	}
	if (HealthValue)
	{
		// 체력이 낮으면 숫자가 원래 색과 경고 색 사이를 사인 곡선으로 오간다. 벗어나면 원래 색으로 한 번 돌려놓는다.
		LowHealthElapsed = bLow ? LowHealthElapsed + DeltaSeconds : 0.f;
		const float Pulse = bLow ? 0.5f - 0.5f * FMath::Cos(UE_TWO_PI * LowHealthElapsed / LowHealthPulseSeconds) : 0.f;
		HealthValue->SetColorAndOpacity(bLow
			? FSlateColor(FMath::Lerp(HealthTextBaseColor.GetSpecifiedColor(), LowHealthTextColor, Pulse))
			: HealthTextBaseColor);
		bLowHealthShown = bLow;
	}
}

void UPlayerVitalsWidget::WriteHealthText() const
{
	const FText Current = FText::AsNumber(FMath::RoundToInt(DisplayedHealth));
	const FText Maximum = FText::AsNumber(FMath::RoundToInt(MaxHealth));
	if (HealthMaxValue)
	{
		HealthMaxValue->SetText(FText::Format(INVTEXT("/ {0}"), Maximum));
	}
	if (HealthValue)
	{
		HealthValue->SetText(HealthMaxValue ? Current : FText::Format(INVTEXT("{0} / {1}"), Current, Maximum));
	}
}

bool UPlayerVitalsWidget::IsHealthLow() const
{
	return MaxHealth > UE_SMALL_NUMBER && TargetHealth > 0.f && TargetHealth < MaxHealth * LowHealthFraction;
}

void UPlayerVitalsWidget::RefreshLevelMotion(const bool bAnimate)
{
	if (!BoundAbilitySystemComponent)
	{
		return;
	}
	const int32 Level = FMath::RoundToInt(BoundAbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetLevelAttribute()));
	if (bAnimate && CurrentLevel != INDEX_NONE && Level > CurrentLevel)
	{
		LevelScale.Value = 1.f;
		LevelScale.Velocity = LevelUpKickVelocity;
		bLevelSettling = true;
	}
	CurrentLevel = Level;
}

void UPlayerVitalsWidget::StepLevelMotion(const float DeltaSeconds)
{
	if (!bLevelSettling)
	{
		return;
	}
	LevelScale.Step(1.f, DeltaSeconds, LevelSpring);
	if (LevelScale.IsSettled(1.f))
	{
		LevelScale.Snap(1.f);
		bLevelSettling = false;
	}
	if (LevelBadge)
	{
		LevelBadge->SetRenderScale(FVector2D(LevelScale.Value));
	}
}
