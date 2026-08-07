#include "UI/Widget/EnemyShieldBarWidget.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyShieldBarWidget)

void UEnemyShieldBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeRetryCount = 0;

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
	StopInitializeRetry();

	UnbindAttributeDelegates();

	Super::NativeDestruct();
}

void UEnemyShieldBarWidget::SetOwnerActor(AActor* InOwnerActor)
{
	StopInitializeRetry();
	OwnerActor = InOwnerActor;
	InitializeRetryCount = 0;
	InitializeFromOwner();
}

void UEnemyShieldBarWidget::UpdateShieldPercent()
{
	if (UProgressBar* ProgressBar = GetProgressBar())
	{
		ProgressBar->SetPercent(GetShieldPercent());
		return;
	}

}

void UEnemyShieldBarWidget::InitializeFromOwner()
{
	UnbindAttributeDelegates();
	if (!GetProgressBar())
	{
		StopInitializeRetry();
		return;
	}

	BoundAbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!BoundAbilitySystemComponent)
	{

		QueueInitializeRetry();
		return;
	}

	bool bFoundShield = false;
	bool bFoundMaxShield = false;
	CurrentShield = GetAttributeValue(UBasicAttributeSet::GetShieldAttribute(), &bFoundShield);
	MaxShield = GetAttributeValue(UBasicAttributeSet::GetMaxShieldAttribute(), &bFoundMaxShield);

	if (!bFoundShield || !bFoundMaxShield)
	{
		QueueInitializeRetry();
		UpdateShieldPercent();
		return;
	}

	StopInitializeRetry();
	InitializeRetryCount = 0;

	UpdateShieldPercent();
	BindAttributeDelegates();
}

void UEnemyShieldBarWidget::QueueInitializeRetry()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (InitializeRetryCount >= MaxInitializeRetryCount)
	{
		StopInitializeRetry();
		return;
	}

	++InitializeRetryCount;
	World->GetTimerManager().ClearTimer(InitializeTimerHandle);
	World->GetTimerManager().SetTimer(
		InitializeTimerHandle,
		this,
		&ThisClass::InitializeFromOwner,
		0.1f,
		false);
}

void UEnemyShieldBarWidget::StopInitializeRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitializeTimerHandle);
	}
	InitializeTimerHandle.Invalidate();
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
	return ShieldProgressBar.Get();
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

	UpdateShieldPercent();
}

void UEnemyShieldBarWidget::OnMaxShieldChanged(const FOnAttributeChangeData& ChangeData)
{
	MaxShield = ChangeData.NewValue;

	UpdateShieldPercent();
}
