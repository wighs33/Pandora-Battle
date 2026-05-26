#include "GameFeature/GameFeatureAction_AddWidgets.h"

#include "AssetRegistry/AssetBundleData.h"
#include "Engine/World.h"
#include "GameFeature/PdActorExtensionWorldSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "Mode/PdHUD.h"
#include "TimerManager.h"
#include "UI/WidgetClassDefinition.h"

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

	UPdActorExtensionWorldSubsystem* ExtensionSubsystem = World->GetSubsystem<UPdActorExtensionWorldSubsystem>();
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

	TSubclassOf<APdHUD> LoadedTargetClass = TargetHudClass.LoadSynchronous();
	if (!LoadedTargetClass)
	{
		UE_LOG(PdGameFeatureAction_AddWidgetsLog, Error, TEXT("AddWidgets skipped '%s': failed to load target HUD class."),
			*TargetHudClass.ToString());
		return;
	}

	FPdGameFeatureWidgetHandles& Handles = ContextHandles.FindOrAdd(ChangeContext);

	FPdActorExtensionSpec ExtensionSpec;
	ExtensionSpec.DebugName = GetFName();
	ExtensionSpec.CanActivate = FPdActorExtensionCanActivate::CreateUObject(this, &ThisClass::CanActivateWidgetExtension);
	ExtensionSpec.OnActivate = FPdActorExtensionExecute::CreateUObject(this, &ThisClass::AddWidgetsToActor, ChangeContext);
	ExtensionSpec.OnDeactivate = FPdActorExtensionExecute::CreateUObject(this, &ThisClass::RemoveWidgetsFromActor, ChangeContext);

	if (TSharedPtr<FPdActorExtensionHandle> ExtensionHandle = ExtensionSubsystem->RegisterExtensionForClass(LoadedTargetClass, MoveTemp(ExtensionSpec)))
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
	if (!Handles || Handles->WidgetDefinitionsByActor.Contains(Actor))
	{
		return;
	}

	UWidgetClassDefinition* LoadedWidgetClassDefinition = WidgetClassDefinition.LoadSynchronous();
	if (!LoadedWidgetClassDefinition)
	{
		UE_LOG(PdGameFeatureAction_AddWidgetsLog, Error, TEXT("AddWidgets skipped '%s': failed to load WidgetClassDefinition."),
			*GetNameSafe(Actor));
		return;
	}

	HUD->InitializeUi(LoadedWidgetClassDefinition);
	Handles->WidgetDefinitionsByActor.Add(Actor, LoadedWidgetClassDefinition);
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
}
