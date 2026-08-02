#include "GameFeature/GameFeatureAction_AddWidgets.h"

#include "AssetRegistry/AssetBundleData.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFeature/ActorExtensionWorldSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "Mode/PdHUD.h"
#include "TimerManager.h"
#include "Definition/UI/WidgetClassDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddWidgets)

DEFINE_LOG_CATEGORY(PdGameFeatureAction_AddWidgetsLog);

UGameFeatureAction_AddWidgets::UGameFeatureAction_AddWidgets()
{
	bClientAction = true;
	bServerAction = false;
	TargetHudClass = APdHUD::StaticClass();
}

void UGameFeatureAction_AddWidgets::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	const FGameFeatureStateChangeContext ChangeContext(Context);
	FPdGameFeatureWidgetHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles)
	{
		return;
	}

	RemoveAllWidgets(*Handles);
	Handles->ExtensionRequestHandles.Reset();
	ContextHandles.Remove(ChangeContext);
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_AddWidgets::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (TargetHudClass.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddWidgets", "MissingTargetHudClass", "TargetHudClass is required."));
	}

	if (WidgetClassDefinition.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddWidgets", "MissingWidgetClassDefinition", "WidgetClassDefinition is required."));
	}

	if (!bClientAction)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_AddWidgets", "MissingClientAction", "Add Widgets must run on the client."));
	}

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void UGameFeatureAction_AddWidgets::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	if (!TargetHudClass.IsNull())
	{
		AssetBundleData.AddBundleAsset(
			UGameFeaturesSubsystemSettings::LoadStateClient,
			TargetHudClass.ToSoftObjectPath().GetAssetPath());
	}

	if (!WidgetClassDefinition.IsNull())
	{
		AssetBundleData.AddBundleAsset(
			UGameFeaturesSubsystemSettings::LoadStateClient,
			WidgetClassDefinition.ToSoftObjectPath().GetAssetPath());
	}
}
#endif

void UGameFeatureAction_AddWidgets::AddToWorld(
	const FWorldContext& WorldContext,
	const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if (!World || !World->IsGameWorld() || TargetHudClass.IsNull())
	{
		return;
	}

	RegisterWidgetExtension(World, ChangeContext);
}

void UGameFeatureAction_AddWidgets::RegisterWidgetExtension(
	UWorld* World,
	FGameFeatureStateChangeContext ChangeContext)
{
	if (!World || TargetHudClass.IsNull())
	{
		return;
	}

	UActorExtensionWorldSubsystem* ExtensionSubsystem = World->GetSubsystem<UActorExtensionWorldSubsystem>();
	if (!ExtensionSubsystem)
	{
		TWeakObjectPtr<UWorld> WeakWorld = World;
		TWeakObjectPtr<ThisClass> WeakThis = this;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis, WeakWorld, ChangeContext]()
		{
			if (ThisClass* This = WeakThis.Get())
			{
				This->RegisterWidgetExtension(WeakWorld.Get(), ChangeContext);
			}
		}));
		return;
	}

	const TSubclassOf<APdHUD> LoadedTargetClass = TargetHudClass.Get();
	if (!LoadedTargetClass)
	{
		UE_LOG(PdGameFeatureAction_AddWidgetsLog, Error,
			TEXT("AddWidgets skipped '%s': target HUD class was not resident after the GameFeature bundle loaded."),
			*TargetHudClass.ToString());
		return;
	}

	FPdGameFeatureWidgetHandles& Handles = ContextHandles.FindOrAdd(ChangeContext);

	FPdActorExtensionSpec ExtensionSpec;
	ExtensionSpec.CanActivate = FPdActorExtensionCanActivate::CreateUObject(this, &ThisClass::CanActivateWidgetExtension);
	ExtensionSpec.OnActivate = FPdActorExtensionExecute::CreateUObject(this, &ThisClass::AddWidgetsToActor, ChangeContext);
	ExtensionSpec.OnDeactivate = FPdActorExtensionExecute::CreateUObject(this, &ThisClass::RemoveWidgetsFromActor, ChangeContext);

	if (TSharedPtr<FActorExtensionHandle> ExtensionHandle = ExtensionSubsystem->RegisterExtensionForClass(LoadedTargetClass, MoveTemp(ExtensionSpec)))
	{
		Handles.ExtensionRequestHandles.Add(ExtensionHandle);
	}
}

