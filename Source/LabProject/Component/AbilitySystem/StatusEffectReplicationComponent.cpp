#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "GameplayEffect.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectReplicationComponent)

void FReplicatedStatusEffectStackEntry::PostReplicatedAdd(
	const FReplicatedStatusEffectStackList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedStackAddedOrChanged(*this);
	}
}

void FReplicatedStatusEffectStackEntry::PostReplicatedChange(
	const FReplicatedStatusEffectStackList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedStackAddedOrChanged(*this);
	}
}

void FReplicatedStatusEffectStackEntry::PreReplicatedRemove(
	const FReplicatedStatusEffectStackList& InArraySerializer)
{
	if (InArraySerializer.Owner)
	{
		InArraySerializer.Owner->HandleReplicatedStackRemoved(DebuffTag);
	}
}

UStatusEffectReplicationComponent::UStatusEffectReplicationComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ReplicatedStacks.Owner = this;
}

void UStatusEffectReplicationComponent::BeginPlay()
{
	Super::BeginPlay();
	ReplicatedStacks.Owner = this;
}

void UStatusEffectReplicationComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	UnbindAbilitySystem();
	ReplicatedStacks.Owner = nullptr;
	StatusEffectStackChanged.Clear();

	Super::EndPlay(EndPlayReason);
}

void UStatusEffectReplicationComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UStatusEffectReplicationComponent,
		ReplicatedStacks,
		Params);
}

void UStatusEffectReplicationComponent::SetStatusEffectStackCount(
	const FGameplayTag DebuffTag,
	const int32 StackCount,
	const FActiveGameplayEffectHandle ActiveEffectHandle)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !DebuffTag.IsValid())
	{
		return;
	}

	if (StackCount <= 0)
	{
		WriteReplicatedStack(DebuffTag, 0);
		return;
	}

	EnsureAbilitySystemBinding();
	TrackDebuffTag(DebuffTag);
	RefreshTrackedActiveEffects(DebuffTag);
	// The apply callback can run before a tag query exposes the new handle.
	// Keep the authoritative handle supplied by GAS even in that ordering.
	TrackActiveEffect(DebuffTag, ActiveEffectHandle);
	WriteReplicatedStack(DebuffTag, StackCount);
}

void UStatusEffectReplicationComponent::TrackAppliedStatusEffect(
	const UStatusEffectDefinition* StatusEffectDefinition,
	const FActiveGameplayEffectHandle ActiveEffectHandle)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor
		|| !OwnerActor->HasAuthority()
		|| !StatusEffectDefinition
		|| !StatusEffectDefinition->DebuffTag.IsValid()
		|| !ActiveEffectHandle.IsValid())
	{
		return;
	}

	const FGameplayTag DebuffTag = StatusEffectDefinition->DebuffTag;
	EnsureAbilitySystemBinding();
	const int32 StackCount = BoundAbilitySystemComponent.IsValid()
		? BoundAbilitySystemComponent->GetCurrentStackCount(ActiveEffectHandle)
		: 0;
	SetStatusEffectStackCount(
		DebuffTag,
		FMath::Max(StackCount, 1),
		ActiveEffectHandle);
	RestartStatusEffectDecay(
		StatusEffectDefinition,
		ActiveEffectHandle,
		FMath::Max(StackCount, 1));
}

int32 UStatusEffectReplicationComponent::GetStatusEffectStackCount(
	const FGameplayTag DebuffTag) const
{
	const int32 EntryIndex = FindReplicatedStackIndex(DebuffTag);
	return ReplicatedStacks.Entries.IsValidIndex(EntryIndex)
		? FMath::Max(ReplicatedStacks.Entries[EntryIndex].StackCount, 0)
		: 0;
}

void UStatusEffectReplicationComponent::HandleReplicatedStackAddedOrChanged(
	const FReplicatedStatusEffectStackEntry& Entry)
{
	NotifyStatusEffectStackChanged(Entry.DebuffTag, Entry.StackCount);
}

void UStatusEffectReplicationComponent::HandleReplicatedStackRemoved(
	const FGameplayTag DebuffTag)
{
	NotifyStatusEffectStackChanged(DebuffTag, 0);
}

