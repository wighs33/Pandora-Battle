#include "UI/Widget/EnemyShieldBarWidget.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyShieldBarWidget)

DEFINE_LOG_CATEGORY_STATIC(LogEnemyShieldBarWidget, Log, All);

void UEnemyShieldBarWidget::NativeConstruct()
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

void UEnemyShieldBarWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitializeTimerHandle);
	}
	InitializeTimerHandle.Invalidate();

	UnbindAttributeDelegates();

	Super::NativeDestruct();
}

void UEnemyShieldBarWidget::SetOwnerActor(AActor* InOwnerActor)
{
	OwnerActor = InOwnerActor;
	InitializeRetryCount = 0;
	InitializeFromOwner();
}

void UEnemyShieldBarWidget::UpdateShieldPercent()
{
	if (UProgressBar* ShieldProgressBar = GetProgressBar())
	{
		ShieldProgressBar->SetPercent(GetShieldPercent());
		return;
	}

	UE_LOG(LogEnemyShieldBarWidget, Warning, TEXT("UpdateShieldPercent failed: progress bar not found widget=%s owner=%s shield=%.3f maxShield=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		CurrentShield,
		MaxShield);
}

void UEnemyShieldBarWidget::InitializeFromOwner()
{
	UnbindAttributeDelegates();

	BoundAbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!BoundAbilitySystemComponent)
	{
		UE_LOG(LogEnemyShieldBarWidget, Warning, TEXT("InitializeFromOwner waiting for ASC: widget=%s owner=%s retry=%d"),
			*GetNameSafe(this),
			*GetNameSafe(OwnerActor.Get()),
			InitializeRetryCount);
		QueueInitializeRetry();
		return;
	}

	bool bFoundShield = false;
	bool bFoundMaxShield = false;
	CurrentShield = GetAttributeValue(UBasicAttributeSet::GetShieldAttribute(), &bFoundShield);
	MaxShield = GetAttributeValue(UBasicAttributeSet::GetMaxShieldAttribute(), &bFoundMaxShield);

	UE_LOG(LogEnemyShieldBarWidget, Log, TEXT("InitializeFromOwner: widget=%s owner=%s asc=%s shield=%.3f foundShield=%s maxShield=%.3f foundMaxShield=%s progressBar=%s percent=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		*GetNameSafe(BoundAbilitySystemComponent.Get()),
		CurrentShield,
		bFoundShield ? TEXT("true") : TEXT("false"),
		MaxShield,
		bFoundMaxShield ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetProgressBar()),
		GetShieldPercent());

	if (!bFoundShield || !bFoundMaxShield || !GetProgressBar())
	{
		QueueInitializeRetry();
	}
	else
	{
		InitializeRetryCount = 0;
	}

	UpdateShieldPercent();
	BindAttributeDelegates();
}

void UEnemyShieldBarWidget::QueueInitializeRetry()
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

void UEnemyShieldBarWidget::BindAttributeDelegates()
{
	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	ShieldChangedHandle = BoundAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetShieldAttribute())
		.AddUObject(this, &ThisClass::OnShieldChanged);

	MaxShieldChangedHandle = BoundAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldAttribute())
		.AddUObject(this, &ThisClass::OnMaxShieldChanged);
}

void UEnemyShieldBarWidget::UnbindAttributeDelegates()
{
	if (!BoundAbilitySystemComponent)
	{
		ShieldChangedHandle.Reset();
		MaxShieldChangedHandle.Reset();
		return;
	}

	if (ShieldChangedHandle.IsValid())
	{
		BoundAbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetShieldAttribute())
			.Remove(ShieldChangedHandle);
		ShieldChangedHandle.Reset();
	}

	if (MaxShieldChangedHandle.IsValid())
	{
		BoundAbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetMaxShieldAttribute())
			.Remove(MaxShieldChangedHandle);
		MaxShieldChangedHandle.Reset();
	}

	BoundAbilitySystemComponent = nullptr;
}

UProgressBar* UEnemyShieldBarWidget::GetProgressBar() const
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	if (UProgressBar* NamedProgressBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("ProgressBar"))))
	{
		return NamedProgressBar;
	}

	return FindFirstProgressBar();
}

UProgressBar* UEnemyShieldBarWidget::FindFirstProgressBar() const
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	UProgressBar* FallbackProgressBar = nullptr;
	WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!FallbackProgressBar)
		{
			FallbackProgressBar = Cast<UProgressBar>(Widget);
		}
	});

	if (FallbackProgressBar)
	{
		UE_LOG(LogEnemyShieldBarWidget, Warning, TEXT("FindFirstProgressBar used fallback: widget=%s fallback=%s"),
			*GetNameSafe(this),
			*GetNameSafe(FallbackProgressBar));
	}

	return FallbackProgressBar;
}

float UEnemyShieldBarWidget::GetAttributeValue(const FGameplayAttribute& Attribute, bool* bOutSuccessfullyFoundAttribute) const
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

UAbilitySystemComponent* UEnemyShieldBarWidget::GetOwnerAbilitySystemComponent() const
{
	return OwnerActor.Get() ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor.Get()) : nullptr;
}

float UEnemyShieldBarWidget::GetShieldPercent() const
{
	return FMath::Clamp(CurrentShield / FMath::Max(MaxShield, 0.001f), 0.0f, 1.0f);
}

void UEnemyShieldBarWidget::OnShieldChanged(const FOnAttributeChangeData& ChangeData)
{
	CurrentShield = ChangeData.NewValue;
	UE_LOG(LogEnemyShieldBarWidget, Log, TEXT("OnShieldChanged: widget=%s owner=%s old=%.3f new=%.3f maxShield=%.3f percent=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		ChangeData.OldValue,
		ChangeData.NewValue,
		MaxShield,
		GetShieldPercent());
	UpdateShieldPercent();
}

void UEnemyShieldBarWidget::OnMaxShieldChanged(const FOnAttributeChangeData& ChangeData)
{
	MaxShield = ChangeData.NewValue;
	UE_LOG(LogEnemyShieldBarWidget, Log, TEXT("OnMaxShieldChanged: widget=%s owner=%s old=%.3f new=%.3f shield=%.3f percent=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(OwnerActor.Get()),
		ChangeData.OldValue,
		ChangeData.NewValue,
		CurrentShield,
		GetShieldPercent());
	UpdateShieldPercent();
}