bool UGameFeatureAction_AddWidgets::CanActivateWidgetExtension(AActor* Actor) const
{
	const APdHUD* HUD = Cast<APdHUD>(Actor);
	return HUD && HUD->GetOwningPlayerController() && !WidgetClassDefinition.IsNull();
}

void UGameFeatureAction_AddWidgets::AddWidgetsToActor(
	AActor* Actor,
	FGameFeatureStateChangeContext ChangeContext)
{
	APdHUD* HUD = Cast<APdHUD>(Actor);
	if (!HUD)
	{
		return;
	}

	FPdGameFeatureWidgetHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles
		|| Handles->WidgetDefinitionsByActor.Contains(Actor)
		|| Handles->PendingWidgetDefinitionsByActor.Contains(Actor))
	{
		return;
	}

	UWidgetClassDefinition* LoadedWidgetClassDefinition = WidgetClassDefinition.Get();
	if (!LoadedWidgetClassDefinition)
	{
		UE_LOG(PdGameFeatureAction_AddWidgetsLog, Error,
			TEXT("AddWidgets skipped '%s': WidgetClassDefinition was not resident after the GameFeature bundle loaded."),
			*GetNameSafe(Actor));
		return;
	}

	TArray<FSoftObjectPath> AssetPaths;
	LoadedWidgetClassDefinition->GetRuntimePreloadAssetPaths(AssetPaths);
	Handles->PendingWidgetDefinitionsByActor.Add(Actor, LoadedWidgetClassDefinition);
	if (AssetPaths.IsEmpty())
	{
		CompleteAddWidgetsToActor(Actor, ChangeContext, LoadedWidgetClassDefinition);
		return;
	}

	UContentDataSubsystem* ContentSubsystem =
		HUD->GetGameInstance()
			? HUD->GetGameInstance()->GetSubsystem<UContentDataSubsystem>()
			: nullptr;
	if (!ContentSubsystem)
	{
		Handles->PendingWidgetDefinitionsByActor.Remove(Actor);
		UE_LOG(
			PdGameFeatureAction_AddWidgetsLog,
			Error,
			TEXT("AddWidgets skipped '%s': ContentDataSubsystem was unavailable for the UI preload."),
			*GetNameSafe(Actor));
		return;
	}

	const TWeakObjectPtr<ThisClass> WeakThis(this);
	const TWeakObjectPtr<AActor> WeakActor(Actor);
	const TWeakObjectPtr<UWidgetClassDefinition> WeakDefinition(LoadedWidgetClassDefinition);
	TSharedPtr<FStreamableHandle> PreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			AssetPaths,
			FSimpleDelegate::CreateLambda(
				[WeakThis, WeakActor, WeakDefinition, ChangeContext]()
				{
					ThisClass* This = WeakThis.Get();
					if (This)
					{
						This->CompleteAddWidgetsToActor(
							WeakActor.Get(),
							ChangeContext,
							WeakDefinition.Get());
					}
				}));

	// RequestAsyncLoad can complete immediately when every asset is already resident.
	// Keep the handle for the initialized UI as well so unused preloaded assets stay
	// resident until this feature is removed.
	Handles = ContextHandles.Find(ChangeContext);
	if (PreloadHandle.IsValid()
		&& Handles
		&& (Handles->PendingWidgetDefinitionsByActor.Contains(Actor)
			|| Handles->WidgetDefinitionsByActor.Contains(Actor)))
	{
		Handles->WidgetPreloadHandlesByActor.Add(Actor, MoveTemp(PreloadHandle));
	}
}

