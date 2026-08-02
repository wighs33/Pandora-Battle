#include "UI/Widget/PlayerVitalsWidget.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Settings/GameSettingsSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerVitalsWidget)

void UPlayerVitalsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BeginGameSettingContentPreload();
	StaminaPresentationInitializeRetryCount = 0;
	if (UWorld* World = GetWorld())
	{
		StaminaPresentationInitializeTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::InitializeStaminaPresentation));
		return;
	}

	InitializeStaminaPresentation();
}

void UPlayerVitalsWidget::NativeDestruct()
{
	ReleaseGameSettingContentPreload();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StaminaPresentationInitializeTimerHandle);
	}
	StaminaPresentationInitializeTimerHandle.Invalidate();

	UnbindStaminaAttributeDelegates();
	Super::NativeDestruct();
}

void UPlayerVitalsWidget::InitializeStaminaPresentation()
{
	UnbindStaminaAttributeDelegates();

	UProgressBar* ResolvedStaminaBar = ResolveStaminaBar();
	BoundAbilitySystemComponent = ResolveOwnerAbilitySystemComponent();
	if (!ResolvedStaminaBar || !BoundAbilitySystemComponent)
	{
		QueueStaminaPresentationInitializeRetry();
		return;
	}

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
	StaminaPresentationInitializeRetryCount = 0;

	BindStaminaAttributeDelegates();
	RefreshStaminaFillTint();
}

void UPlayerVitalsWidget::QueueStaminaPresentationInitializeRetry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	++StaminaPresentationInitializeRetryCount;
	const float RetryDelay =
		StaminaPresentationInitializeRetryCount >= MaxStaminaPresentationInitializeRetryCount
			? 0.25f
			: 0.1f;
	World->GetTimerManager().ClearTimer(StaminaPresentationInitializeTimerHandle);
	World->GetTimerManager().SetTimer(
		StaminaPresentationInitializeTimerHandle,
		this,
		&ThisClass::InitializeStaminaPresentation,
		RetryDelay,
		false);
}

void UPlayerVitalsWidget::BindStaminaAttributeDelegates()
{
	if (!BoundAbilitySystemComponent)
	{
		return;
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

UAbilitySystemComponent* UPlayerVitalsWidget::ResolveOwnerAbilitySystemComponent() const
{
	const APlayerController* OwningPlayerController = GetOwningPlayer();
	APawn* OwningPawn = OwningPlayerController ? OwningPlayerController->GetPawn() : nullptr;
	return OwningPawn
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn)
		: nullptr;
}

void UPlayerVitalsWidget::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	CurrentStamina = Data.NewValue;
	RefreshStaminaFillTint();
}

void UPlayerVitalsWidget::HandleMaxStaminaChanged(const FOnAttributeChangeData& Data)
{
	CurrentMaxStamina = Data.NewValue;
	RefreshStaminaFillTint();
}
