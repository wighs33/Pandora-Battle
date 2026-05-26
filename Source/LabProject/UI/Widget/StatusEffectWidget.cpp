#include "UI/Widget/StatusEffectWidget.h"

#include "AbilitySystem/Data/PdStatusEffectDataAsset.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Common/LabGameplayTags.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectWidget)

namespace
{
	constexpr float MeterEmptyPercent = 0.0f;
	constexpr float MeterFullPercent = 1.0f;

	void ConfigureIconBrush(FSlateBrush& Brush, UObject* ResourceObject)
	{
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(32.0f, 32.0f);
		Brush.SetResourceObject(ResourceObject);
	}
}

void UStatusEffectWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyDesignerDefaults();
}

void UStatusEffectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsConstructed = true;
	InitializeStatusEffect();
}

void UStatusEffectWidget::NativeDestruct()
{
	bIsConstructed = false;
	ClearDecreaseStackFillTimer();
	ClearUpdateTimeRemainingTimer();
	UnbindGameplayListeners();

	Super::NativeDestruct();
}

void UStatusEffectWidget::SetOwnerActor(AActor* InOwnerActor)
{
	if (OwnerActor.Get() == InOwnerActor)
	{
		return;
	}

	OwnerActor = InOwnerActor;

	if (bIsConstructed)
	{
		InitializeStatusEffect();
	}
}

void UStatusEffectWidget::SetEffectDataAsset(UPdStatusEffectDataAsset* InEffectDataAsset)
{
	if (EffectDataAsset.Get() == InEffectDataAsset)
	{
		return;
	}

	if (bIsConstructed)
	{
		UnbindGameplayListeners();
	}

	EffectDataAsset = InEffectDataAsset;

	if (bIsConstructed)
	{
		InitializeStatusEffect();
	}
}

UPdStatusEffectDataAsset* UStatusEffectWidget::GetEffectDataAsset() const
{
	return EffectDataAsset.Get();
}

void UStatusEffectWidget::InitializeStatusEffect()
{
	ClearDecreaseStackFillTimer();
	ClearUpdateTimeRemainingTimer();
	UnbindGameplayListeners();

	SetInitialValues();
	SetIconStyle();
	BindGameplayListeners();
	RefreshFromActiveEffects();
	StartDecreaseFillMeterTimer();
}

void UStatusEffectWidget::ApplyDesignerDefaults()
{
	if (EffectFillMeter)
	{
		EffectFillMeter->SetBarFillType(EProgressBarFillType::BottomToTop);
	}

	if (EffectAppliedTimeLeft)
	{
		EffectAppliedTimeLeft->SetBarFillType(EProgressBarFillType::BottomToTop);
	}
}

void UStatusEffectWidget::SetInitialValues()
{
	CurrentStackCount = 0;
	bRemoveWhenStatusEffectTagRemoved = false;

	if (EffectAppliedTimeLeft)
	{
		EffectAppliedTimeLeft->SetPercent(MeterEmptyPercent);
	}

	if (EffectFillMeter)
	{
		EffectFillMeter->SetPercent(MeterEmptyPercent);
	}

	if (EffectIcon)
	{
		EffectIcon->SetRenderOpacity(InitialIconOpacity);
	}
}

void UStatusEffectWidget::SetIconStyle()
{
	if (!EffectDataAsset)
	{
		return;
	}

	if (EffectIcon && EffectDataAsset->Icon)
	{
		FSlateBrush Brush;
		ConfigureIconBrush(Brush, EffectDataAsset->Icon);
		EffectIcon->SetBrush(Brush);
	}

	if (EffectFillMeter)
	{
		EffectFillMeter->SetFillColorAndOpacity(EffectDataAsset->IconBackgroundColor);
	}
}

void UStatusEffectWidget::RefreshFromActiveEffects()
{
	UAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!AbilitySystemComponent || !EffectDataAsset)
	{
		UpdateFillMeter();
		return;
	}

	CurrentStackCount = GetActiveDebuffStackCount();
	UpdateFillMeter();

	if (EffectDataAsset->StatusEffectTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(EffectDataAsset->StatusEffectTag))
	{
		HandleStatusEffectApplied();
	}
}

void UStatusEffectWidget::UpdateFillMeter()
{
	if (!EffectFillMeter)
	{
		return;
	}

	const int32 MaxStackCount = GetMaxStackCount();
	const float FillPercent = MaxStackCount > 0
		? static_cast<float>(CurrentStackCount) / static_cast<float>(MaxStackCount)
		: MeterEmptyPercent;

	EffectFillMeter->SetPercent(FMath::Clamp(FillPercent, MeterEmptyPercent, MeterFullPercent));
}

