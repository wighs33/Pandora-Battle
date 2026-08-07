#include "Component/Experience/ExperienceManagerComponent.h"

#include "Engine/AssetManager.h"
#include "Definition/Experience/ExperienceDefinition.h"
#include "GameFeaturePluginOperationResult.h"
#include "GameFeaturesSubsystem.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceManagerComponent)

DEFINE_LOG_CATEGORY(PdExperienceManagerLog);

namespace
{
	TMap<FString, TSet<TWeakObjectPtr<UExperienceManagerComponent>>> ExperienceGameFeatureReferences;

	void RemoveStaleExperienceGameFeatureReferences(
		TSet<TWeakObjectPtr<UExperienceManagerComponent>>& References)
	{
		for (auto Iterator = References.CreateIterator(); Iterator; ++Iterator)
		{
			if (!Iterator->IsValid())
			{
				Iterator.RemoveCurrent();
			}
		}
	}

	int32 AcquireExperienceGameFeatureReference(
		UExperienceManagerComponent* ExperienceManager,
		const FString& PluginURL)
	{
		TSet<TWeakObjectPtr<UExperienceManagerComponent>>& References =
			ExperienceGameFeatureReferences.FindOrAdd(PluginURL);
		RemoveStaleExperienceGameFeatureReferences(References);
		References.Add(TWeakObjectPtr<UExperienceManagerComponent>(ExperienceManager));
		return References.Num();
	}

	int32 ReleaseExperienceGameFeatureReference(
		UExperienceManagerComponent* ExperienceManager,
		const FString& PluginURL)
	{
		TSet<TWeakObjectPtr<UExperienceManagerComponent>>* References =
			ExperienceGameFeatureReferences.Find(PluginURL);
		if (!References)
		{
			return 0;
		}

		References->Remove(TWeakObjectPtr<UExperienceManagerComponent>(ExperienceManager));
		RemoveStaleExperienceGameFeatureReferences(*References);
		const int32 RemainingReferences = References->Num();
		if (RemainingReferences == 0)
		{
			ExperienceGameFeatureReferences.Remove(PluginURL);
		}
		return RemainingReferences;
	}
}

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

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UExperienceManagerComponent, CurrentExperienceId, Params);
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
		UE_LOG(PdExperienceManagerLog, Warning, TEXT("SetCurrentExperienceAuth ignored because the owner has no authority."));
		return;
	}

	if (!ExperienceId.IsValid())
	{
		UE_LOG(PdExperienceManagerLog, Error, TEXT("SetCurrentExperienceAuth ignored because ExperienceId is invalid."));
		return;
	}

	if (LoadState != EExperienceLoadState::Unloaded)
	{
		UE_LOG(
			PdExperienceManagerLog,
			Warning,
			TEXT("SetCurrentExperienceAuth ignored because an experience is already active or loading. Current=%s Requested=%s State=%d"),
			*CurrentExperienceId.ToString(),
			*ExperienceId.ToString(),
			static_cast<int32>(LoadState));
		return;
	}

	CurrentExperienceId = ExperienceId;
	MARK_PROPERTY_DIRTY_FROM_NAME(UExperienceManagerComponent, CurrentExperienceId, this);
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

FDelegateHandle UExperienceManagerComponent::CallOrRegister_OnExperienceLoadFailed(FOnPdExperienceLoadFailed::FDelegate Delegate)
{
	if (HasExperienceLoadFailed())
	{
		Delegate.ExecuteIfBound(LastFailedExperienceId, LastLoadFailureMessage);
		return FDelegateHandle();
	}

	return OnExperienceLoadFailed.Add(MoveTemp(Delegate));
}

