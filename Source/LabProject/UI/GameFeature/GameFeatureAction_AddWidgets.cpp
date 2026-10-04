#include "UI/GameFeature/GameFeatureAction_AddWidgets.h"

#include "AssetRegistry/AssetBundleData.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFeature/ActorExtensionWorldSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "UI/HUD/PdHUD.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "UI/Core/UiSubsystem.h"
#include "Data/ContentLease.h"

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
	FGameFeatureWidgetHandles* Handles = ContextHandles.Find(ChangeContext);
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
		AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient,
			TargetHudClass.ToSoftObjectPath().GetAssetPath());
	}

	if (!WidgetClassDefinition.IsNull())
	{
		AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient,
			WidgetClassDefinition.ToSoftObjectPath().GetAssetPath());
	}
}
#endif

void UGameFeatureAction_AddWidgets::AddToWorld(const FWorldContext& WorldContext,
	const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if (!World || !World->IsGameWorld() || TargetHudClass.IsNull())
	{
		return;
	}

	RegisterWidgetExtension(World, ChangeContext);
}

void UGameFeatureAction_AddWidgets::RegisterWidgetExtension(UWorld* World, FGameFeatureStateChangeContext ChangeContext)
{
	if (!World || TargetHudClass.IsNull())
	{
		return;
	}

	UActorExtensionWorldSubsystem* ExtensionSubsystem = World->GetSubsystem<UActorExtensionWorldSubsystem>();
	// 기반 클래스는 초기화를 마친 게임 월드에서만 부르므로, 없으면 확장을 지원하지 않는 월드 종류다.
	if (!ExtensionSubsystem)
	{
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

	FGameFeatureWidgetHandles& Handles = ContextHandles.FindOrAdd(ChangeContext);

	FActorExtensionSpec ExtensionSpec;
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

void UGameFeatureAction_AddWidgets::AddWidgetsToActor(AActor* Actor, FGameFeatureStateChangeContext ChangeContext)
{
	APdHUD* HUD = Cast<APdHUD>(Actor);
	if (!HUD)
	{
		return;
	}

	FGameFeatureWidgetHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles || Handles->WidgetDefinitionsByActor.Contains(Actor)
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

	Handles->PendingWidgetDefinitionsByActor.Add(Actor, LoadedWidgetClassDefinition);
	APlayerController* PlayerController = HUD->GetOwningPlayerController();
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UUiSubsystem* UiSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
	if (!UiSubsystem)
	{
		Handles->PendingWidgetDefinitionsByActor.Remove(Actor);
		UE_LOG(PdGameFeatureAction_AddWidgetsLog, Error,
			TEXT("AddWidgets skipped '%s': UiSubsystem was unavailable for the UI preload."),
			*GetNameSafe(Actor));
		return;
	}

	const TWeakObjectPtr<ThisClass> WeakThis(this);
	const TWeakObjectPtr<AActor> WeakActor(Actor);
	const TWeakObjectPtr<UWidgetClassDefinition> WeakDefinition(LoadedWidgetClassDefinition);
	const FSimpleDelegate CompletionDelegate = FSimpleDelegate::CreateLambda(
		[WeakThis, WeakActor, WeakDefinition, ChangeContext]()
		{
			if (ThisClass* This = WeakThis.Get())
			{
				This->CompleteAddWidgetsToActor(WeakActor.Get(), ChangeContext, WeakDefinition.Get());
			}
		});

	TArray<TSharedPtr<FContentLease>> ContentLeases;
	for (const EUiContentGroup Group : {EUiContentGroup::Core, EUiContentGroup::InGame})
	{
		TSharedPtr<FContentLease> Lease =
			UiSubsystem->AcquireUiContent(LoadedWidgetClassDefinition, Group, CompletionDelegate);
		if (!Lease.IsValid())
		{
			Handles->PendingWidgetDefinitionsByActor.Remove(Actor);
			UE_LOG(PdGameFeatureAction_AddWidgetsLog, Error,
				TEXT("AddWidgets skipped '%s': UI content group lease acquisition failed."),
				*GetNameSafe(Actor));
			return;
		}
		ContentLeases.Add(MoveTemp(Lease));
	}
	Handles->WidgetContentLeasesByActor.Add(Actor, MoveTemp(ContentLeases));
	CompleteAddWidgetsToActor(Actor, ChangeContext, LoadedWidgetClassDefinition);
}

void UGameFeatureAction_AddWidgets::CompleteAddWidgetsToActor(AActor* Actor,
	FGameFeatureStateChangeContext ChangeContext, UWidgetClassDefinition* ExpectedWidgetClassDefinition)
{
	APdHUD* HUD = Cast<APdHUD>(Actor);
	FGameFeatureWidgetHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!HUD || !Handles || !IsValid(ExpectedWidgetClassDefinition))
	{
		return;
	}

	const TWeakObjectPtr<UWidgetClassDefinition>* PendingDefinition =
		Handles->PendingWidgetDefinitionsByActor.Find(Actor);
	const TArray<TSharedPtr<FContentLease>>* ContentLeases = Handles->WidgetContentLeasesByActor.Find(Actor);
	if (!PendingDefinition || PendingDefinition->Get() != ExpectedWidgetClassDefinition || !ContentLeases)
	{
		return;
	}

	for (const TSharedPtr<FContentLease>& Lease : *ContentLeases)
	{
		if (!Lease.IsValid() || Lease->HasFailed())
		{
			UE_LOG(PdGameFeatureAction_AddWidgetsLog, Error,
				TEXT("HUD initialization was canceled because a Core/InGame UI content group for '%s' failed."),
				*GetNameSafe(ExpectedWidgetClassDefinition));
			Handles->PendingWidgetDefinitionsByActor.Remove(Actor);
			Handles->WidgetContentLeasesByActor.Remove(Actor);
			return;
		}
		if (!Lease->IsReady())
		{
			return;
		}
	}

	Handles->PendingWidgetDefinitionsByActor.Remove(Actor);
	HUD->InitializeUi(ExpectedWidgetClassDefinition);
	Handles->WidgetDefinitionsByActor.Add(Actor, ExpectedWidgetClassDefinition);
}

void UGameFeatureAction_AddWidgets::RemoveWidgetsFromActor(AActor* Actor, FGameFeatureStateChangeContext ChangeContext)
{
	APdHUD* HUD = Cast<APdHUD>(Actor);
	if (!HUD)
	{
		return;
	}

	FGameFeatureWidgetHandles* Handles = ContextHandles.Find(ChangeContext);
	if (!Handles)
	{
		return;
	}

	Handles->PendingWidgetDefinitionsByActor.Remove(Actor);
	Handles->WidgetContentLeasesByActor.Remove(Actor);

	TWeakObjectPtr<UWidgetClassDefinition> WidgetDefinition;
	if (Handles->WidgetDefinitionsByActor.RemoveAndCopyValue(Actor, WidgetDefinition))
	{
		HUD->DeinitializeUi(WidgetDefinition.Get());
	}
}

void UGameFeatureAction_AddWidgets::RemoveAllWidgets(FGameFeatureWidgetHandles& Handles) const
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
	Handles.WidgetContentLeasesByActor.Reset();
}
