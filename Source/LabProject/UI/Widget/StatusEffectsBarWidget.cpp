#include "UI/Widget/StatusEffectsBarWidget.h"

#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/GameInstance.h"
#include "Settings/GameSettingsSubsystem.h"
#include "UI/Widget/StatusEffectWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"
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

	UStatusEffectDefinition* GetStatusEffectDataAssetFromWidget(const UWidget* Widget)
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
			? Cast<UStatusEffectDefinition>(ObjectProperty->GetObjectPropertyValue_InContainer(Widget))
			: nullptr;
	}
}

UStatusEffectsBarWidget::UStatusEffectsBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UStatusEffectsBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	ResolveStatusEffectWidgetClass();
	bIsConstructed = true;
	BeginStatusEffectContentPreload();
	ScheduleStatusEffectTagBinding();
}

void UStatusEffectsBarWidget::NativeDestruct()
{
	bIsConstructed = false;
	ReleaseStatusEffectContentPreload();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindStatusEffectTagsTimerHandle);
		World->GetTimerManager().ClearTimer(RefreshStatusEffectWidgetsTimerHandle);
	}

	bBindStatusEffectTagsScheduled = false;
	bStatusEffectWidgetRefreshScheduled = false;
	BindStatusEffectTagsTimerHandle.Invalidate();
	RefreshStatusEffectWidgetsTimerHandle.Invalidate();
	UnbindStatusEffectTagDelegates();
	InvalidateObservedStatusEffectDataAssetCache();

	Super::NativeDestruct();
}

void UStatusEffectsBarWidget::BeginStatusEffectContentPreload()
{
	ReleaseStatusEffectContentPreload();
	const int32 PreloadGeneration = ++ContentPreloadGeneration;

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
				if (PreloadGeneration != ContentPreloadGeneration || !bIsConstructed)
				{
					return;
				}

				InvalidateObservedStatusEffectDataAssetCache();
				ScheduleStatusEffectTagBinding();
				ScheduleStatusEffectWidgetRefresh();
			}));
}

void UStatusEffectsBarWidget::ReleaseStatusEffectContentPreload()
{
	++ContentPreloadGeneration;
}

void UStatusEffectsBarWidget::SetOwnerActor(AActor* InOwnerActor)
{
	if (OwnerActor.Get() == InOwnerActor)
	{
		if (bIsConstructed)
		{
			ScheduleStatusEffectWidgetRefresh();
		}
		return;
	}

	if (HorizontalBox)
	{
		HorizontalBox->ClearChildren();
	}

	OwnerActor = InOwnerActor;
	BindRetryCount = 0;

	if (bIsConstructed)
	{
		ScheduleStatusEffectTagBinding();
	}
}

void UStatusEffectsBarWidget::ApplyWidgetDefinitionSettings()
{
	InvalidateObservedStatusEffectDataAssetCache();

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		const FStatusEffectsBarWidgetSettings& Settings = WidgetDefinition->GetStatusEffectsBarWidgetSettings();
		if (const TSubclassOf<UStatusEffectWidget> ResolvedStatusEffectWidgetClass =
			WidgetDefinition->GetStatusEffectWidgetClass())
		{
			StatusEffectWidgetClass = TSubclassOf<UUserWidget>(ResolvedStatusEffectWidgetClass.Get());
		}
		StatusEffectWidgetPadding = Settings.StatusEffectWidgetPadding;
	}
}

void UStatusEffectsBarWidget::TryAddStatusEffectWidget(UStatusEffectDefinition* DataAsset)
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

	StatusEffectWidget->SetRenderTranslation(FVector2D(0.0f, StatusEffectWidgetVerticalOffset));
	const float SafeScale = FMath::Max(StatusEffectWidgetScale, 0.01f);
	StatusEffectWidget->SetRenderScale(FVector2D(SafeScale, SafeScale));
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
		const UStatusEffectDefinition* ChildDataAsset = GetStatusEffectDataAssetFromWidget(ChildWidget);
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

void UStatusEffectsBarWidget::ScheduleStatusEffectWidgetRefresh()
{
	if (bStatusEffectWidgetRefreshScheduled)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		bStatusEffectWidgetRefreshScheduled = true;
		RefreshStatusEffectWidgetsTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &ThisClass::RefreshStatusEffectWidgets));
		return;
	}

	RefreshStatusEffectWidgets();
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

	TArray<UStatusEffectDefinition*> ObservedDataAssets;
	GatherObservedStatusEffectDataAssets(ObservedDataAssets);
	for (UStatusEffectDefinition* DataAsset : ObservedDataAssets)
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

	RefreshStatusEffectWidgets();
}

