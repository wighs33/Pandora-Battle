#include "GameFeature/ActorExtensionWorldSubsystem.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Definition/Experience/ExperienceDefinition.h"
#include "Component/Experience/ExperienceManagerComponent.h"
#include "GameFramework/GameStateBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActorExtensionWorldSubsystem)

FActorExtensionHandle::FActorExtensionHandle(UActorExtensionWorldSubsystem* InSubsystem, int32 InExtensionId)
	: Subsystem(InSubsystem)
	, ExtensionId(InExtensionId)
{
}

FActorExtensionHandle::~FActorExtensionHandle()
{
	Unregister();
}

void FActorExtensionHandle::Unregister()
{
	if (ExtensionId == INDEX_NONE)
	{
		return;
	}

	if (UActorExtensionWorldSubsystem* ExtensionSubsystem = Subsystem.Get())
	{
		ExtensionSubsystem->UnregisterExtension(ExtensionId);
	}

	ExtensionId = INDEX_NONE;
}

void UActorExtensionWorldSubsystem::Deinitialize()
{
	for (auto& Pair : ActiveActorExtensions)
	{
		AActor* Actor = Pair.Key.Get();
		if (!Actor)
		{
			continue;
		}

		for (const int32 ExtensionId : Pair.Value)
		{
			if (const FActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId))
			{
				DeactivateExtensionForActor(Actor, ExtensionId, *ExtensionSpec);
			}
		}
	}

	UncheckedActors.Reset();
	RegisterActors.Reset();
	ActiveActorExtensions.Reset();
	ExtensionById.Reset();
	ClassExtensionIds.Reset();
	ExtensionEventHandles.Reset();

	Super::Deinitialize();
}

void UActorExtensionWorldSubsystem::Tick(float DeltaTime)
{
	static_cast<void>(DeltaTime);

	for (int32 ActorIndex = UncheckedActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		AActor* Actor = UncheckedActors[ActorIndex].Get();
		UncheckedActors.RemoveAtSwap(ActorIndex);

		if (!Actor)
		{
			continue;
		}

		TArray<int32> ExtensionIds;
		CollectExtensionsForActor(Actor, ExtensionIds);
		if (!ExtensionIds.IsEmpty())
		{
			RegisterActors.Add(FRegisteredActorExtension{ Actor, MoveTemp(ExtensionIds) });
		}
	}

	for (int32 ActorIndex = RegisterActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		FRegisteredActorExtension& RegisteredActor = RegisterActors[ActorIndex];
		AActor* Actor = RegisteredActor.Actor.Get();
		if (!Actor)
		{
			RegisterActors.RemoveAtSwap(ActorIndex);
			continue;
		}

		for (int32 ExtensionIndex = RegisteredActor.ExtensionIds.Num() - 1; ExtensionIndex >= 0; --ExtensionIndex)
		{
			const int32 ExtensionId = RegisteredActor.ExtensionIds[ExtensionIndex];
			FActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId);
			if (!ExtensionSpec || IsExtensionActiveForActor(Actor, ExtensionId) || !ShouldApplyExtensionToActor(Actor, *ExtensionSpec))
			{
				RegisteredActor.ExtensionIds.RemoveAtSwap(ExtensionIndex);
				continue;
			}

			const bool bCanActivate = !ExtensionSpec->CanActivate.IsBound() || ExtensionSpec->CanActivate.Execute(Actor);
			if (!bCanActivate)
			{
				continue;
			}

			if (ExtensionSpec->OnActivate.IsBound())
			{
				ExtensionSpec->OnActivate.Execute(Actor);
			}

			ActiveActorExtensions.FindOrAdd(TWeakObjectPtr<AActor>(Actor)).Add(ExtensionId);
			RegisteredActor.ExtensionIds.RemoveAtSwap(ExtensionIndex);
		}

		if (RegisteredActor.ExtensionIds.IsEmpty())
		{
			RegisterActors.RemoveAtSwap(ActorIndex);
		}
	}
}

TStatId UActorExtensionWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UActorExtensionWorldSubsystem, STATGROUP_Tickables);
}

bool UActorExtensionWorldSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UActorExtensionWorldSubsystem::IsTickable() const
{
	return bExperienceLoaded && (!UncheckedActors.IsEmpty() || !RegisterActors.IsEmpty());
}

