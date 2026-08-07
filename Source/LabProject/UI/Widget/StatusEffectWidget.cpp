#include "UI/Widget/StatusEffectWidget.h"

#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "GameplayEffectTypes.h"
#include "Styling/SlateBrush.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectWidget)

namespace
{
	constexpr float MeterEmptyPercent = 0.0f;
	constexpr float MeterFullPercent = 1.0f;

	void ConfigureIconBrush(FSlateBrush& Brush, UObject* ResourceObject)
	{
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(ResourceObject);
	}
}

void UStatusEffectWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyWidgetDefinitionSettings();
	ApplyDesignerDefaults();
}

void UStatusEffectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	bIsConstructed = true;
	InitializeStatusEffect();
}

void UStatusEffectWidget::NativeDestruct()
{
	bIsConstructed = false;
	ClearStackFillPresentationTimers();
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

void UStatusEffectWidget::SetEffectDataAsset(UStatusEffectDefinition* InEffectDataAsset)
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

UStatusEffectDefinition* UStatusEffectWidget::GetEffectDataAsset() const
{
	return EffectDataAsset.Get();
}

void UStatusEffectWidget::ApplyWidgetDefinitionSettings()
{
	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FStatusEffectsBarWidgetSettings& Settings = WidgetDefinition->GetStatusEffectsBarWidgetSettings();
		MeterUpdateInterval = FMath::Max(Settings.MeterUpdateInterval, 0.001f);
		InitialIconOpacity = FMath::Clamp(Settings.InitialIconOpacity, 0.0f, 1.0f);
	}
}

void UStatusEffectWidget::InitializeStatusEffect()
{
	ClearStackFillPresentationTimers();
	ClearUpdateTimeRemainingTimer();
	UnbindGameplayListeners();

	SetInitialValues();
	SetIconStyle();
	BindGameplayListeners();
	RefreshFromActiveEffects();
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
	bIsStatusEffectApplied = false;

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
		FSlateBrush Brush = EffectIcon->GetBrush();
		ConfigureIconBrush(Brush, EffectDataAsset->Icon);
		EffectIcon->SetBrush(Brush);
	}

	if (EffectFillMeter)
	{
		EffectFillMeter->SetFillColorAndOpacity(EffectDataAsset->IconBackgroundColor);
	}

	if (EffectAppliedTimeLeft)
	{
		EffectAppliedTimeLeft->SetFillColorAndOpacity(EffectDataAsset->IconBackgroundColor);
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
	RestartStackFillPresentation();

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
	float FillPercent = MeterEmptyPercent;
	if (bIsStatusEffectApplied)
	{
		FillPercent = MeterFullPercent;
	}
	else if (MaxStackCount > 0)
	{
		FillPercent = static_cast<float>(CurrentStackCount)
			/ static_cast<float>(MaxStackCount);
	}

	EffectFillMeter->SetPercent(FMath::Clamp(FillPercent, MeterEmptyPercent, MeterFullPercent));
}

void UStatusEffectWidget::RestartStackFillPresentation()
{
	ClearStackFillPresentationTimers();

	UWorld* World = GetWorld();
	if (!World
		|| !EffectFillMeter
		|| bIsStatusEffectApplied
		|| CurrentStackCount <= 0
		|| GetMaxStackCount() <= 0)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		StackFillHoldTimer,
		this,
		&ThisClass::StartStackFillDecrease,
		StatusEffectTiming::StackHoldSeconds,
		false);
}

void UStatusEffectWidget::StartStackFillDecrease()
{
	UWorld* World = GetWorld();
	if (!World
		|| !EffectFillMeter
		|| bIsStatusEffectApplied
		|| CurrentStackCount <= 0
		|| GetMaxStackCount() <= 0)
	{
		return;
	}
	StackFillDecreaseStartTime = World->GetTimeSeconds();
	StackFillDecreaseStartPercent = EffectFillMeter->GetPercent();

	World->GetTimerManager().SetTimer(
		UpdateStackFillTimer,
		this,
		&ThisClass::UpdateStackFillDecrease,
		StatusEffectTiming::StackPresentationUpdateIntervalSeconds,
		true);
}

void UStatusEffectWidget::UpdateStackFillDecrease()
{
	UWorld* World = GetWorld();
	if (!World || !EffectFillMeter || bIsStatusEffectApplied)
	{
		ClearStackFillPresentationTimers();
		return;
	}

	if (StatusEffectTiming::StackDecaySeconds <= 0.0f)
	{
		ClearStackFillPresentationTimers();
		return;
	}

	const double ElapsedSeconds = FMath::Max(
		World->GetTimeSeconds() - StackFillDecreaseStartTime,
		0.0);
	const float NewPercent = FMath::Clamp(
		StackFillDecreaseStartPercent
			- static_cast<float>(ElapsedSeconds
				/ static_cast<double>(StatusEffectTiming::StackDecaySeconds)),
		MeterEmptyPercent,
		MeterFullPercent);
	EffectFillMeter->SetPercent(NewPercent);

	if (NewPercent <= MeterEmptyPercent)
	{
		ClearStackFillPresentationTimers();
	}
}

void UStatusEffectWidget::ClearStackFillPresentationTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StackFillHoldTimer);
		World->GetTimerManager().ClearTimer(UpdateStackFillTimer);
	}

	StackFillHoldTimer.Invalidate();
	UpdateStackFillTimer.Invalidate();
	StackFillDecreaseStartTime = 0.0;
	StackFillDecreaseStartPercent = MeterEmptyPercent;
}

