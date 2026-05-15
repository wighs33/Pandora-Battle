#include "GameFeature/PdActorExtensionWorldSubsystem.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Experience/ExperienceManagerComponent.h"
#include "GameFramework/GameStateBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdActorExtensionWorldSubsystem)

DEFINE_LOG_CATEGORY(PdActorExtensionWorldSubsystemLog);

FPdActorExtensionHandle::FPdActorExtensionHandle(UPdActorExtensionWorldSubsystem* InSubsystem, int32 InExtensionId)
	: Subsystem(InSubsystem)
	, ExtensionId(InExtensionId)
{
}

FPdActorExtensionHandle::~FPdActorExtensionHandle()
{
	Unregister();
}

void FPdActorExtensionHandle::Unregister()
{
	if (ExtensionId == INDEX_NONE)
	{
		return;
	}

	if (UPdActorExtensionWorldSubsystem* ExtensionSubsystem = Subsystem.Get())
	{
		ExtensionSubsystem->UnregisterExtension(ExtensionId);
	}

	ExtensionId = INDEX_NONE;
}

void UPdActorExtensionWorldSubsystem::Deinitialize()
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
			if (const FPdActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId))
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

void UPdActorExtensionWorldSubsystem::Tick(float DeltaTime)
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
			RegisterActors.Add(FPdRegisteredActorExtension{ Actor, MoveTemp(ExtensionIds) });
		}
	}

	for (int32 ActorIndex = RegisterActors.Num() - 1; ActorIndex >= 0; --ActorIndex)
	{
		FPdRegisteredActorExtension& RegisteredActor = RegisterActors[ActorIndex];
		AActor* Actor = RegisteredActor.Actor.Get();
		if (!Actor)
		{
			RegisterActors.RemoveAtSwap(ActorIndex);
			continue;
		}

		for (int32 ExtensionIndex = RegisteredActor.ExtensionIds.Num() - 1; ExtensionIndex >= 0; --ExtensionIndex)
		{
			const int32 ExtensionId = RegisteredActor.ExtensionIds[ExtensionIndex];
			FPdActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId);
			if (!ExtensionSpec || IsExtensionActiveForActor(Actor, ExtensionId) || !ShouldApplyExtensionToActor(Actor, *ExtensionSpec))
			{
				RegisteredActor.WaitFrameCountsByExtensionId.Remove(ExtensionId);
				RegisteredActor.ExtensionIds.RemoveAtSwap(ExtensionIndex);
				continue;
			}

			const bool bCanActivate = !ExtensionSpec->CanActivate.IsBound() || ExtensionSpec->CanActivate.Execute(Actor);
			if (!bCanActivate)
			{
				int32& WaitFrameCount = RegisteredActor.WaitFrameCountsByExtensionId.FindOrAdd(ExtensionId);
				++WaitFrameCount;

				if (ExtensionSpec->ActivationWarningFrameCount > 0 && WaitFrameCount == ExtensionSpec->ActivationWarningFrameCount)
				{
					const FString DebugName = ExtensionSpec->DebugName.IsNone()
						? FString::Printf(TEXT("ExtensionId %d"), ExtensionId)
						: ExtensionSpec->DebugName.ToString();
					UE_LOG(PdActorExtensionWorldSubsystemLog, Warning,
						TEXT("Actor extension '%s' is still waiting for '%s'. Check required GameFeature component/action ordering."),
						*DebugName,
						*GetNameSafe(Actor));
				}
				continue;
			}

			if (ExtensionSpec->OnActivate.IsBound())
			{
				ExtensionSpec->OnActivate.Execute(Actor);
			}

			ActiveActorExtensions.FindOrAdd(TWeakObjectPtr<AActor>(Actor)).Add(ExtensionId);
			RegisteredActor.WaitFrameCountsByExtensionId.Remove(ExtensionId);
			RegisteredActor.ExtensionIds.RemoveAtSwap(ExtensionIndex);
		}

		if (RegisteredActor.ExtensionIds.IsEmpty())
		{
			RegisterActors.RemoveAtSwap(ActorIndex);
		}
	}
}

TStatId UPdActorExtensionWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPdActorExtensionWorldSubsystem, STATGROUP_Tickables);
}

bool UPdActorExtensionWorldSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UPdActorExtensionWorldSubsystem::IsTickable() const
{
	return bExperienceLoaded && (!UncheckedActors.IsEmpty() || !RegisterActors.IsEmpty());
}

