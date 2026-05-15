#include "GameFeature/GameFeatureAction_SetProjectTagConfig.h"

#include "AssetRegistry/AssetBundleData.h"
#include "Common/ProjectTagConfig.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFeature/ProjectTagConfigWorldSubsystem.h"
#include "GameFeaturesSubsystemSettings.h"
#include "Item/InventoryComponent.h"
#include "Pandora/PandoraComponent.h"
#include "Skin/SkinComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_SetProjectTagConfig)

DEFINE_LOG_CATEGORY(PdGameFeatureAction_SetProjectTagConfigLog);

UGameFeatureAction_SetProjectTagConfig::UGameFeatureAction_SetProjectTagConfig()
{
	bClientAction = true;
	bServerAction = true;
}

void UGameFeatureAction_SetProjectTagConfig::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
	Super::OnGameFeatureDeactivating(Context);

	const FGameFeatureStateChangeContext ChangeContext(Context);
	FPdProjectTagConfigActionState* State = ContextStates.Find(ChangeContext);
	if (!State)
	{
		return;
	}

	for (const TPair<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UProjectTagConfig>>& Pair : State->PreviousConfigsByWorld)
	{
		UWorld* World = Pair.Key.Get();
		if (!World)
		{
			continue;
		}

		UProjectTagConfigWorldSubsystem* Subsystem = World->GetSubsystem<UProjectTagConfigWorldSubsystem>();
		if (!Subsystem)
		{
			continue;
		}

		if (UProjectTagConfig* PreviousConfig = Pair.Value.Get())
		{
			Subsystem->SetProjectTagConfig(PreviousConfig);
			ApplyConfigToWorld(World, PreviousConfig);
		}
		else
		{
			Subsystem->ClearProjectTagConfig(nullptr);
			ApplyConfigToWorld(World, UProjectTagConfig::GetDefaultConfig());
		}
	}

	ContextStates.Remove(ChangeContext);
}

#if WITH_EDITOR
EDataValidationResult UGameFeatureAction_SetProjectTagConfig::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	if (!bClientAction && !bServerAction)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_SetProjectTagConfig", "MissingNetwork", "At least one Network option is required."));
	}

	if (ProjectTagConfig.IsNull())
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(NSLOCTEXT("PdGameFeatureAction_SetProjectTagConfig", "MissingConfig", "Project Tag Config is required."));
	}

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void UGameFeatureAction_SetProjectTagConfig::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData)
{
	if (ProjectTagConfig.IsNull())
	{
		return;
	}

	const FSoftObjectPath ConfigPath = ProjectTagConfig.ToSoftObjectPath();
	AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateClient, ConfigPath.GetAssetPath());
	AssetBundleData.AddBundleAsset(UGameFeaturesSubsystemSettings::LoadStateServer, ConfigPath.GetAssetPath());
}
#endif

void UGameFeatureAction_SetProjectTagConfig::AddToWorld(
	const FWorldContext& WorldContext,
	const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if (!World || !World->IsGameWorld() || ProjectTagConfig.IsNull())
	{
		return;
	}

	UProjectTagConfig* LoadedConfig = ProjectTagConfig.LoadSynchronous();
	if (!LoadedConfig)
	{
		UE_LOG(PdGameFeatureAction_SetProjectTagConfigLog, Error, TEXT("SetProjectTagConfig skipped: failed to load '%s'."),
			*ProjectTagConfig.ToString());
		return;
	}

	UProjectTagConfigWorldSubsystem* Subsystem = World->GetSubsystem<UProjectTagConfigWorldSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FPdProjectTagConfigActionState& State = ContextStates.FindOrAdd(ChangeContext);
	if (!State.PreviousConfigsByWorld.Contains(World))
	{
		State.PreviousConfigsByWorld.Add(World, const_cast<UProjectTagConfig*>(Subsystem->GetProjectTagConfig()));
	}

	Subsystem->SetProjectTagConfig(LoadedConfig);
	ApplyConfigToWorld(World, LoadedConfig);
}

void UGameFeatureAction_SetProjectTagConfig::ApplyConfigToWorld(UWorld* World, const UProjectTagConfig* Config) const
{
	if (!World)
	{
		return;
	}

	const UProjectTagConfig* EffectiveConfig = Config ? Config : UProjectTagConfig::GetDefaultConfig();
	for (TActorIterator<AActor> ActorIterator(World); ActorIterator; ++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		if (!Actor)
		{
			continue;
		}

		if (UInventoryComponent* InventoryComponent = Actor->FindComponentByClass<UInventoryComponent>())
		{
			InventoryComponent->ApplyProjectTagConfig(EffectiveConfig);
		}

		if (UPandoraComponent* PandoraComponent = Actor->FindComponentByClass<UPandoraComponent>())
		{
			PandoraComponent->ApplyProjectTagConfig(EffectiveConfig);
		}

		if (USkinComponent* SkinComponent = Actor->FindComponentByClass<USkinComponent>())
		{
			SkinComponent->ApplyProjectTagConfig(EffectiveConfig);
		}
	}
}