void UExperienceManagerComponent::RemoveOnExperienceLoaded(const FDelegateHandle DelegateHandle)
{
	if (DelegateHandle.IsValid())
	{
		OnExperienceLoaded.Remove(DelegateHandle);
	}
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
	if (!CurrentExperienceId.IsValid() || LoadState != EExperienceLoadState::Unloaded)
	{
		return;
	}

	LoadState = EExperienceLoadState::Loading;
	LastFailedExperienceId = FPrimaryAssetId();
	LastLoadFailureMessage.Reset();

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

	if (LoadState != EExperienceLoadState::Loading)
	{
		return;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	CurrentExperience = Cast<UExperienceDefinition>(AssetManager.GetPrimaryAssetObject(CurrentExperienceId));

	if (!CurrentExperience)
	{
		FailExperienceLoad(
			LoadedExperienceId,
			FString::Printf(TEXT("Experience asset could not be loaded: %s"), *LoadedExperienceId.ToString()));
		return;
	}

	StartGameFeatureLoads();
}

void UExperienceManagerComponent::StartGameFeatureLoads()
{
	LoadState = EExperienceLoadState::LoadingGameFeatures;
	GameFeaturePluginURLs.Reset();
	PendingGameFeatureLoadCount = 0;

	if (!CurrentExperience)
	{
		FailExperienceLoad(CurrentExperienceId, TEXT("Experience is missing before GameFeature load."));
		return;
	}

	UGameFeaturesSubsystem& GameFeaturesSubsystem = UGameFeaturesSubsystem::Get();
	for (const FPrimaryAssetId& GameFeatureId : CurrentExperience->GameFeaturesToEnable)
	{
		if (!GameFeatureId.IsValid())
		{
			FailExperienceLoad(
				CurrentExperienceId,
				FString::Printf(TEXT("Experience contains an invalid GameFeature id: %s"), *CurrentExperienceId.ToString()));
			return;
		}

		const FString PluginName = GameFeatureId.PrimaryAssetName.ToString();
		FString PluginURL;
		if (!GameFeaturesSubsystem.GetPluginURLByName(PluginName, PluginURL))
		{
			FailExperienceLoad(
				CurrentExperienceId,
				FString::Printf(TEXT("GameFeature plugin not found: %s"), *PluginName));
			return;
		}

		if (GameFeaturePluginURLs.Contains(PluginURL))
		{
			continue;
		}

		GameFeaturePluginURLs.Add(PluginURL);
		AcquireExperienceGameFeatureReference(this, PluginURL);
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
	if (LoadState != EExperienceLoadState::LoadingGameFeatures || !GameFeaturePluginURLs.Contains(PluginURL))
	{
		return;
	}

	if (Result.HasError())
	{
		FailExperienceLoad(
			CurrentExperienceId,
			FString::Printf(TEXT("GameFeature load failed: %s (%s)"), *PluginURL, *Result.GetError()));
		return;
	}

	PendingGameFeatureLoadCount = FMath::Max(PendingGameFeatureLoadCount - 1, 0);
	if (PendingGameFeatureLoadCount <= 0)
	{
		FinishExperienceLoad();
	}
}

void UExperienceManagerComponent::FinishExperienceLoad()
{
	if (LoadState != EExperienceLoadState::LoadingGameFeatures)
	{
		return;
	}

	LoadState = EExperienceLoadState::Loaded;
	LastFailedExperienceId = FPrimaryAssetId();
	LastLoadFailureMessage.Reset();
	OnExperienceLoaded.Broadcast(CurrentExperience);
	OnExperienceLoaded.Clear();
	OnExperienceLoadFailed.Clear();
}

void UExperienceManagerComponent::FailExperienceLoad(FPrimaryAssetId FailedExperienceId, FString FailureMessage)
{
	if (LoadState == EExperienceLoadState::Failed || LoadState == EExperienceLoadState::Deactivating)
	{
		return;
	}

	LastFailedExperienceId = FailedExperienceId;
	LastLoadFailureMessage = MoveTemp(FailureMessage);
	UE_LOG(PdExperienceManagerLog, Error, TEXT("%s"), *LastLoadFailureMessage);

	ReleaseGameFeaturePluginReferences();

	if (CurrentExperienceId.IsValid() && UAssetManager::IsInitialized())
	{
		UAssetManager::Get().UnloadPrimaryAsset(CurrentExperienceId);
	}

	ExperienceLoadHandle.Reset();
	CurrentExperience = nullptr;
	GameFeaturePluginURLs.Reset();
	PendingGameFeatureLoadCount = 0;
	LoadState = EExperienceLoadState::Failed;

	OnExperienceLoaded.Clear();
	OnExperienceLoadFailed.Broadcast(LastFailedExperienceId, LastLoadFailureMessage);
	OnExperienceLoadFailed.Clear();
}

void UExperienceManagerComponent::DeactivateExperience()
{
	if (LoadState == EExperienceLoadState::Unloaded || LoadState == EExperienceLoadState::Deactivating)
	{
		return;
	}

	LoadState = EExperienceLoadState::Deactivating;

	ReleaseGameFeaturePluginReferences();

	if (CurrentExperienceId.IsValid() && UAssetManager::IsInitialized())
	{
		UAssetManager::Get().UnloadPrimaryAsset(CurrentExperienceId);
	}

	ExperienceLoadHandle.Reset();
	CurrentExperience = nullptr;
	CurrentExperienceId = FPrimaryAssetId();
	if (const AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UExperienceManagerComponent, CurrentExperienceId, this);
	}
	GameFeaturePluginURLs.Reset();
	PendingGameFeatureLoadCount = 0;
	LastFailedExperienceId = FPrimaryAssetId();
	LastLoadFailureMessage.Reset();
	OnExperienceLoaded.Clear();
	OnExperienceLoadFailed.Clear();
	LoadState = EExperienceLoadState::Unloaded;
}

void UExperienceManagerComponent::ReleaseGameFeaturePluginReferences()
{
	UGameFeaturesSubsystem& GameFeaturesSubsystem = UGameFeaturesSubsystem::Get();
	for (const FString& PluginURL : GameFeaturePluginURLs)
	{
		const int32 RemainingReferences =
			ReleaseExperienceGameFeatureReference(this, PluginURL);

		if (RemainingReferences == 0)
		{
			GameFeaturesSubsystem.DeactivateGameFeaturePlugin(PluginURL);
		}
	}
}
