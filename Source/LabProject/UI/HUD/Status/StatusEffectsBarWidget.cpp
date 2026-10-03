#include "UI/HUD/Status/StatusEffectsBarWidget.h"

#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/CharacterBase.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Engine/GameInstance.h"
#include "UI/HUD/Status/StatusEffectWidget.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectsBarWidget)

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
	ObserveOwnerAbilitySystem();
	BindStatusEffectTagDelegates();
}

void UStatusEffectsBarWidget::NativeDestruct()
{
	bIsConstructed = false;
	OwnerReadySubscription.Reset();
	StatusEffectContentLease.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshStatusEffectWidgetsTimerHandle);
	}

	bStatusEffectWidgetRefreshScheduled = false;
	RefreshStatusEffectWidgetsTimerHandle.Invalidate();
	UnbindStatusEffectTagDelegates();
	InvalidateObservedStatusEffectDataAssetCache();

	Super::NativeDestruct();
}

void UStatusEffectsBarWidget::BeginStatusEffectContentPreload()
{
	StatusEffectContentLease.Reset();

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

	StatusEffectContentLease = ContentSubsystem->AcquireContent(
		StatusEffectPaths,
		FSimpleDelegate::CreateWeakLambda(
			this,
			[this]()
			{
				if (!bIsConstructed)
				{
					return;
				}

				InvalidateObservedStatusEffectDataAssetCache();
				BindStatusEffectTagDelegates();
				ScheduleStatusEffectWidgetRefresh();
			}));
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

	if (bIsConstructed)
	{
		ObserveOwnerAbilitySystem();
		BindStatusEffectTagDelegates();
	}
}

// 캐릭터 소유자의 ASC가 준비될 때마다(리스폰 포함) 태그를 다시 구독한다.
void UStatusEffectsBarWidget::ObserveOwnerAbilitySystem()
{
	OwnerReadySubscription.SubscribeToCharacter(Cast<ACharacterBase>(OwnerActor.Get()),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleOwnerAbilitySystemReady));
}

void UStatusEffectsBarWidget::HandleOwnerAbilitySystemReady(
	ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent)
{
	static_cast<void>(Character);
	static_cast<void>(AbilitySystemComponent);
	BindStatusEffectTagDelegates();
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
			StatusEffectWidgetClass = ResolvedStatusEffectWidgetClass;
		}
	}
}

void UStatusEffectsBarWidget::TryAddStatusEffectWidget(UStatusEffectDefinition* DataAsset)
{
	if (!HorizontalBox || !DataAsset)
	{
		return;
	}

	UStatusEffectWidget* StatusEffectWidget = CreateStatusEffectWidget();
	if (!StatusEffectWidget)
	{
		return;
	}

	StatusEffectWidget->SetOwnerActor(OwnerActor.Get());
	StatusEffectWidget->SetEffectDataAsset(DataAsset);

	HorizontalBox->AddChildToHorizontalBox(StatusEffectWidget);
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

// 생성·소유자 변경·ASC 준비·설정 로딩 완료 때마다 부른다. 기존 구독을 먼저 지우므로 여러 번 불러도 같다.
void UStatusEffectsBarWidget::BindStatusEffectTagDelegates()
{
	UnbindStatusEffectTagDelegates();

	// 소유 캐릭터의 ASC가 아직이면 HandleOwnerAbilitySystemReady가 준비될 때 다시 부른다.
	BoundAbilitySystemComponent = GetOwnerAbilitySystemComponent();
	if (!BoundAbilitySystemComponent)
	{
		return;
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

		if (DataAsset->StackTag.IsValid() && !ObservedTagChangedHandles.Contains(DataAsset->StackTag))
		{
			ObservedTagChangedHandles.Add(
				DataAsset->StackTag,
				BoundAbilitySystemComponent
					->RegisterGameplayTagEvent(DataAsset->StackTag, EGameplayTagEventType::NewOrRemoved)
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
		const UStatusEffectWidget* ChildWidget = Cast<UStatusEffectWidget>(HorizontalBox->GetChildAt(ChildIndex));
		const UStatusEffectDefinition* ChildDataAsset = ChildWidget ? ChildWidget->GetEffectDataAsset() : nullptr;
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
	if (DataAsset->StackTag.IsValid())
	{
		if (BoundStatusEffectReplicationComponent)
		{
			StackCount = BoundStatusEffectReplicationComponent
				->GetStatusEffectStackCount(DataAsset->StackTag);
		}
		else
		{
			FGameplayTagContainer DebuffTags;
			DebuffTags.AddTag(DataAsset->StackTag);
			const TArray<FActiveGameplayEffectHandle> ActiveHandles =
				BoundAbilitySystemComponent->GetActiveEffectsWithAllTags(DebuffTags);
			for (const FActiveGameplayEffectHandle ActiveHandle : ActiveHandles)
			{
				StackCount += FMath::Max(
					BoundAbilitySystemComponent->GetCurrentStackCount(ActiveHandle),
					1);
			}

			if (StackCount <= 0
				&& BoundAbilitySystemComponent->HasMatchingGameplayTag(DataAsset->StackTag))
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

UStatusEffectWidget* UStatusEffectsBarWidget::CreateStatusEffectWidget()
{
	ResolveStatusEffectWidgetClass();

	if (!StatusEffectWidgetClass)
	{
		return nullptr;
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		return CreateWidget<UStatusEffectWidget>(OwningPlayer, StatusEffectWidgetClass);
	}

	UWorld* World = GetWorld();
	return World ? CreateWidget<UStatusEffectWidget>(World, StatusEffectWidgetClass) : nullptr;
}

void UStatusEffectsBarWidget::ResolveStatusEffectWidgetClass()
{
	if (StatusEffectWidgetClass)
	{
		return;
	}

	if (const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this))
	{
		StatusEffectWidgetClass = WidgetDefinition->GetStatusEffectWidgetClass();
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
