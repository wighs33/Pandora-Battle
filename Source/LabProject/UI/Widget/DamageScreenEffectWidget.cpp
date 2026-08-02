#include "UI/Widget/DamageScreenEffectWidget.h"

#include "Components/Image.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DamageScreenEffectWidget)

UDamageScreenEffectWidget::UDamageScreenEffectWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDamageScreenEffectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyDamageScreenTint();
	HideDamageScreenEffect();
}

void UDamageScreenEffectWidget::PlayDamageScreenEffect(float DamageAmount)
{
	ApplyDamageScreenTint();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(MaxOpacity);

	if (DamageFlash)
	{
		PlayAnimation(DamageFlash, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f);
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HideTimerHandle,
			this,
			&ThisClass::HideDamageScreenEffect,
			FallbackVisibleDuration,
			false);
	}
}

void UDamageScreenEffectWidget::HideDamageScreenEffect()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	SetRenderOpacity(0.0f);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDamageScreenEffectWidget::ApplyDamageScreenTint()
{
	if (!Img_DamageScreenEffect)
	{
		return;
	}

	Img_DamageScreenEffect->SetColorAndOpacity(DamageScreenTint);
	Img_DamageScreenEffect->SetVisibility(ESlateVisibility::HitTestInvisible);
}
