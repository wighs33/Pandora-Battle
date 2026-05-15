#include "GameFeature/GameFeatureAction_AddActorExtension.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFeature/PdActorExtensionWorldSubsystem.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddActorExtension)

DEFINE_LOG_CATEGORY(PdGameFeatureAction_AddActorExtensionLog);

void UGameFeatureAction_AddActorExtension::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	const FGameFeatureStateChangeContext ChangeContext(Context);
	FPdGameFeatureActorExtensionHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles)
	{
		return;
	}

	DeactivateAllActorExtensions(*Handles);
	Handles->ExtensionRequestHandles.Reset();
	ContextHandles.Remove(ChangeContext);
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddActorExtension::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (TargetClass.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddActorExtension", "MissingTargetClass", "TargetClass is required."));
	}

	if (!bClientAction && !bServerAction)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddActorExtension", "MissingNetwork", "At least one Network option is required."));
	}

	if (Extension.Conditions.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("PdGameFeatureAction_AddActorExtension", "NoConditions", "Extension has no conditions and will run as soon as the actor is ready."));
	}

	if (Extension.Executes.IsEmpty())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddActorExtension", "NoExecutes", "Extension requires at least one execute entry."));
	}

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void UGameFeatureAction_AddActorExtension::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	Extension.AddAdditionalAssetBundleData(AssetBundleData);
}
#endif

void UGameFeatureAction_AddActorExtension::AddToWorld(
	const FWorldContext& WorldContext,
	const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if (!World || !World->IsGameWorld() || TargetClass.IsNull())
	{
		return;
	}

	RegisterActorExtension(World, ChangeContext);
}

void UGameFeatureAction_AddActorExtension::RegisterActorExtension(
	UWorld* World,
	FGameFeatureStateChangeContext ChangeContext)
{
	if (!World || TargetClass.IsNull())
	{
		return;
	}

	UPdActorExtensionWorldSubsystem* ExtensionSubsystem = World->GetSubsystem<UPdActorExtensionWorldSubsystem>();
	if (!ExtensionSubsystem)
	{
		TWeakObjectPtr<UWorld> WeakWorld = World;
		TWeakObjectPtr<ThisClass> WeakThis = this;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, WeakWorld, ChangeContext]()
		{
			if (ThisClass* This = WeakThis.Get())
			{
				This->RegisterActorExtension(WeakWorld.Get(), ChangeContext);
			}
		}));
		return;
	}

	TSubclassOf<AActor> LoadedTargetClass = TargetClass.LoadSynchronous();
	if (!LoadedTargetClass)
	{
		UE_LOG(PdGameFeatureAction_AddActorExtensionLog, Error, TEXT("AddActorExtension skipped '%s': failed to load target class."),
			*TargetClass.ToString());
		return;
	}

	FPdGameFeatureActorExtensionHandles& Handles = ContextHandles.FindOrAdd(ChangeContext);

	FPdActorExtensionSpec ExtensionSpec;
	ExtensionSpec.DebugName = GetFName();
	ExtensionSpec.bUseClientRoleFilter = bClientAction;
	ExtensionSpec.bAddToLocallyControlled = bAddToLocallyControlled;
	ExtensionSpec.bAddToSimulatedProxy = bAddToSimulatedProxy;
	ExtensionSpec.CanActivate = FPdActorExtensionCanActivate::CreateUObject(this, &ThisClass::CanActivateActorExtension);
	ExtensionSpec.OnActivate = FPdActorExtensionExecute::CreateUObject(this, &ThisClass::ActivateActorExtension, ChangeContext);
	ExtensionSpec.OnDeactivate = FPdActorExtensionExecute::CreateUObject(this, &ThisClass::DeactivateActorExtension, ChangeContext);

	if (TSharedPtr<FPdActorExtensionHandle> ExtensionHandle = ExtensionSubsystem->RegisterExtensionForClass(LoadedTargetClass, MoveTemp(ExtensionSpec)))
	{
		Handles.ExtensionRequestHandles.Add(ExtensionHandle);
	}
}

bool UGameFeatureAction_AddActorExtension::CanActivateActorExtension(AActor* Actor) const
{
	return Actor && Extension.CanActivate(Actor);
}

void UGameFeatureAction_AddActorExtension::ActivateActorExtension(
	AActor* Actor,
	FGameFeatureStateChangeContext ChangeContext)
{
	if (!Actor)
	{
		return;
	}

	FPdGameFeatureActorExtensionHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles || Handles->ActorExtensions.Contains(Actor))
	{
		return;
	}

	FActorExtension& ActorExtension = Handles->ActorExtensions.Add(Actor, Extension);
	ActorExtension.OnActivate(Actor);
}

void UGameFeatureAction_AddActorExtension::DeactivateActorExtension(
	AActor* Actor,
	FGameFeatureStateChangeContext ChangeContext)
{
	if (!Actor)
	{
		return;
	}

	FPdGameFeatureActorExtensionHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles)
	{
		return;
	}

	FActorExtension ActorExtension;
	if (Handles->ActorExtensions.RemoveAndCopyValue(Actor, ActorExtension))
	{
		ActorExtension.OnDeactivate(Actor);
	}
}

void UGameFeatureAction_AddActorExtension::DeactivateAllActorExtensions(
	FPdGameFeatureActorExtensionHandles& Handles) const
{
	TArray<TWeakObjectPtr<AActor>> Actors;
	Handles.ActorExtensions.GetKeys(Actors);

	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		if (FActorExtension* ActorExtension = Handles.ActorExtensions.Find(Actor))
		{
			ActorExtension->OnDeactivate(Actor.Get());
		}
	}

	Handles.ActorExtensions.Reset();
}