void UStatusEffectsBarWidget::UnbindStatusEffectTagDelegates()
{
	if (!BoundAbilitySystemComponent)
	{
		ObservedTagChangedHandles.Reset();
		return;
	}

	const TMap<FGameplayTag, FDelegateHandle> ObservedTagChangedHandlesSnapshot = ObservedTagChangedHandles;
	for (const TPair<FGameplayTag, FDelegateHandle>& ObservedTagChangedHandle : ObservedTagChangedHandlesSnapshot)
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
	static_cast<void>(CallbackTag);
	static_cast<void>(NewCount);
	ScheduleStatusEffectWidgetRefresh();
}

void UStatusEffectsBarWidget::RefreshStatusEffectWidgets()
{
	bStatusEffectWidgetRefreshScheduled = false;
	RefreshStatusEffectWidgetsTimerHandle.Invalidate();

	if (!HorizontalBox)
	{
		return;
	}

	for (int32 ChildIndex = HorizontalBox->GetChildrenCount() - 1; ChildIndex >= 0; --ChildIndex)
	{
		UWidget* ChildWidget = HorizontalBox->GetChildAt(ChildIndex);
		const UStatusEffectDefinition* ChildDataAsset = GetStatusEffectDataAssetFromWidget(ChildWidget);
		if (!IsStatusEffectActive(ChildDataAsset))
		{
			HorizontalBox->RemoveChildAt(ChildIndex);
		}
	}

	AddWidgetsForExistingTags();
}

void UStatusEffectsBarWidget::AddWidgetsForExistingTags()
{
	if (!BoundAbilitySystemComponent)
	{
		return;
	}

	TArray<UStatusEffectDefinition*> ObservedDataAssets;
	GatherObservedStatusEffectDataAssets(ObservedDataAssets);
	for (UStatusEffectDefinition* DataAsset : ObservedDataAssets)
	{
		if (!DataAsset)
		{
			continue;
		}

		if (IsStatusEffectActive(DataAsset))
		{
			TryAddStatusEffectWidget(DataAsset);
		}
	}
}

bool UStatusEffectsBarWidget::IsStatusEffectActive(const UStatusEffectDefinition* DataAsset) const
{
	if (!BoundAbilitySystemComponent || !DataAsset)
	{
		return false;
	}

	const bool bHasDebuffTag = DataAsset->DebuffTag.IsValid()
		&& BoundAbilitySystemComponent->HasMatchingGameplayTag(DataAsset->DebuffTag);
	const bool bHasStatusEffectTag = DataAsset->StatusEffectTag.IsValid()
		&& BoundAbilitySystemComponent->HasMatchingGameplayTag(DataAsset->StatusEffectTag);
	return bHasDebuffTag || bHasStatusEffectTag;
}

UUserWidget* UStatusEffectsBarWidget::CreateStatusEffectWidget()
{
	ResolveStatusEffectWidgetClass();

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

void UStatusEffectsBarWidget::ResolveStatusEffectWidgetClass()
{
	if (StatusEffectWidgetClass)
	{
		return;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		StatusEffectWidgetClass = TSubclassOf<UUserWidget>(WidgetDefinition->GetStatusEffectWidgetClass().Get());
	}
}

UAbilitySystemComponent* UStatusEffectsBarWidget::GetOwnerAbilitySystemComponent() const
{
	return OwnerActor.Get() ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor.Get()) : nullptr;
}

void UStatusEffectsBarWidget::GatherObservedStatusEffectDataAssets(
	TArray<UStatusEffectDefinition*>& OutDataAssets)
{
	if (!bObservedStatusEffectDataAssetCacheValid)
	{
		RebuildObservedStatusEffectDataAssetCache();
	}

	for (UStatusEffectDefinition* DataAsset : CachedObservedStatusEffectDataAssets)
	{
		if (DataAsset)
		{
			OutDataAssets.Add(DataAsset);
		}
	}
}

void UStatusEffectsBarWidget::RebuildObservedStatusEffectDataAssetCache()
{
	CachedObservedStatusEffectDataAssets.Reset();

	for (UStatusEffectDefinition* DataAsset : StatusEffectDataAssets)
	{
		AddObservedStatusEffectDataAsset(DataAsset);
	}

	if (const UGameSettingDefinition* GameSettingDefinition =
		UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(this))
	{
		for (const TSoftObjectPtr<UStatusEffectDefinition>& StatusEffectDataAsset : GameSettingDefinition->StatusEffectDataAssets)
		{
			AddObservedStatusEffectDataAsset(StatusEffectDataAsset.Get());
		}
	}

	bObservedStatusEffectDataAssetCacheValid = true;
}

void UStatusEffectsBarWidget::AddObservedStatusEffectDataAsset(UStatusEffectDefinition* DataAsset)
{
	if (DataAsset)
	{
		CachedObservedStatusEffectDataAssets.AddUnique(DataAsset);
	}
}

void UStatusEffectsBarWidget::InvalidateObservedStatusEffectDataAssetCache()
{
	CachedObservedStatusEffectDataAssets.Reset();
	bObservedStatusEffectDataAssetCacheValid = false;
}