void UStatusEffectReplicationComponent::NotifyStatusEffectStackChanged(
	const FGameplayTag DebuffTag,
	const int32 StackCount)
{
	if (DebuffTag.IsValid())
	{
		StatusEffectStackChanged.Broadcast(DebuffTag, FMath::Max(StackCount, 0));
	}
}

void UStatusEffectReplicationComponent::EnsureAbilitySystemBinding()
{
	UAbilitySystemComponent* AbilitySystemComponent =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (BoundAbilitySystemComponent.Get() == AbilitySystemComponent)
	{
		return;
	}

	UnbindAbilitySystem();
	BoundAbilitySystemComponent = AbilitySystemComponent;
	if (AbilitySystemComponent)
	{
		ActiveEffectRemovedHandle = AbilitySystemComponent
			->OnAnyGameplayEffectRemovedDelegate()
			.AddUObject(this, &ThisClass::HandleAnyActiveEffectRemoved);
	}
}

void UStatusEffectReplicationComponent::UnbindAbilitySystem()
{
	ClearAllStatusEffectDecayTimers();

	UAbilitySystemComponent* AbilitySystemComponent =
		BoundAbilitySystemComponent.Get();
	if (AbilitySystemComponent)
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : DebuffTagChangedHandles)
		{
			if (Pair.Key.IsValid() && Pair.Value.IsValid())
			{
				AbilitySystemComponent
					->RegisterGameplayTagEvent(
						Pair.Key,
						EGameplayTagEventType::NewOrRemoved)
					.Remove(Pair.Value);
			}
		}

		for (const TPair<FActiveGameplayEffectHandle, FTrackedActiveEffectBinding>& Pair
			: TrackedActiveEffects)
		{
			if (!Pair.Value.StackChangedHandle.IsValid())
			{
				continue;
			}

			if (FOnActiveGameplayEffectStackChange* StackChangedDelegate =
				AbilitySystemComponent->OnGameplayEffectStackChangeDelegate(Pair.Key))
			{
				StackChangedDelegate->Remove(Pair.Value.StackChangedHandle);
			}
		}

		if (ActiveEffectRemovedHandle.IsValid())
		{
			AbilitySystemComponent->OnAnyGameplayEffectRemovedDelegate().Remove(
				ActiveEffectRemovedHandle);
		}
	}

	DebuffTagChangedHandles.Reset();
	TrackedActiveEffects.Reset();
	ActiveEffectRemovedHandle.Reset();
	BoundAbilitySystemComponent.Reset();
}

void UStatusEffectReplicationComponent::TrackDebuffTag(
	const FGameplayTag DebuffTag)
{
	UAbilitySystemComponent* AbilitySystemComponent =
		BoundAbilitySystemComponent.Get();
	if (!AbilitySystemComponent
		|| !DebuffTag.IsValid()
		|| DebuffTagChangedHandles.Contains(DebuffTag))
	{
		return;
	}

	const FDelegateHandle DelegateHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(
			DebuffTag,
			EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleDebuffTagChanged);
	DebuffTagChangedHandles.Add(DebuffTag, DelegateHandle);
}

void UStatusEffectReplicationComponent::TrackActiveEffect(
	const FGameplayTag DebuffTag,
	const FActiveGameplayEffectHandle ActiveEffectHandle)
{
	UAbilitySystemComponent* AbilitySystemComponent =
		BoundAbilitySystemComponent.Get();
	if (!AbilitySystemComponent
		|| !DebuffTag.IsValid()
		|| !ActiveEffectHandle.IsValid()
		|| TrackedActiveEffects.Contains(ActiveEffectHandle))
	{
		return;
	}

	FOnActiveGameplayEffectStackChange* StackChangedDelegate =
		AbilitySystemComponent->OnGameplayEffectStackChangeDelegate(
			ActiveEffectHandle);
	if (!StackChangedDelegate)
	{
		return;
	}

	FTrackedActiveEffectBinding Binding;
	Binding.DebuffTag = DebuffTag;
	Binding.StackChangedHandle = StackChangedDelegate->AddUObject(
		this,
		&ThisClass::HandleActiveEffectStackChanged);
	TrackedActiveEffects.Add(ActiveEffectHandle, Binding);
}