void UStatusEffectWidget::HandleStatusEffectApplied()
{
	ClearStackFillPresentationTimers();
	bIsStatusEffectApplied = true;
	CurrentStackCount = GetMaxStackCount();

	if (EffectIcon)
	{
		EffectIcon->SetRenderOpacity(1.0f);
	}

	UpdateFillMeter();

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
		RemoveStatusEffectWidget();
	}
}

void UStatusEffectWidget::EvaluateRemovalAfterDebuffRemoved()
{
	UAbilitySystemComponent* AbilitySystemComponent = GetOwnerAbilitySystemComponent();
	const bool bHasDebuff = EffectDataAsset
		&& EffectDataAsset->DebuffTag.IsValid()
		&& (BoundStatusEffectReplicationComponent
			? BoundStatusEffectReplicationComponent->GetStatusEffectStackCount(
				EffectDataAsset->DebuffTag) > 0
			: AbilitySystemComponent
				&& AbilitySystemComponent->HasMatchingGameplayTag(
					EffectDataAsset->DebuffTag));
	const bool bHasStatusEffect = bIsStatusEffectApplied
		|| (AbilitySystemComponent
			&& EffectDataAsset
			&& EffectDataAsset->StatusEffectTag.IsValid()
			&& AbilitySystemComponent->HasMatchingGameplayTag(
				EffectDataAsset->StatusEffectTag));
	if (bHasDebuff || bHasStatusEffect)
	{
		return;
	}

	RemoveStatusEffectWidget();
}

void UStatusEffectWidget::RemoveStatusEffectWidget()
{
	ClearStackFillPresentationTimers();
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

	BoundStatusEffectReplicationComponent = OwnerActor
		? OwnerActor->FindComponentByClass<UStatusEffectReplicationComponent>()
		: nullptr;
	if (BoundStatusEffectReplicationComponent)
	{
		ReplicatedStackChangedHandle = BoundStatusEffectReplicationComponent
			->OnStatusEffectStackChanged()
			.AddUObject(
				this,
				&ThisClass::OnReplicatedStatusEffectStackChanged);
	}
}

void UStatusEffectWidget::UnbindGameplayListeners()
{
	if (!BoundAbilitySystemComponent)
	{
		if (BoundStatusEffectReplicationComponent
			&& ReplicatedStackChangedHandle.IsValid())
		{
			BoundStatusEffectReplicationComponent
				->OnStatusEffectStackChanged()
				.Remove(ReplicatedStackChangedHandle);
		}
		BoundStatusEffectReplicationComponent = nullptr;
		DebuffTagChangedHandle.Reset();
		StatusEffectTagChangedHandle.Reset();
		ReplicatedStackChangedHandle.Reset();
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

	if (BoundStatusEffectReplicationComponent
		&& ReplicatedStackChangedHandle.IsValid())
	{
		BoundStatusEffectReplicationComponent
			->OnStatusEffectStackChanged()
			.Remove(ReplicatedStackChangedHandle);
	}

	BoundAbilitySystemComponent = nullptr;
	BoundStatusEffectReplicationComponent = nullptr;
	DebuffTagChangedHandle.Reset();
	StatusEffectTagChangedHandle.Reset();
	ReplicatedStackChangedHandle.Reset();
	BoundDebuffTag = FGameplayTag();
	BoundStatusEffectTag = FGameplayTag();
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
		CurrentStackCount = FMath::Max(GetActiveDebuffStackCount(), 1);
		UpdateFillMeter();
		RestartStackFillPresentation();
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

	if (bIsStatusEffectApplied)
	{
		if (EffectAppliedTimeLeft)
		{
			EffectAppliedTimeLeft->SetPercent(MeterEmptyPercent);
		}
		RemoveStatusEffectWidget();
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

void UStatusEffectWidget::OnReplicatedStatusEffectStackChanged(
	const FGameplayTag DebuffTag,
	const int32 StackCount)
{
	if (!EffectDataAsset
		|| !DebuffTag.MatchesTagExact(EffectDataAsset->DebuffTag))
	{
		return;
	}

	const int32 PreviousStackCount = CurrentStackCount;
	CurrentStackCount = FMath::Max(StackCount, 0);
	if (CurrentStackCount > PreviousStackCount)
	{
		UpdateFillMeter();
		RestartStackFillPresentation();
	}
	if (CurrentStackCount <= 0 && !bIsStatusEffectApplied)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(
					this,
					&ThisClass::EvaluateRemovalAfterDebuffRemoved));
			return;
		}

		EvaluateRemovalAfterDebuffRemoved();
	}
}

UAbilitySystemComponent* UStatusEffectWidget::GetOwnerAbilitySystemComponent() const
{
	return OwnerActor.Get() ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor.Get()) : nullptr;
}

int32 UStatusEffectWidget::GetMaxStackCount() const
{
	return EffectDataAsset ? FMath::Max(EffectDataAsset->MaxStackCount, 0) : 0;
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

	if (BoundStatusEffectReplicationComponent)
	{
		return BoundStatusEffectReplicationComponent->GetStatusEffectStackCount(
			EffectDataAsset->DebuffTag);
	}

	int32 StackCount = 0;
	const TArray<FActiveGameplayEffectHandle> ActiveHandles =
		BoundAbilitySystemComponent->GetActiveEffectsWithAllTags(DebuffTags);
	for (const FActiveGameplayEffectHandle& ActiveHandle : ActiveHandles)
	{
		StackCount += FMath::Max(
			BoundAbilitySystemComponent->GetCurrentStackCount(ActiveHandle),
			1);
	}

	if (StackCount <= 0 && BoundAbilitySystemComponent->HasMatchingGameplayTag(EffectDataAsset->DebuffTag))
	{
		StackCount = 1;
	}

	return StackCount;
}
