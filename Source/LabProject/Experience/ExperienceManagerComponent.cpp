#include "Experience/ExperienceManagerComponent.h"

#include "Engine/AssetManager.h"
#include "Experience/ExperienceDefinition.h"
#include "GameFeaturePluginOperationResult.h"
#include "GameFeaturesSubsystem.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceManagerComponent)

DEFINE_LOG_CATEGORY(PdExperienceManagerLog);

UExperienceManagerComponent::UExperienceManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

//----------------------------------------------------------------------------------------------------------------------
//--- Engine Events
void UExperienceManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UExperienceManagerComponent, CurrentExperienceId);
}

void UExperienceManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeactivateExperience();

	Super::EndPlay(EndPlayReason);
}

//----------------------------------------------------------------------------------------------------------------------
//--- Experience API
void UExperienceManagerComponent::SetCurrentExperienceAuth(FPrimaryAssetId ExperienceId)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (!ExperienceId.IsValid())
	{
		UE_LOG(PdExperienceManagerLog, Warning, TEXT("SetCurrentExperienceAuth failed: invalid ExperienceId."));
		return;
	}

	if (LoadState != EPdExperienceLoadState::Unloaded)
	{
		UE_LOG(PdExperienceManagerLog, Warning, TEXT("SetCurrentExperienceAuth skipped: experience is already loading or loaded."));
		return;
	}

	CurrentExperienceId = ExperienceId;
	StartExperienceLoad();
}

const UExperienceDefinition* UExperienceManagerComponent::GetCurrentExperienceChecked() const
{
	check(IsExperienceLoaded());
	check(CurrentExperience);
	return CurrentExperience;
}

FDelegateHandle UExperienceManagerComponent::CallOrRegister_OnExperienceLoaded(FOnPdExperienceLoaded::FDelegate Delegate)
{
	if (IsExperienceLoaded())
	{
		Delegate.ExecuteIfBound(CurrentExperience);
		return FDelegateHandle();
	}

	return OnExperienceLoaded.Add(MoveTemp(Delegate));
}

//----------------------------------------------------------------------------------------------------------------------
//--- Replication Events
void UExperienceManagerComponent::HandleCurrentExperienceIdReplicated()
{
	StartExperienceLoad();
}

//----------------------------------------------------------------------------------------------------------------------
//--- Load Flow
void UExperienceManagerComponent::StartExperienceLoad()
{
	if (!CurrentExperienceId.IsValid() || LoadState != EPdExperienceLoadState::Unloaded)
	{
		return;
	}

	LoadState = EPdExperienceLoadState::Loading;

	UAssetManager& AssetManager = UAssetManager::Get();

	ExperienceLoadHandle = AssetManager.LoadPrimaryAsset(
		CurrentExperienceId,
		TArray<FName>(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleExperienceAssetLoaded, CurrentExperienceId));

	if (!ExperienceLoadHandle.IsValid())
	{
		HandleExperienceAssetLoaded(CurrentExperienceId);
	}
}

void UExperienceManagerComponent::HandleExperienceAssetLoaded(FPrimaryAssetId LoadedExperienceId)
{
	if (LoadedExperienceId != CurrentExperienceId)
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	CurrentExperience = Cast<UExperienceDefinition>(AssetManager.GetPrimaryAssetObject(CurrentExperienceId));
	if (!CurrentExperience)
	{
		const FSoftObjectPath ExperiencePath = AssetManager.GetPrimaryAssetPath(CurrentExperienceId);
		CurrentExperience = Cast<UExperienceDefinition>(ExperiencePath.TryLoad());
	}

	if (!CurrentExperience)
	{
		UE_LOG(PdExperienceManagerLog, Error, TEXT("Experience load failed: %s"), *CurrentExperienceId.ToString());
		LoadState = EPdExperienceLoadState::Unloaded;
		return;
	}

	StartGameFeatureLoads();
}

void UExperienceManagerComponent::StartGameFeatureLoads()
{
	LoadState = EPdExperienceLoadState::LoadingGameFeatures;
	GameFeaturePluginURLs.Reset();
	PendingGameFeatureLoadCount = 0;

	if (!CurrentExperience)
	{
		FinishExperienceLoad();
		return;
	}

	UGameFeaturesSubsystem& GameFeaturesSubsystem = UGameFeaturesSubsystem::Get();
	for (const FPrimaryAssetId& GameFeatureId : CurrentExperience->GameFeaturesToEnable)
	{
		if (!GameFeatureId.IsValid())
		{
			continue;
		}

		const FString PluginName = GameFeatureId.PrimaryAssetName.ToString();
		FString PluginURL;
		if (!GameFeaturesSubsystem.GetPluginURLByName(PluginName, PluginURL))
		{
			UE_LOG(PdExperienceManagerLog, Error, TEXT("GameFeature plugin not found: %s"), *PluginName);
			continue;
		}

		GameFeaturePluginURLs.AddUnique(PluginURL);
		++PendingGameFeatureLoadCount;

		GameFeaturesSubsystem.LoadAndActivateGameFeaturePlugin(
			PluginURL,
			FGameFeaturePluginLoadComplete::CreateUObject(this, &ThisClass::HandleGameFeatureLoaded, PluginURL));
	}

	if (PendingGameFeatureLoadCount <= 0)
	{
		FinishExperienceLoad();
	}
}

void UExperienceManagerComponent::HandleGameFeatureLoaded(const UE::GameFeatures::FResult& Result, FString PluginURL)
{
	if (Result.HasError())
	{
		UE_LOG(PdExperienceManagerLog, Error, TEXT("GameFeature load failed: %s (%s)"), *PluginURL, *Result.GetError());
	}

	--PendingGameFeatureLoadCount;
	if (PendingGameFeatureLoadCount <= 0)
	{
		FinishExperienceLoad();
	}
}

void UExperienceManagerComponent::FinishExperienceLoad()
{
	if (LoadState != EPdExperienceLoadState::LoadingGameFeatures)
	{
		return;
	}

	LoadState = EPdExperienceLoadState::Loaded;
	OnExperienceLoaded.Broadcast(CurrentExperience);
	OnExperienceLoaded.Clear();
}

void UExperienceManagerComponent::DeactivateExperience()
{
	if (LoadState == EPdExperienceLoadState::Unloaded || LoadState == EPdExperienceLoadState::Deactivating)
	{
		return;
	}

	LoadState = EPdExperienceLoadState::Deactivating;

	UGameFeaturesSubsystem& GameFeaturesSubsystem = UGameFeaturesSubsystem::Get();
	for (const FString& PluginURL : GameFeaturePluginURLs)
	{
		GameFeaturesSubsystem.DeactivateGameFeaturePlugin(PluginURL);
	}

	if (CurrentExperienceId.IsValid() && UAssetManager::IsInitialized())
	{
		UAssetManager::Get().UnloadPrimaryAsset(CurrentExperienceId);
	}

	ExperienceLoadHandle.Reset();
	CurrentExperience = nullptr;
	CurrentExperienceId = FPrimaryAssetId();
	GameFeaturePluginURLs.Reset();
	PendingGameFeatureLoadCount = 0;
	LoadState = EPdExperienceLoadState::Unloaded;
}