void UStatusEffectWidget::StartDecreaseFillMeterTimer()
{
	ClearDecreaseStackFillTimer();

	if (!GetWorld() || GetDebuffStackDuration() <= 0.0f || MeterUpdateInterval <= 0.0f || GetMaxStackCount() <= 0)
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		DecreaseStackFillTimer,
		this,
		&ThisClass::DecreaseStackFill,
		MeterUpdateInterval,
		true);
}

void UStatusEffectWidget::DecreaseStackFill()
{
	if (!EffectFillMeter || GetDebuffStackDuration() <= 0.0f || GetMaxStackCount() <= 0)
	{
		return;
	}

	const float DecreaseAmount = MeterUpdateInterval / GetDebuffStackDuration() / static_cast<float>(GetMaxStackCount());
	const float NewPercent = FMath::Clamp(
		EffectFillMeter->GetPercent() - DecreaseAmount,
		MeterEmptyPercent,
		MeterFullPercent);

	EffectFillMeter->SetPercent(NewPercent);
}

void UStatusEffectWidget::HandleStatusEffectApplied()
{
	bRemoveWhenStatusEffectTagRemoved = true;
	ClearDecreaseStackFillTimer();

	if (EffectIcon)
	{
		EffectIcon->SetRenderOpacity(1.0f);
	}

	if (EffectFillMeter)
	{
		EffectFillMeter->SetPercent(MeterFullPercent);
	}

	if (EffectAppliedTimeLeft)
	{
		EffectAppliedTimeLeft->SetPercent(MeterFullPercent);
	}

	if (!GetWorld() || GetStatusDuration() <= 0.0f || MeterUpdateInterval <= 0.0f)
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		UpdateTimeRemainingTimer,
		this,
		&ThisClass::UpdateTimeRemaining,
		MeterUpdateInterval,
		true);
}

void UStatusEffectWidget::UpdateTimeRemaining()
{
	if (!EffectAppliedTimeLeft || GetStatusDuration() <= 0.0f)
	{
		return;
	}

	const float NewPercent = FMath::Clamp(
		EffectAppliedTimeLeft->GetPercent() - (MeterUpdateInterval / GetStatusDuration()),
		MeterEmptyPercent,
		MeterFullPercent);
	EffectAppliedTimeLeft->SetPercent(NewPercent);

	if (NewPercent <= MeterEmptyPercent)
	{
		ClearUpdateTimeRemainingTimer();
	}
}

void UStatusEffectWidget::EvaluateRemovalAfterDebuffRemoved()
{
	UAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (AbilitySystemComponent && EffectDataAsset && EffectDataAsset->StatusEffectTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(EffectDataAsset->StatusEffectTag))
	{
		bRemoveWhenStatusEffectTagRemoved = true;
		return;
	}

	RemoveStatusEffectWidget();
}

void UStatusEffectWidget::RemoveStatusEffectWidget()
{
	ClearDecreaseStackFillTimer();
	ClearUpdateTimeRemainingTimer();
	UnbindGameplayListeners();
	RemoveFromParent();
}

void UStatusEffectWidget::BindGameplayListeners()
{
	UnbindGameplayListeners();

	if (!EffectDataAsset)
	{
		return;
	}

	BoundAbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	BoundDebuffTag = EffectDataAsset->DebuffTag;
	BoundStatusEffectTag = EffectDataAsset->StatusEffectTag;

	if (BoundDebuffTag.IsValid())
	{
		DebuffTagChangedHandle = BoundAbilitySystemComponent
			->RegisterGameplayTagEvent(BoundDebuffTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnDebuffTagChanged);
	}

	if (BoundStatusEffectTag.IsValid())
	{
		StatusEffectTagChangedHandle = BoundAbilitySystemComponent
			->RegisterGameplayTagEvent(BoundStatusEffectTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnStatusEffectTagChanged);
	}

	StackCountChangedEventHandle = BoundAbilitySystemComponent
		->GenericGameplayEventCallbacks
		.FindOrAdd(LabGameplayTags::Event_Effect_StackCountChanged)
		.AddUObject(this, &ThisClass::OnStackCountChangedEvent);
}