TSharedPtr<FActorExtensionHandle> UActorExtensionWorldSubsystem::RegisterExtensionForClass(
	UClass* TargetClass,
	FActorExtensionSpec ExtensionSpec)
{
	if (!TargetClass)
	{
		return nullptr;
	}

	EnsureExtensionEventHandler(TargetClass);
	RefreshExperienceLoadState();

	const int32 ExtensionId = NextExtensionId++;
	ExtensionById.Add(ExtensionId, MoveTemp(ExtensionSpec));
	ClassExtensionIds.FindOrAdd(TargetClass).Add(ExtensionId);

	QueueExistingActorsForClass(TargetClass);
	return MakeShared<FActorExtensionHandle>(this, ExtensionId);
}

void UActorExtensionWorldSubsystem::UnregisterExtension(int32 ExtensionId)
{
	const FActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId);
	if (!ExtensionSpec)
	{
		return;
	}

	for (auto It = ActiveActorExtensions.CreateIterator(); It; ++It)
	{
		if (!It.Value().Contains(ExtensionId))
		{
			continue;
		}

		if (AActor* Actor = It.Key().Get())
		{
			DeactivateExtensionForActor(Actor, ExtensionId, *ExtensionSpec);
		}

		It.Value().Remove(ExtensionId);
		if (It.Value().IsEmpty())
		{
			It.RemoveCurrent();
		}
	}

	for (FRegisteredActorExtension& RegisteredActor : RegisterActors)
	{
		RegisteredActor.ExtensionIds.Remove(ExtensionId);
	}

	for (auto& Pair : ClassExtensionIds)
	{
		Pair.Value.Remove(ExtensionId);
	}

	ExtensionById.Remove(ExtensionId);
}

void UActorExtensionWorldSubsystem::EnsureExtensionEventHandler(UClass* TargetClass)
{
	if (!TargetClass || ExtensionEventHandles.Contains(TargetClass))
	{
		return;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UGameFrameworkComponentManager* ComponentManager = GameInstance
		? UGameInstance::GetSubsystem<UGameFrameworkComponentManager>(GameInstance)
		: nullptr;
	if (!ComponentManager)
	{
		return;
	}

	ExtensionEventHandles.Add(
		TargetClass,
		ComponentManager->AddExtensionHandler(
			TargetClass,
			UGameFrameworkComponentManager::FExtensionHandlerDelegate::CreateUObject(
				this,
				&ThisClass::HandleActorExtensionEvent)));
}

void UActorExtensionWorldSubsystem::HandleActorExtensionEvent(AActor* Actor, FName EventName)
{
	if (!Actor)
	{
		return;
	}

	if (EventName == UGameFrameworkComponentManager::NAME_GameActorReady
		|| EventName == UGameFrameworkComponentManager::NAME_ExtensionAdded
		|| EventName == UGameFrameworkComponentManager::NAME_ReceiverAdded)
	{
		QueueActor(Actor);
		return;
	}

	if (EventName == UGameFrameworkComponentManager::NAME_ExtensionRemoved
		|| EventName == UGameFrameworkComponentManager::NAME_ReceiverRemoved)
	{
		RemoveActor(Actor);
	}
}

void UActorExtensionWorldSubsystem::RefreshExperienceLoadState()
{
	if (bExperienceLoaded)
	{
		return;
	}

	UWorld* World = GetWorld();
	AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	UExperienceManagerComponent* ExperienceManager = GameState ? GameState->FindComponentByClass<UExperienceManagerComponent>() : nullptr;
	if (!ExperienceManager)
	{
		bExperienceLoaded = true;
		return;
	}

	if (ExperienceManager->IsExperienceLoaded())
	{
		bExperienceLoaded = true;
		return;
	}

	ExperienceManager->CallOrRegister_OnExperienceLoaded(FOnPdExperienceLoaded::FDelegate::CreateWeakLambda(
		this,
		[this](const UExperienceDefinition*)
		{
			bExperienceLoaded = true;
		}));
	ExperienceManager->CallOrRegister_OnExperienceLoadFailed(FOnPdExperienceLoadFailed::FDelegate::CreateWeakLambda(
		this,
		[this](FPrimaryAssetId, const FString&)
		{
			bExperienceLoaded = true;
		}));
}

void UActorExtensionWorldSubsystem::QueueActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& UncheckedActor : UncheckedActors)
	{
		if (UncheckedActor.Get() == Actor)
		{
			return;
		}
	}

	UncheckedActors.Add(Actor);
}

