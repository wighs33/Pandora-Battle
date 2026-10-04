#include "UI/HUD/Combat/EnemyShieldBarWidget.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyShieldBarWidget)

void UEnemyShieldBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ObserveOwnerAbilitySystem();
}

void UEnemyShieldBarWidget::NativeDestruct()
{
	OwnerReadySubscription.Reset();
	UnbindAttributeDelegates();

	Super::NativeDestruct();
}

void UEnemyShieldBarWidget::SetOwnerActor(AActor* InOwnerActor)
{
	OwnerActor = InOwnerActor;
	ObserveOwnerAbilitySystem();
}

// 캐릭터 소유자는 ASC 준비 알림에서 연결한다. 캐릭터가 아닌 소유자는 이미 가진 ASC에 바로 연결한다.
void UEnemyShieldBarWidget::ObserveOwnerAbilitySystem()
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

void UEnemyShieldBarWidget::HandleOwnerAbilitySystemReady(
	ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent)
{
	static_cast<void>(Character);
	static_cast<void>(AbilitySystemComponent);
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
		return;
	}

	BoundAbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	CurrentShield = GetAttributeValue(UBasicAttributeSet::GetShieldAttribute());
	MaxShield = GetAttributeValue(UBasicAttributeSet::GetMaxShieldAttribute());

	UpdateShieldPercent();
	BindAttributeDelegates();
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
		BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetShieldAttribute())
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
		BoundAbilitySystemComponent, Attribute, bSuccessfullyFoundAttribute);
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
