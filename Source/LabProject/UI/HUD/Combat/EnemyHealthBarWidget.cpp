#include "UI/HUD/Combat/EnemyHealthBarWidget.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Character/CharacterBase.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyHealthBarWidget)

void UEnemyHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ObserveOwnerAbilitySystem();
}

void UEnemyHealthBarWidget::NativeDestruct()
{
	OwnerReadySubscription.Reset();

	ClearAnimationTimers();
	UnbindAttributeDelegates();

	Super::NativeDestruct();
}

void UEnemyHealthBarWidget::SetOwnerActor(AActor* InOwnerActor)
{
	OwnerActor = InOwnerActor;
	ObserveOwnerAbilitySystem();
}

// 캐릭터 소유자는 ASC 준비 알림에서 연결한다. 캐릭터가 아닌 소유자는 이미 가진 ASC에 바로 연결한다.
void UEnemyHealthBarWidget::ObserveOwnerAbilitySystem()
{
	if (ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(OwnerActor.Get()))
	{
		OwnerReadySubscription.SubscribeToCharacter(OwnerCharacter,
			FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleOwnerAbilitySystemReady));
		return;
	}

	OwnerReadySubscription.Reset();
	InitializeFromOwner();
}

void UEnemyHealthBarWidget::HandleOwnerAbilitySystemReady(
	ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent)
{
	static_cast<void>(Character);
	static_cast<void>(AbilitySystemComponent);
	InitializeFromOwner();
}

void UEnemyHealthBarWidget::UpdateHealthPercent()
{
	if (UProgressBar* HealthProgressBar = GetProgressBar())
	{
		HealthProgressBar->SetPercent(GetHealthPercent(CurrentHealth, MaxHealth));
		return;
	}

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
	if (!GetProgressBar())
	{
		return;
	}

	BoundAbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	CurrentHealth = GetAttributeValue(UBasicAttributeSet::GetHealthAttribute());
	MaxHealth = GetAttributeValue(UBasicAttributeSet::GetMaxHealthAttribute());

	UpdateHealthPercent();
	HideAnimatedProgressBar();
	BindAttributeDelegates();
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
	return FindProgressBarByName(TEXT("ProgressBar"));
}

UProgressBar* UEnemyHealthBarWidget::GetAnimatedProgressBar() const
{
	return FindProgressBarByName(TEXT("AnimatedProgressBar"));
}

UProgressBar* UEnemyHealthBarWidget::FindProgressBarByName(const FName WidgetName) const
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	return Cast<UProgressBar>(WidgetTree->FindWidget(WidgetName));
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

UpdateHealthPercent();
	AnimateHealth(OldPercent, NewPercent);
}

void UEnemyHealthBarWidget::OnMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	MaxHealth = ChangeData.NewValue;

	UpdateHealthPercent();
}
