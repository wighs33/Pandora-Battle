#include "UI/Widget/StatusEffectsBarWidget.h"

#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
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
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	TArray<FSoftObjectPath> StatusEffectPaths;
	for (const TSoftObjectPtr<UStatusEffectDefinition>& StatusEffect :
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().StatusEffects)
	{
		StatusEffectPaths.Add(StatusEffect.ToSoftObjectPath());
	}

	StatusEffectContentPreloadHandle = ContentSubsystem->PreloadSoftObjectPathsAsync(
		StatusEffectPaths,
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
	if (StatusEffectContentPreloadHandle.IsValid())
	{
		StatusEffectContentPreloadHandle->CancelHandle();
		StatusEffectContentPreloadHandle->ReleaseHandle();
		StatusEffectContentPreloadHandle.Reset();
	}
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

	UnbindStatusEffectTagDelegates();
	OwnerActor = InOwnerActor;
	BindRetryCount = 0;

	if (bIsConstructed)
	{
		ScheduleStatusEffectTagBinding();
	}
}

void UStatusEffectsBarWidget::CenterHorizontalBox()
{
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	if (!CanvasSlot)
	{
		return;
	}

	FAnchorData Layout = CanvasSlot->GetLayout();
	const float Width = CanvasSlot->GetSize().X;
	Layout.Anchors.Minimum.X = 0.5f;
	Layout.Anchors.Maximum.X = 0.5f;
	Layout.Alignment.X = 0.5f;
	Layout.Offsets.Left = 0.0f;
	Layout.Offsets.Right = Width;
	CanvasSlot->SetLayout(Layout);
}

void UStatusEffectsBarWidget::ApplyWidgetDefinitionSettings()
{
	InvalidateObservedStatusEffectDataAssetCache();

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		if (const TSubclassOf<UStatusEffectWidget> ResolvedStatusEffectWidgetClass =
			WidgetDefinition->GetStatusEffectWidgetClass())
		{
			StatusEffectWidgetClass = TSubclassOf<UUserWidget>(ResolvedStatusEffectWidgetClass.Get());
		}
	}
}

void UStatusEffectsBarWidget::TryAddStatusEffectWidget(UStatusEffectDefinition* DataAsset)
{
	if (!HorizontalBox || !DataAsset)
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

	HorizontalBox->AddChildToHorizontalBox(StatusEffectWidget);
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
	BoundStatusEffectReplicationComponent = OwnerActor
		? OwnerActor->FindComponentByClass<UStatusEffectReplicationComponent>()
		: nullptr;
	if (BoundStatusEffectReplicationComponent)
	{
		ReplicatedStackChangedHandle = BoundStatusEffectReplicationComponent
			->OnStatusEffectStackChanged()
			.AddUObject(
				this,
				&ThisClass::HandleReplicatedStatusEffectStackChanged);
	}

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
	if (BoundStatusEffectReplicationComponent
		&& ReplicatedStackChangedHandle.IsValid())
	{
		BoundStatusEffectReplicationComponent
			->OnStatusEffectStackChanged()
			.Remove(ReplicatedStackChangedHandle);
	}
	BoundStatusEffectReplicationComponent = nullptr;
	ReplicatedStackChangedHandle.Reset();

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

void UStatusEffectsBarWidget::HandleReplicatedStatusEffectStackChanged(
	const FGameplayTag DebuffTag,
	const int32 StackCount)
{
	static_cast<void>(DebuffTag);
	static_cast<void>(StackCount);
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

	TArray<UStatusEffectDefinition*> ObservedDataAssets;
	GatherObservedStatusEffectDataAssets(ObservedDataAssets);

	TMap<const UStatusEffectDefinition*, int32> DesiredWidgetCounts;
	for (const UStatusEffectDefinition* DataAsset : ObservedDataAssets)
	{
		if (DataAsset)
		{
			DesiredWidgetCounts.Add(DataAsset, GetStatusEffectDisplayCount(DataAsset));
		}
	}

	TMap<const UStatusEffectDefinition*, int32> ExistingWidgetCounts;
	for (int32 ChildIndex = HorizontalBox->GetChildrenCount() - 1; ChildIndex >= 0; --ChildIndex)
	{
		UWidget* ChildWidget = HorizontalBox->GetChildAt(ChildIndex);
		const UStatusEffectDefinition* ChildDataAsset = GetStatusEffectDataAssetFromWidget(ChildWidget);
		const int32* DesiredWidgetCount = DesiredWidgetCounts.Find(ChildDataAsset);
		int32& ExistingWidgetCount = ExistingWidgetCounts.FindOrAdd(ChildDataAsset);
		if (!DesiredWidgetCount || ExistingWidgetCount >= *DesiredWidgetCount)
		{
			HorizontalBox->RemoveChildAt(ChildIndex);
			continue;
		}

		++ExistingWidgetCount;
	}

	for (UStatusEffectDefinition* DataAsset : ObservedDataAssets)
	{
		if (!DataAsset)
		{
			continue;
		}

		const int32 DesiredWidgetCount = DesiredWidgetCounts.FindRef(DataAsset);
		const int32 ExistingWidgetCount = ExistingWidgetCounts.FindRef(DataAsset);
		for (int32 WidgetIndex = ExistingWidgetCount;
			WidgetIndex < DesiredWidgetCount;
			++WidgetIndex)
		{
			TryAddStatusEffectWidget(DataAsset);
		}
	}
}

int32 UStatusEffectsBarWidget::GetStatusEffectDisplayCount(
	const UStatusEffectDefinition* DataAsset) const
{
	if (!BoundAbilitySystemComponent || !DataAsset)
	{
		return 0;
	}

	int32 StackCount = 0;
	if (DataAsset->DebuffTag.IsValid())
	{
		if (BoundStatusEffectReplicationComponent)
		{
			StackCount = BoundStatusEffectReplicationComponent
				->GetStatusEffectStackCount(DataAsset->DebuffTag);
		}
		else
		{
			FGameplayTagContainer DebuffTags;
			DebuffTags.AddTag(DataAsset->DebuffTag);
			const TArray<FActiveGameplayEffectHandle> ActiveHandles =
				BoundAbilitySystemComponent->GetActiveEffectsWithAllTags(DebuffTags);
			for (const FActiveGameplayEffectHandle ActiveHandle : ActiveHandles)
			{
				StackCount += FMath::Max(
					BoundAbilitySystemComponent->GetCurrentStackCount(ActiveHandle),
					1);
			}

			if (StackCount <= 0
				&& BoundAbilitySystemComponent->HasMatchingGameplayTag(DataAsset->DebuffTag))
			{
				StackCount = 1;
			}
		}
	}

	if (DataAsset->StatusEffectTag.IsValid()
		&& BoundAbilitySystemComponent->HasMatchingGameplayTag(DataAsset->StatusEffectTag))
	{
		StackCount = FMath::Max(StackCount, 1);
	}

	return StackCount > 0 ? 1 : 0;
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

	for (const TSoftObjectPtr<UStatusEffectDefinition>& StatusEffect :
		UPdGameInstanceDefinition::GetConfiguredDefinitionReferences().StatusEffects)
	{
		AddObservedStatusEffectDataAsset(StatusEffect.Get());
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
