#include "UI/Widget/EnemyHealthBarWidget.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyHealthBarWidget)

DEFINE_LOG_CATEGORY_STATIC(LogEnemyHealthBarWidget, Log, All);

void UEnemyHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld())
	{
		InitializeTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::InitializeFromOwner));
		return;
	}

	InitializeFromOwner();
}

void UEnemyHealthBarWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitializeTimerHandle);
	}
	InitializeTimerHandle.Invalidate();

	ClearAnimationTimers();
	UnbindAttributeDelegates();

	Super::NativeDestruct();
}

void UEnemyHealthBarWidget::SetOwnerActor(AActor* InOwnerActor)
{
	OwnerActor = InOwnerActor;
	InitializeRetryCount = 0;
	InitializeFromOwner();
}

void UEnemyHealthBarWidget::UpdateHealthPercent()
{
	if (UProgressBar* HealthProgressBar = GetProgressBar())
	{
		HealthProgressBar->SetPercent(GetHealthPercent(CurrentHealth, MaxHealth));
		return;
	}

	UE_LOG(LogEnemyHealthBarWidget, Warning, TEXT("UpdateHealthPercent failed: progress bar not found widget=%s owner=%s health=%.3f max=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		CurrentHealth,
		MaxHealth);
}

void UEnemyHealthBarWidget::AnimateHealth(const double From, const double To)
{
	ClearAnimationTimers();

	AnimatedHealthPercent = FMath::Clamp(static_cast<float>(From), 0.0f, 1.0f);
	AnimatedHealthTargetPercent = FMath::Clamp(static_cast<float>(To), 0.0f, 1.0f);

	UProgressBar* AnimatedHealthProgressBar = GetAnimatedProgressBar();
	if (!AnimatedHealthProgressBar)
	{
		return;
	}

	AnimatedHealthProgressBar->SetPercent(AnimatedHealthPercent);

	if (AnimatedHealthPercent <= AnimatedHealthTargetPercent)
	{
		HideAnimatedProgressBar();
		return;
	}

	AnimatedHealthProgressBar->SetVisibility(ESlateVisibility::Visible);

	if (AnimateHealthDelay <= 0.0f)
	{
		StartDecreaseHealthAnimation();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AnimateHealthDelayTimer,
			this,
			&ThisClass::StartDecreaseHealthAnimation,
			AnimateHealthDelay,
			false);
	}
}

void UEnemyHealthBarWidget::DecreaseHealthIncrement()
{
	UProgressBar* AnimatedHealthProgressBar = GetAnimatedProgressBar();
	if (!AnimatedHealthProgressBar)
	{
		ClearAnimationTimers();
		return;
	}

	AnimatedHealthPercent = FMath::Max(
		AnimatedHealthTargetPercent,
		AnimatedHealthPercent - AnimatedHealthDecreaseStep);
	AnimatedHealthProgressBar->SetPercent(AnimatedHealthPercent);

	if (AnimatedHealthPercent <= AnimatedHealthTargetPercent)
	{
		ClearAnimationTimers();
		HideAnimatedProgressBar();
	}
}

void UEnemyHealthBarWidget::InitializeFromOwner()
{
	UnbindAttributeDelegates();
	ClearAnimationTimers();

	BoundAbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!BoundAbilitySystemComponent)
	{
		UE_LOG(LogEnemyHealthBarWidget, Warning, TEXT("InitializeFromOwner waiting for ASC: widget=%s owner=%s retry=%d"),
			*GetNameSafe(this),
			*GetNameSafe(OwnerActor.Get()),
			InitializeRetryCount);
		QueueInitializeRetry();
		return;
	}

	bool bFoundHealth = false;
	bool bFoundMaxHealth = false;
	CurrentHealth = GetAttributeValue(UBasicAttributeSet::GetHealthAttribute(), &bFoundHealth);
	MaxHealth = GetAttributeValue(UBasicAttributeSet::GetMaxHealthAttribute(), &bFoundMaxHealth);

	UE_LOG(LogEnemyHealthBarWidget, Log, TEXT("InitializeFromOwner: widget=%s owner=%s asc=%s health=%.3f foundHealth=%s maxHealth=%.3f foundMaxHealth=%s progressBar=%s animatedBar=%s percent=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		*GetNameSafe(BoundAbilitySystemComponent.Get()),
		CurrentHealth,
		bFoundHealth ? TEXT("true") : TEXT("false"),
		MaxHealth,
		bFoundMaxHealth ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetProgressBar()),
		*GetNameSafe(GetAnimatedProgressBar()),
		GetHealthPercent(CurrentHealth, MaxHealth));

	if (!bFoundHealth || !bFoundMaxHealth || !GetProgressBar())
	{
		QueueInitializeRetry();
	}
	else
	{
		InitializeRetryCount = 0;
	}

	UpdateHealthPercent();
	HideAnimatedProgressBar();
	BindAttributeDelegates();
}

void UEnemyHealthBarWidget::QueueInitializeRetry()
{
	if (!GetWorld() || InitializeRetryCount >= MaxInitializeRetryCount)
	{
		return;
	}

	++InitializeRetryCount;
	GetWorld()->GetTimerManager().ClearTimer(InitializeTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		InitializeTimerHandle,
		this,
		&ThisClass::InitializeFromOwner,
		0.1f,
		false);
}