void UGameFeatureAction_AddWidgets::CompleteAddWidgetsToActor(
	AActor* Actor,
	FGameFeatureStateChangeContext ChangeContext,
	UWidgetClassDefinition* ExpectedWidgetClassDefinition)
{
	APdHUD* HUD = Cast<APdHUD>(Actor);
	FPdGameFeatureWidgetHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!HUD || !Handles || !IsValid(ExpectedWidgetClassDefinition))
	{
		return;
	}

	TWeakObjectPtr<UWidgetClassDefinition> PendingDefinition;
	if (!Handles->PendingWidgetDefinitionsByActor.RemoveAndCopyValue(Actor, PendingDefinition)
		|| PendingDefinition.Get() != ExpectedWidgetClassDefinition)
	{
		return;
	}

	TArray<FSoftObjectPath> AssetPaths;
	ExpectedWidgetClassDefinition->GetRuntimePreloadAssetPaths(AssetPaths);
	for (const FSoftObjectPath& AssetPath : AssetPaths)
	{
		if (!AssetPath.ResolveObject())
		{
			UE_LOG(
				PdGameFeatureAction_AddWidgetsLog,
				Error,
				TEXT("UI preload completed without resolving '%s' for '%s'."),
				*AssetPath.ToString(),
				*GetNameSafe(ExpectedWidgetClassDefinition));
		}
	}

	HUD->InitializeUi(ExpectedWidgetClassDefinition);
	Handles->WidgetDefinitionsByActor.Add(Actor, ExpectedWidgetClassDefinition);
}

void UGameFeatureAction_AddWidgets::RemoveWidgetsFromActor(
	AActor* Actor,
	FGameFeatureStateChangeContext ChangeContext)
{
	APdHUD* HUD = Cast<APdHUD>(Actor);
	if (!HUD)
	{
		return;
	}

	FPdGameFeatureWidgetHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles)
	{
		return;
	}

	Handles->PendingWidgetDefinitionsByActor.Remove(Actor);
	if (TSharedPtr<FStreamableHandle> PreloadHandle;
		Handles->WidgetPreloadHandlesByActor.RemoveAndCopyValue(Actor, PreloadHandle)
		&& PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle->ReleaseHandle();
	}

	TWeakObjectPtr<UWidgetClassDefinition> WidgetDefinition;
	if (Handles->WidgetDefinitionsByActor.RemoveAndCopyValue(Actor, WidgetDefinition))
	{
		HUD->DeinitializeUi(WidgetDefinition.Get());
	}
}

void UGameFeatureAction_AddWidgets::RemoveAllWidgets(FPdGameFeatureWidgetHandles& Handles) const
{
	TArray<TWeakObjectPtr<AActor>> Actors;
	Handles.WidgetDefinitionsByActor.GetKeys(Actors);

	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		APdHUD* HUD = Cast<APdHUD>(Actor.Get());
		const TWeakObjectPtr<UWidgetClassDefinition>* WidgetDefinition = Handles.WidgetDefinitionsByActor.Find(Actor);
		if (HUD && WidgetDefinition)
		{
			HUD->DeinitializeUi(WidgetDefinition->Get());
		}
	}

	Handles.WidgetDefinitionsByActor.Reset();
	Handles.PendingWidgetDefinitionsByActor.Reset();

	for (TPair<TWeakObjectPtr<AActor>, TSharedPtr<FStreamableHandle>>& HandlePair :
		Handles.WidgetPreloadHandlesByActor)
	{
		if (HandlePair.Value.IsValid())
		{
			HandlePair.Value->CancelHandle();
			HandlePair.Value->ReleaseHandle();
		}
	}
	Handles.WidgetPreloadHandlesByActor.Reset();
}