TSharedPtr<FPdActorExtensionHandle> UPdActorExtensionWorldSubsystem::RegisterExtensionForClass(
	UClass* TargetClass,
	FPdActorExtensionSpec ExtensionSpec)
{
	if (!TargetClass)
	{
		return nullptr;
	}

	EnsureExtensionEventHandler(TargetClass);
	RefreshExperienceLoadState();

	const int32 ExtensionId = NextExtensionId++;
	ExtensionById.Add(ExtensionId, MoveTemp(ExtensionSpec));

	TArray<UClass*> Classes;
	GetDerivedClasses(TargetClass, Classes, true);
	Classes.Add(TargetClass);

	for (UClass* Class : Classes)
	{
		if (Class)
		{
			ClassExtensionIds.FindOrAdd(Class).Add(ExtensionId);
		}
	}

	QueueExistingActorsForClass(TargetClass);
	return MakeShared<FPdActorExtensionHandle>(this, ExtensionId);
}

void UPdActorExtensionWorldSubsystem::UnregisterExtension(int32 ExtensionId)
{
	const FPdActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId);
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

	for (FPdRegisteredActorExtension& RegisteredActor : RegisterActors)
	{
		RegisteredActor.ExtensionIds.Remove(ExtensionId);
	}

	for (auto& Pair : ClassExtensionIds)
	{
		Pair.Value.Remove(ExtensionId);
	}

	ExtensionById.Remove(ExtensionId);
}

void UPdActorExtensionWorldSubsystem::EnsureExtensionEventHandler(UClass* TargetClass)
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

void UPdActorExtensionWorldSubsystem::HandleActorExtensionEvent(AActor* Actor, FName EventName)
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

void UPdActorExtensionWorldSubsystem::RefreshExperienceLoadState()
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
}

void UPdActorExtensionWorldSubsystem::QueueActor(AActor* Actor)
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

void UPdActorExtensionWorldSubsystem::QueueExistingActorsForClass(UClass* TargetClass)
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

void UPdActorExtensionWorldSubsystem::CollectExtensionsForActor(AActor* Actor, TArray<int32>& OutExtensionIds) const
{
	if (!Actor)
	{
		return;
	}

	const TSet<int32>* ExtensionIds = ClassExtensionIds.Find(Actor->GetClass());
	if (!ExtensionIds)
	{
		return;
	}

	for (const int32 ExtensionId : *ExtensionIds)
	{
		const FPdActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId);
		if (!ExtensionSpec
			|| IsExtensionActiveForActor(Actor, ExtensionId)
			|| IsExtensionPendingForActor(Actor, ExtensionId)
			|| !ShouldApplyExtensionToActor(Actor, *ExtensionSpec))
		{
			continue;
		}

		OutExtensionIds.Add(ExtensionId);
	}
}

void UPdActorExtensionWorldSubsystem::RemoveActor(AActor* Actor)
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
			if (const FPdActorExtensionSpec* ExtensionSpec = ExtensionById.Find(ExtensionId))
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

void UPdActorExtensionWorldSubsystem::DeactivateExtensionForActor(
	AActor* Actor,
	int32 ExtensionId,
	const FPdActorExtensionSpec& ExtensionSpec)
{
	static_cast<void>(ExtensionId);

	if (Actor && ExtensionSpec.OnDeactivate.IsBound())
	{
		ExtensionSpec.OnDeactivate.Execute(Actor);
	}
}

bool UPdActorExtensionWorldSubsystem::IsExtensionActiveForActor(AActor* Actor, int32 ExtensionId) const
{
	const TSet<int32>* ActiveExtensionIds = ActiveActorExtensions.Find(TWeakObjectPtr<AActor>(Actor));
	return ActiveExtensionIds && ActiveExtensionIds->Contains(ExtensionId);
}

bool UPdActorExtensionWorldSubsystem::IsExtensionPendingForActor(AActor* Actor, int32 ExtensionId) const
{
	for (const FPdRegisteredActorExtension& RegisteredActor : RegisterActors)
	{
		if (RegisteredActor.Actor.Get() == Actor && RegisteredActor.ExtensionIds.Contains(ExtensionId))
		{
			return true;
		}
	}

	return false;
}

bool UPdActorExtensionWorldSubsystem::ShouldApplyExtensionToActor(
	AActor* Actor,
	const FPdActorExtensionSpec& ExtensionSpec) const
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