void UEnemyHealthBarWidget::BindAttributeDelegates()
{
	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	HealthChangedHandle = BoundAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute())
		.AddUObject(this, &ThisClass::OnHealthChanged);

	MaxHealthChangedHandle = BoundAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::OnMaxHealthChanged);
}

void UEnemyHealthBarWidget::UnbindAttributeDelegates()
{
	if (!BoundAbilitySystemComponent)
	{
		HealthChangedHandle.Reset();
		MaxHealthChangedHandle.Reset();
		return;
	}

	if (HealthChangedHandle.IsValid())
	{
		BoundAbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetHealthAttribute())
			.Remove(HealthChangedHandle);
		HealthChangedHandle.Reset();
	}

	if (MaxHealthChangedHandle.IsValid())
	{
		BoundAbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxHealthAttribute())
			.Remove(MaxHealthChangedHandle);
		MaxHealthChangedHandle.Reset();
	}

	BoundAbilitySystemComponent = nullptr;
}

void UEnemyHealthBarWidget::StartDecreaseHealthAnimation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DecreaseHealthTimer,
			this,
			&ThisClass::DecreaseHealthIncrement,
			FMath::Max(AnimateHealthDecreaseIncrement, 0.001f),
			true,
			0.0f);
	}
}

void UEnemyHealthBarWidget::ClearAnimationTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AnimateHealthDelayTimer);
		World->GetTimerManager().ClearTimer(DecreaseHealthTimer);
	}

	AnimateHealthDelayTimer.Invalidate();
	DecreaseHealthTimer.Invalidate();
}

void UEnemyHealthBarWidget::HideAnimatedProgressBar()
{
	if (UProgressBar* AnimatedHealthProgressBar = GetAnimatedProgressBar())
	{
		AnimatedHealthProgressBar->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UProgressBar* UEnemyHealthBarWidget::GetProgressBar() const
{
	return FindProgressBarByNameOrIndex(TEXT("ProgressBar"), 0);
}

UProgressBar* UEnemyHealthBarWidget::GetAnimatedProgressBar() const
{
	return FindProgressBarByNameOrIndex(TEXT("AnimatedProgressBar"), 1);
}

UProgressBar* UEnemyHealthBarWidget::FindProgressBarByNameOrIndex(const FName WidgetName, const int32 FallbackIndex) const
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UProgressBar* NamedProgressBar = Cast<UProgressBar>(WidgetTree->FindWidget(WidgetName)))
	{
		return NamedProgressBar;
	}

	int32 ProgressBarIndex = 0;
	UProgressBar* FallbackProgressBar = nullptr;
	WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (FallbackProgressBar)
		{
			return;
		}

		if (UProgressBar* ProgressBar = Cast<UProgressBar>(Widget))
		{
			if (ProgressBarIndex == FallbackIndex)
			{
				FallbackProgressBar = ProgressBar;
			}
			++ProgressBarIndex;
		}
	});

	if (FallbackProgressBar)
	{
		UE_LOG(LogEnemyHealthBarWidget, Warning, TEXT("FindProgressBarByNameOrIndex used fallback: widget=%s requested=%s fallback=%s index=%d"),
			*GetNameSafe(this),
			*WidgetName.ToString(),
			*GetNameSafe(FallbackProgressBar),
			FallbackIndex);
	}

	return FallbackProgressBar;
}

float UEnemyHealthBarWidget::GetAttributeValue(const FGameplayAttribute& Attribute, bool* bOutSuccessfullyFoundAttribute) const
{
	bool bSuccessfullyFoundAttribute = false;
	const float Value = UAbilitySystemBlueprintLibrary::GetFloatAttributeFromAbilitySystemComponent(
		BoundAbilitySystemComponent,
		Attribute,
		bSuccessfullyFoundAttribute);
	if (bOutSuccessfullyFoundAttribute)
	{
		*bOutSuccessfullyFoundAttribute = bSuccessfullyFoundAttribute;
	}
	return Value;
}

UAbilitySystemComponent* UEnemyHealthBarWidget::GetOwnerAbilitySystemComponent() const
{
	return OwnerActor.Get() ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor.Get()) : nullptr;
}

float UEnemyHealthBarWidget::GetHealthPercent(const float CurrentValue, const float MaxValue) const
{
	if (MaxValue <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f);
}

void UEnemyHealthBarWidget::OnHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	const float OldPercent = GetHealthPercent(ChangeData.OldValue, MaxHealth);
	CurrentHealth = ChangeData.NewValue;
	const float NewPercent = GetHealthPercent(CurrentHealth, MaxHealth);

	UE_LOG(LogEnemyHealthBarWidget, Log, TEXT("OnHealthChanged: widget=%s owner=%s old=%.3f new=%.3f max=%.3f oldPercent=%.3f newPercent=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		ChangeData.OldValue,
		ChangeData.NewValue,
		MaxHealth,
		OldPercent,
		NewPercent);

	UpdateHealthPercent();
	AnimateHealth(OldPercent, NewPercent);
}

void UEnemyHealthBarWidget::OnMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	MaxHealth = ChangeData.NewValue;
	UE_LOG(LogEnemyHealthBarWidget, Log, TEXT("OnMaxHealthChanged: widget=%s owner=%s old=%.3f new=%.3f health=%.3f percent=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		ChangeData.OldValue,
		ChangeData.NewValue,
		CurrentHealth,
		GetHealthPercent(CurrentHealth, MaxHealth));
	UpdateHealthPercent();
}