void UStatusEffectWidget::UnbindGameplayListeners()
{
	if (!BoundAbilitySystemComponent)
	{
		DebuffTagChangedHandle.Reset();
		StatusEffectTagChangedHandle.Reset();
		StackCountChangedEventHandle.Reset();
		BoundDebuffTag = FGameplayTag();
		BoundStatusEffectTag = FGameplayTag();
		return;
	}

	if (DebuffTagChangedHandle.IsValid() && BoundDebuffTag.IsValid())
	{
		BoundAbilitySystemComponent
			->RegisterGameplayTagEvent(BoundDebuffTag, EGameplayTagEventType::NewOrRemoved)
			.Remove(DebuffTagChangedHandle);
	}

	if (StatusEffectTagChangedHandle.IsValid() && BoundStatusEffectTag.IsValid())
	{
		BoundAbilitySystemComponent
			->RegisterGameplayTagEvent(BoundStatusEffectTag, EGameplayTagEventType::NewOrRemoved)
			.Remove(StatusEffectTagChangedHandle);
	}

	if (StackCountChangedEventHandle.IsValid())
	{
		if (FGameplayEventMulticastDelegate* EventDelegate =
			BoundAbilitySystemComponent->GenericGameplayEventCallbacks.Find(LabGameplayTags::Event_Effect_StackCountChanged))
		{
			EventDelegate->Remove(StackCountChangedEventHandle);
		}
	}

	BoundAbilitySystemComponent = nullptr;
	DebuffTagChangedHandle.Reset();
	StatusEffectTagChangedHandle.Reset();
	StackCountChangedEventHandle.Reset();
	BoundDebuffTag = FGameplayTag();
	BoundStatusEffectTag = FGameplayTag();
}

void UStatusEffectWidget::ClearDecreaseStackFillTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DecreaseStackFillTimer);
	}

	DecreaseStackFillTimer.Invalidate();
}

void UStatusEffectWidget::ClearUpdateTimeRemainingTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimeRemainingTimer);
	}

	UpdateTimeRemainingTimer.Invalidate();
}

void UStatusEffectWidget::OnDebuffTagChanged(const FGameplayTag CallbackTag, const int32 NewCount)
{
	(void)CallbackTag;

	if (NewCount > 0)
	{
		CurrentStackCount = FMath::Max(CurrentStackCount, 1);
		UpdateFillMeter();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::EvaluateRemovalAfterDebuffRemoved));
		return;
	}

	EvaluateRemovalAfterDebuffRemoved();
}

void UStatusEffectWidget::OnStatusEffectTagChanged(const FGameplayTag CallbackTag, const int32 NewCount)
{
	(void)CallbackTag;

	if (NewCount > 0)
	{
		HandleStatusEffectApplied();
		return;
	}

	if (bRemoveWhenStatusEffectTagRemoved)
	{
		RemoveStatusEffectWidget();
	}
}

void UStatusEffectWidget::OnStackCountChangedEvent(const FGameplayEventData* Payload)
{
	if (!Payload || !EffectDataAsset || !Payload->TargetTags.HasTagExact(EffectDataAsset->DebuffTag))
	{
		return;
	}

	CurrentStackCount = FMath::Max(FMath::TruncToInt(Payload->EventMagnitude), 0);
	UpdateFillMeter();
}

UAbilitySystemComponent* UStatusEffectWidget::GetOwnerAbilitySystemComponent() const
{
	return OwnerActor.Get() ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor.Get()) : nullptr;
}

int32 UStatusEffectWidget::GetMaxStackCount() const
{
	return EffectDataAsset ? FMath::Max(EffectDataAsset->MaxStackCount, 0) : 0;
}

float UStatusEffectWidget::GetDebuffStackDuration() const
{
	return EffectDataAsset ? FMath::Max(EffectDataAsset->DebuffStackDuration, 0.0f) : 0.0f;
}

float UStatusEffectWidget::GetStatusDuration() const
{
	return EffectDataAsset ? FMath::Max(EffectDataAsset->StatusDuration, 0.0f) : 0.0f;
}

int32 UStatusEffectWidget::GetActiveDebuffStackCount() const
{
	if (!BoundAbilitySystemComponent || !EffectDataAsset || !EffectDataAsset->DebuffTag.IsValid())
	{
		return 0;
	}

	FGameplayTagContainer DebuffTags;
	DebuffTags.AddTag(EffectDataAsset->DebuffTag);

	int32 StackCount = 0;
	const TArray<FActiveGameplayEffectHandle> ActiveHandles =
		BoundAbilitySystemComponent->GetActiveEffectsWithAllTags(DebuffTags);
	for (const FActiveGameplayEffectHandle& ActiveHandle : ActiveHandles)
	{
		StackCount = FMath::Max(StackCount, BoundAbilitySystemComponent->GetCurrentStackCount(ActiveHandle));
	}

	if (StackCount <= 0 && BoundAbilitySystemComponent->HasMatchingGameplayTag(EffectDataAsset->DebuffTag))
	{
		StackCount = 1;
	}

	return StackCount;
}