void UStatusEffectReplicationComponent::RefreshTrackedActiveEffects(
	const FGameplayTag DebuffTag)
{
	UAbilitySystemComponent* AbilitySystemComponent =
		BoundAbilitySystemComponent.Get();
	if (!AbilitySystemComponent || !DebuffTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer DebuffTags;
	DebuffTags.AddTag(DebuffTag);
	const TArray<FActiveGameplayEffectHandle> ActiveHandles =
		AbilitySystemComponent->GetActiveEffectsWithAllTags(DebuffTags);
	TSet<FActiveGameplayEffectHandle> ActiveHandleSet;
	for (const FActiveGameplayEffectHandle ActiveHandle : ActiveHandles)
	{
		ActiveHandleSet.Add(ActiveHandle);
		TrackActiveEffect(DebuffTag, ActiveHandle);
	}

	for (auto Iterator = TrackedActiveEffects.CreateIterator(); Iterator; ++Iterator)
	{
		if (!Iterator.Value().DebuffTag.MatchesTagExact(DebuffTag)
			|| ActiveHandleSet.Contains(Iterator.Key()))
		{
			continue;
		}

		if (Iterator.Value().StackChangedHandle.IsValid())
		{
			if (FOnActiveGameplayEffectStackChange* StackChangedDelegate =
				AbilitySystemComponent->OnGameplayEffectStackChangeDelegate(
					Iterator.Key()))
			{
				StackChangedDelegate->Remove(
					Iterator.Value().StackChangedHandle);
			}
		}
		Iterator.RemoveCurrent();
	}
}

void UStatusEffectReplicationComponent::RefreshReplicatedStackFromAbilitySystem(
	const FGameplayTag DebuffTag)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !DebuffTag.IsValid())
	{
		return;
	}

	EnsureAbilitySystemBinding();
	TrackDebuffTag(DebuffTag);
	RefreshTrackedActiveEffects(DebuffTag);
	WriteReplicatedStack(
		DebuffTag,
		CalculateStackCountFromAbilitySystem(DebuffTag));
}

int32 UStatusEffectReplicationComponent::CalculateStackCountFromAbilitySystem(
	const FGameplayTag DebuffTag) const
{
	const UAbilitySystemComponent* AbilitySystemComponent =
		BoundAbilitySystemComponent.Get();
	if (!AbilitySystemComponent || !DebuffTag.IsValid())
	{
		return 0;
	}

	FGameplayTagContainer DebuffTags;
	DebuffTags.AddTag(DebuffTag);

	int32 StackCount = 0;
	const TArray<FActiveGameplayEffectHandle> ActiveHandles =
		AbilitySystemComponent->GetActiveEffectsWithAllTags(DebuffTags);
	for (const FActiveGameplayEffectHandle ActiveHandle : ActiveHandles)
	{
		const int32 HandleStackCount = FMath::Max(
			AbilitySystemComponent->GetCurrentStackCount(ActiveHandle),
			1);
		StackCount += HandleStackCount;
	}

	if (StackCount <= 0
		&& AbilitySystemComponent->HasMatchingGameplayTag(DebuffTag))
	{
		StackCount = 1;
	}

	return StackCount;
}

void UStatusEffectReplicationComponent::WriteReplicatedStack(
	const FGameplayTag DebuffTag,
	const int32 StackCount)
{
	const int32 SafeStackCount = FMath::Max(StackCount, 0);
	const int32 EntryIndex = FindReplicatedStackIndex(DebuffTag);
	if (SafeStackCount <= 0)
	{
		if (!ReplicatedStacks.Entries.IsValidIndex(EntryIndex))
		{
			return;
		}

		ReplicatedStacks.Entries.RemoveAt(EntryIndex);
		ReplicatedStacks.MarkArrayDirty();
		MarkReplicatedStacksDirty();
		NotifyStatusEffectStackChanged(DebuffTag, 0);
		return;
	}

	if (ReplicatedStacks.Entries.IsValidIndex(EntryIndex))
	{
		FReplicatedStatusEffectStackEntry& Entry =
			ReplicatedStacks.Entries[EntryIndex];
		if (Entry.StackCount == SafeStackCount)
		{
			return;
		}

		Entry.StackCount = SafeStackCount;
		ReplicatedStacks.MarkEntryDirty(Entry);
		MarkReplicatedStacksDirty();
		NotifyStatusEffectStackChanged(DebuffTag, SafeStackCount);
		return;
	}

	FReplicatedStatusEffectStackEntry& NewEntry =
		ReplicatedStacks.Entries.AddDefaulted_GetRef();
	NewEntry.DebuffTag = DebuffTag;
	NewEntry.StackCount = SafeStackCount;
	ReplicatedStacks.MarkEntryDirty(NewEntry);
	MarkReplicatedStacksDirty();
	NotifyStatusEffectStackChanged(DebuffTag, SafeStackCount);
}

