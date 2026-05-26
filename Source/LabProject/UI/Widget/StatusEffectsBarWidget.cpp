#include "UI/Widget/StatusEffectsBarWidget.h"

#include "AbilitySystem/Data/PdStatusEffectDataAsset.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "UI/Widget/StatusEffectWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectsBarWidget)

namespace
{
	bool SetStatusEffectObjectPropertyValue(UObject* Object, const FName PropertyName, UObject* Value)
	{
		if (!IsValid(Object))
		{
			return false;
		}

		FObjectPropertyBase* ObjectProperty = FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName);
		if (!ObjectProperty)
		{
			return false;
		}

		if (Value && ObjectProperty->PropertyClass && !Value->IsA(ObjectProperty->PropertyClass))
		{
			return false;
		}

		ObjectProperty->SetObjectPropertyValue_InContainer(Object, Value);
		return true;
	}

	UPdStatusEffectDataAsset* GetStatusEffectDataAssetFromWidget(const UWidget* Widget)
	{
		if (!IsValid(Widget))
		{
			return nullptr;
		}

		if (const UStatusEffectWidget* StatusEffectWidget = Cast<UStatusEffectWidget>(Widget))
		{
			return StatusEffectWidget->GetEffectDataAsset();
		}

		const FObjectPropertyBase* ObjectProperty =
			FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("EffectDataAsset"));
		return ObjectProperty
			? Cast<UPdStatusEffectDataAsset>(ObjectProperty->GetObjectPropertyValue_InContainer(Widget))
			: nullptr;
	}
}

UStatusEffectsBarWidget::UStatusEffectsBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FClassFinder<UUserWidget> StatusEffectWidgetFinder(
		TEXT("/Game/UI/Widget/WBP_StatusEffect"));
	if (StatusEffectWidgetFinder.Succeeded())
	{
		StatusEffectWidgetClass = StatusEffectWidgetFinder.Class;
	}

	static ConstructorHelpers::FObjectFinder<UPdStatusEffectDataAsset> BurnStatusEffectDataAssetFinder(
		TEXT("/Game/StatusEffects/DA_StatusEffect_Burn.DA_StatusEffect_Burn"));
	if (BurnStatusEffectDataAssetFinder.Succeeded())
	{
		StatusEffectDataAssets.Add(BurnStatusEffectDataAssetFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UPdStatusEffectDataAsset> FreezeStatusEffectDataAssetFinder(
		TEXT("/Game/StatusEffects/DA_StatusEffect_Freeze.DA_StatusEffect_Freeze"));
	if (FreezeStatusEffectDataAssetFinder.Succeeded())
	{
		StatusEffectDataAssets.Add(FreezeStatusEffectDataAssetFinder.Object);
	}
}

void UStatusEffectsBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bIsConstructed = true;
	ScheduleStatusEffectTagBinding();
}

void UStatusEffectsBarWidget::NativeDestruct()
{
	bIsConstructed = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindStatusEffectTagsTimerHandle);
	}

	bBindStatusEffectTagsScheduled = false;
	BindStatusEffectTagsTimerHandle.Invalidate();
	UnbindStatusEffectTagDelegates();

	Super::NativeDestruct();
}

void UStatusEffectsBarWidget::SetOwnerActor(AActor* InOwnerActor)
{
	if (OwnerActor.Get() == InOwnerActor)
	{
		return;
	}

	OwnerActor = InOwnerActor;
	BindRetryCount = 0;

	if (bIsConstructed)
	{
		ScheduleStatusEffectTagBinding();
	}
}

void UStatusEffectsBarWidget::TryAddStatusEffectWidget(UPdStatusEffectDataAsset* DataAsset)
{
	if (!HorizontalBox || !DataAsset || AlreadyDisplayingStatusEffect(DataAsset->DebuffTag))
	{
		return;
	}

	UUserWidget* StatusEffectWidget = CreateStatusEffectWidget();
	if (!StatusEffectWidget)
	{
		return;
	}

	if (UStatusEffectWidget* NativeStatusEffectWidget = Cast<UStatusEffectWidget>(StatusEffectWidget))
	{
		NativeStatusEffectWidget->SetOwnerActor(OwnerActor.Get());
		NativeStatusEffectWidget->SetEffectDataAsset(DataAsset);
	}
	else
	{
		SetStatusEffectObjectPropertyValue(StatusEffectWidget, TEXT("OwnerActor"), OwnerActor.Get());
		SetStatusEffectObjectPropertyValue(StatusEffectWidget, TEXT("EffectDataAsset"), DataAsset);
	}

	UHorizontalBoxSlot* HorizontalBoxSlot = HorizontalBox->AddChildToHorizontalBox(StatusEffectWidget);
	if (HorizontalBoxSlot)
	{
		HorizontalBoxSlot->SetPadding(StatusEffectWidgetPadding);
	}
}

bool UStatusEffectsBarWidget::AlreadyDisplayingStatusEffect(const FGameplayTag DebuffTag) const
{
	if (!HorizontalBox || !DebuffTag.IsValid())
	{
		return false;
	}

	const int32 ChildrenCount = HorizontalBox->GetChildrenCount();
	for (int32 ChildIndex = 0; ChildIndex < ChildrenCount; ++ChildIndex)
	{
		const UWidget* ChildWidget = HorizontalBox->GetChildAt(ChildIndex);
		const UPdStatusEffectDataAsset* ChildDataAsset = GetStatusEffectDataAssetFromWidget(ChildWidget);
		if (ChildDataAsset && ChildDataAsset->DebuffTag == DebuffTag)
		{
			return true;
		}
	}

	return false;
}