void UActorExtensionWorldSubsystem::QueueExistingActorsForClass(UClass* TargetClass)
{
	UWorld* World = GetWorld();
	if (!World || !TargetClass)
	{
		return;
	}

	for (TActorIterator<AActor> ActorIt(World, TargetClass); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if (Actor && Actor->IsActorInitialized())
		{
			QueueActor(Actor);
		}
	}
}

void UActorExtensionWorldSubsystem::CollectExtensionsForActor(AActor* Actor, TArray<int32>& OutExtensionIds) const
{
	if (!Actor)
	{
		return;
	}

	for (UClass* ActorClass = Actor->GetClass(); ActorClass; ActorClass = ActorClass->GetSuperClass())
	{
		const TSet<int32>* ExtensionIds = ClassExtensionIds.Find(ActorClass);
		if (!ExtensionIds)
		{
			continue;
		}

		for (const int32 ExtensionId : *ExtensionIds)
		{
			const FActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId);
			if (!ExtensionSpec
				|| IsExtensionActiveForActor(Actor, ExtensionId)
				|| IsExtensionPendingForActor(Actor, ExtensionId)
				|| !ShouldApplyExtensionToActor(Actor, *ExtensionSpec))
			{
				continue;
			}

			OutExtensionIds.AddUnique(ExtensionId);
		}
	}
}

void UActorExtensionWorldSubsystem::RemoveActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	const TWeakObjectPtr<AActor> ActorKey(Actor);
	if (TSet<int32>* ActiveExtensionIds = ActiveActorExtensions.Find(ActorKey))
	{
		for (const int32 ExtensionId : *ActiveExtensionIds)
		{
			if (const FActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId))
			{
				DeactivateExtensionForActor(Actor, ExtensionId, *ExtensionSpec);
			}
		}

		ActiveActorExtensions.Remove(ActorKey);
	}

	for (int32 ActorIndex = RegisterActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		if (RegisterActors[ActorIndex].Actor.Get() == Actor)
		{
			RegisterActors.RemoveAtSwap(ActorIndex);
		}
	}

	for (int32 ActorIndex = UncheckedActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		if (UncheckedActors[ActorIndex].Get() == Actor)
		{
			UncheckedActors.RemoveAtSwap(ActorIndex);
		}
	}
}

void UActorExtensionWorldSubsystem::DeactivateExtensionForActor(
	AActor* Actor,
	int32 ExtensionId,
	const FActorExtensionSpec& ExtensionSpec)
{
	static_cast<void>(ExtensionId);

	if (Actor && ExtensionSpec.OnDeactivate.IsBound())
	{
		ExtensionSpec.OnDeactivate.Execute(Actor);
	}
}

bool UActorExtensionWorldSubsystem::IsExtensionActiveForActor(AActor* Actor, int32 ExtensionId) const
{
	const TSet<int32>* ActiveExtensionIds = ActiveActorExtensions.Find(TWeakObjectPtr<AActor>(Actor));
	return ActiveExtensionIds && ActiveExtensionIds->Contains(ExtensionId);
}

bool UActorExtensionWorldSubsystem::IsExtensionPendingForActor(AActor* Actor, int32 ExtensionId) const
{
	for (const FRegisteredActorExtension& RegisteredActor : RegisterActors)
	{
		if (RegisteredActor.Actor.Get() == Actor && RegisteredActor.ExtensionIds.Contains(ExtensionId))
		{
			return true;
		}
	}

	return false;
}

bool UActorExtensionWorldSubsystem::ShouldApplyExtensionToActor(
	AActor* Actor,
	const FActorExtensionSpec& ExtensionSpec) const
{
	if (!Actor || !ExtensionSpec.bUseClientRoleFilter)
	{
		return true;
	}

	if (Actor->HasLocalNetOwner())
	{
		return ExtensionSpec.bAddToLocallyControlled;
	}

	if (Actor->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return ExtensionSpec.bAddToSimulatedProxy;
	}

	return true;
}