void UStatusEffectReplicationComponent::MarkReplicatedStacksDirty()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(
		UStatusEffectReplicationComponent,
		ReplicatedStacks,
		this);

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

int32 UStatusEffectReplicationComponent::FindReplicatedStackIndex(
	const FGameplayTag DebuffTag) const
{
	return ReplicatedStacks.Entries.IndexOfByPredicate(
		[DebuffTag](const FReplicatedStatusEffectStackEntry& Entry)
		{
			return Entry.DebuffTag.MatchesTagExact(DebuffTag);
		});
}

void UStatusEffectReplicationComponent::RestartStatusEffectDecay(
	const UStatusEffectDefinition* StatusEffectDefinition,
	const FActiveGameplayEffectHandle ActiveEffectHandle,
	const int32 StackCount)
{
	UWorld* World = GetWorld();
	if (!World
		|| !StatusEffectDefinition
		|| !StatusEffectDefinition->DebuffTag.IsValid()
		|| !ActiveEffectHandle.IsValid()
		|| StackCount <= 0)
	{
		return;
	}

	const FGameplayTag DebuffTag = StatusEffectDefinition->DebuffTag;
	ClearStatusEffectDecay(DebuffTag);

	FStatusEffectDecayState& DecayState =
		StatusEffectDecayStates.Add(DebuffTag);
	DecayState.ActiveEffectHandle = ActiveEffectHandle;
	DecayState.DecayStartTime = World->GetTimeSeconds()
		+ StatusEffectTiming::StackHoldSeconds;
	DecayState.StartingStackCount = StackCount;
	DecayState.MaxStackCount = FMath::Max(
		StatusEffectDefinition->MaxStackCount,
		1);
	const float StackBoundaryInterval =
		StatusEffectTiming::StackDecaySeconds
		/ static_cast<float>(DecayState.MaxStackCount);

	World->GetTimerManager().SetTimer(
		DecayState.UpdateTimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::UpdateStatusEffectDecay,
			DebuffTag),
		StatusEffectTiming::StackHoldSeconds + StackBoundaryInterval,
		false);
}