void UStatusEffectsBarWidget::ScheduleStatusEffectTagBinding()
{
	if (bBindStatusEffectTagsScheduled)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		bBindStatusEffectTagsScheduled = true;
		BindStatusEffectTagsTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::BindStatusEffectTagDelegates));
		return;
	}

	BindStatusEffectTagDelegates();
}

void UStatusEffectsBarWidget::BindStatusEffectTagDelegates()
{
	bBindStatusEffectTagsScheduled = false;
	BindStatusEffectTagsTimerHandle.Invalidate();
	UnbindStatusEffectTagDelegates();

	BoundAbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!BoundAbilitySystemComponent)
	{
		if (UWorld* World = GetWorld(); World && BindRetryCount < MaxBindRetryCount)
		{
			++BindRetryCount;
			World->GetTimerManager().SetTimer(
				BindStatusEffectTagsTimerHandle,
				this,
				&ThisClass::BindStatusEffectTagDelegates,
				0.1f,
				false);
		}
		return;
	}

	BindRetryCount = 0;

	for (UPdStatusEffectDataAsset* DataAsset : StatusEffectDataAssets)
	{
		if (!DataAsset)
		{
			continue;
		}

		if (DataAsset->DebuffTag.IsValid() && !ObservedTagChangedHandles.Contains(DataAsset->DebuffTag))
		{
			ObservedTagChangedHandles.Add(
				DataAsset->DebuffTag,
				BoundAbilitySystemComponent
					->RegisterGameplayTagEvent(DataAsset->DebuffTag, EGameplayTagEventType::NewOrRemoved)
					.AddUObject(this, &ThisClass::HandleObservedTagChanged));
		}

		if (DataAsset->StatusEffectTag.IsValid() && !ObservedTagChangedHandles.Contains(DataAsset->StatusEffectTag))
		{
			ObservedTagChangedHandles.Add(
				DataAsset->StatusEffectTag,
				BoundAbilitySystemComponent
					->RegisterGameplayTagEvent(DataAsset->StatusEffectTag, EGameplayTagEventType::NewOrRemoved)
					.AddUObject(this, &ThisClass::HandleObservedTagChanged));
		}
	}

	AddWidgetsForExistingTags();
}

void UStatusEffectsBarWidget::UnbindStatusEffectTagDelegates()
{
	if (!BoundAbilitySystemComponent)
	{
		ObservedTagChangedHandles.Reset();
		return;
	}

	for (const TPair<FGameplayTag, FDelegateHandle>& ObservedTagChangedHandle : ObservedTagChangedHandles)
	{
		if (ObservedTagChangedHandle.Key.IsValid() && ObservedTagChangedHandle.Value.IsValid())
		{
			BoundAbilitySystemComponent
				->RegisterGameplayTagEvent(ObservedTagChangedHandle.Key, EGameplayTagEventType::NewOrRemoved)
				.Remove(ObservedTagChangedHandle.Value);
		}
	}

	ObservedTagChangedHandles.Reset();
	BoundAbilitySystemComponent = nullptr;
}

void UStatusEffectsBarWidget::HandleObservedTagChanged(const FGameplayTag CallbackTag, const int32 NewCount)
{
	if (NewCount <= 0)
	{
		return;
	}

	TryAddStatusEffectWidget(FindDataAssetForObservedTag(CallbackTag));
}

void UStatusEffectsBarWidget::AddWidgetsForExistingTags()
{
	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	for (UPdStatusEffectDataAsset* DataAsset : StatusEffectDataAssets)
	{
		if (!DataAsset)
		{
			continue;
		}

		const bool bHasDebuffTag = DataAsset->DebuffTag.IsValid()
			&& BoundAbilitySystemComponent->HasMatchingGameplayTag(DataAsset->DebuffTag);
		const bool bHasStatusEffectTag = DataAsset->StatusEffectTag.IsValid()
			&& BoundAbilitySystemComponent->HasMatchingGameplayTag(DataAsset->StatusEffectTag);

		if (bHasDebuffTag || bHasStatusEffectTag)
		{
			TryAddStatusEffectWidget(DataAsset);
		}
	}
}

UUserWidget* UStatusEffectsBarWidget::CreateStatusEffectWidget() const
{
	if (!StatusEffectWidgetClass)
	{
		return nullptr;
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		return CreateWidget<UUserWidget>(OwningPlayer, StatusEffectWidgetClass);
	}

	UWorld* World = GetWorld();
	return World ? CreateWidget<UUserWidget>(World, StatusEffectWidgetClass) : nullptr;
}

UAbilitySystemComponent* UStatusEffectsBarWidget::GetOwnerAbilitySystemComponent() const
{
	return OwnerActor.Get() ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor.Get()) : nullptr;
}

UPdStatusEffectDataAsset* UStatusEffectsBarWidget::FindDataAssetForObservedTag(const FGameplayTag Tag) const
{
	if (!Tag.IsValid())
	{
		return nullptr;
	}

	for (UPdStatusEffectDataAsset* DataAsset : StatusEffectDataAssets)
	{
		if (!DataAsset)
		{
			continue;
		}

		if (DataAsset->DebuffTag == Tag || DataAsset->StatusEffectTag == Tag)
		{
			return DataAsset;
		}
	}

	return nullptr;
}