void UStatusEffectReplicationComponent::UpdateStatusEffectDecay(
	const FGameplayTag DebuffTag)
{
	UWorld* World = GetWorld();
	UAbilitySystemComponent* AbilitySystemComponent =
		BoundAbilitySystemComponent.Get();
	const FStatusEffectDecayState* DecayState =
		StatusEffectDecayStates.Find(DebuffTag);
	if (!World || !AbilitySystemComponent || !DecayState)
	{
		ClearStatusEffectDecay(DebuffTag);
		return;
	}

	const FActiveGameplayEffectHandle ActiveEffectHandle =
		DecayState->ActiveEffectHandle;
	const int32 CurrentStackCount =
		AbilitySystemComponent->GetCurrentStackCount(ActiveEffectHandle);
	if (CurrentStackCount <= 0)
	{
		ClearStatusEffectDecay(DebuffTag);
		return;
	}

	const double ElapsedSeconds = FMath::Max(
		World->GetTimeSeconds() - DecayState->DecayStartTime,
		0.0);
	const double RemovedStackProgress =
		(ElapsedSeconds
			/ static_cast<double>(StatusEffectTiming::StackDecaySeconds))
		* static_cast<double>(DecayState->MaxStackCount);
	const int32 TargetStackCount = FMath::Max(
		FMath::CeilToInt(
			static_cast<double>(DecayState->StartingStackCount)
				- RemovedStackProgress),
		0);
	if (TargetStackCount < CurrentStackCount)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(
			ActiveEffectHandle,
			CurrentStackCount - TargetStackCount);
	}

	if (TargetStackCount <= 0)
	{
		return;
	}

	FStatusEffectDecayState* MutableDecayState =
		StatusEffectDecayStates.Find(DebuffTag);
	if (!MutableDecayState)
	{
		return;
	}

	const int32 RemovedStackCount =
		MutableDecayState->StartingStackCount - TargetStackCount;
	const double StackBoundaryInterval =
		static_cast<double>(StatusEffectTiming::StackDecaySeconds)
		/ static_cast<double>(MutableDecayState->MaxStackCount);
	const double NextBoundaryTime = MutableDecayState->DecayStartTime
		+ (static_cast<double>(RemovedStackCount + 1)
			* StackBoundaryInterval);
	const float NextDelay = FMath::Max(
		static_cast<float>(NextBoundaryTime - World->GetTimeSeconds()),
		KINDA_SMALL_NUMBER);
	World->GetTimerManager().SetTimer(
		MutableDecayState->UpdateTimerHandle,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::UpdateStatusEffectDecay,
			DebuffTag),
		NextDelay,
		false);
}

void UStatusEffectReplicationComponent::ClearStatusEffectDecay(
	const FGameplayTag DebuffTag)
{
	FStatusEffectDecayState* DecayState =
		StatusEffectDecayStates.Find(DebuffTag);
	if (!DecayState)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(
			DecayState->UpdateTimerHandle);
	}
	StatusEffectDecayStates.Remove(DebuffTag);
}

void UStatusEffectReplicationComponent::ClearAllStatusEffectDecayTimers()
{
	if (UWorld* World = GetWorld())
	{
		for (TPair<FGameplayTag, FStatusEffectDecayState>& Pair
			: StatusEffectDecayStates)
		{
			World->GetTimerManager().ClearTimer(
				Pair.Value.UpdateTimerHandle);
		}
	}
	StatusEffectDecayStates.Reset();
}

void UStatusEffectReplicationComponent::HandleDebuffTagChanged(
	const FGameplayTag DebuffTag,
	const int32 NewCount)
{
	if (NewCount <= 0)
	{
		WriteReplicatedStack(DebuffTag, 0);
		return;
	}

	RefreshReplicatedStackFromAbilitySystem(DebuffTag);
}

void UStatusEffectReplicationComponent::HandleActiveEffectStackChanged(
	const FActiveGameplayEffectHandle ActiveEffectHandle,
	const int32 NewStackCount,
	const int32)
{
	if (const FTrackedActiveEffectBinding* Binding =
		TrackedActiveEffects.Find(ActiveEffectHandle))
	{
		// GAS invokes this delegate while its active-effect container is being
		// updated. Querying the container here can temporarily return no handles
		// and collapse the presentation value to the one-tag fallback. The
		// delegate's stack count is the authoritative value for this tracked GE.
		WriteReplicatedStack(
			Binding->DebuffTag,
			FMath::Max(NewStackCount, 0));
	}
}

void UStatusEffectReplicationComponent::HandleAnyActiveEffectRemoved(
	const FActiveGameplayEffect& RemovedEffect)
{
	const FTrackedActiveEffectBinding* Binding =
		TrackedActiveEffects.Find(RemovedEffect.Handle);
	if (!Binding)
	{
		return;
	}

	const FGameplayTag DebuffTag = Binding->DebuffTag;
	TrackedActiveEffects.Remove(RemovedEffect.Handle);
	if (const FStatusEffectDecayState* DecayState =
		StatusEffectDecayStates.Find(DebuffTag);
		DecayState
		&& DecayState->ActiveEffectHandle == RemovedEffect.Handle)
	{
		ClearStatusEffectDecay(DebuffTag);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this,
				&ThisClass::RefreshReplicatedStackFromAbilitySystem,
				DebuffTag));
		return;
	}

	RefreshReplicatedStackFromAbilitySystem(DebuffTag);
}
