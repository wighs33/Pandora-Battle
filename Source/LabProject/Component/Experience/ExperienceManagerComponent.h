#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "GameFeaturePluginOperationResult.h"
#include "UObject/PrimaryAssetId.h"
#include "ExperienceManagerComponent.generated.h"

class UExperienceDefinition;
struct FStreamableHandle;

DECLARE_LOG_CATEGORY_EXTERN(PdExperienceManagerLog, Log, All);

UENUM(BlueprintType)
enum class EPdExperienceLoadState : uint8
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	Loaded,
	Failed,
	Deactivating
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPdExperienceLoaded, const UExperienceDefinition*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPdExperienceLoadFailed, FPrimaryAssetId, const FString&);

UCLASS()
class LABPROJECT_API UExperienceManagerComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Events
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Experience API
	void SetCurrentExperienceAuth(FPrimaryAssetId ExperienceId);
	bool IsExperienceLoaded() const { return LoadState == EPdExperienceLoadState::Loaded; }
	bool HasExperienceLoadFailed() const { return LoadState == EPdExperienceLoadState::Failed; }
	EPdExperienceLoadState GetLoadState() const { return LoadState; }
	const UExperienceDefinition* GetCurrentExperienceChecked() const;
	FDelegateHandle CallOrRegister_OnExperienceLoaded(FOnPdExperienceLoaded::FDelegate Delegate);
	FDelegateHandle CallOrRegister_OnExperienceLoadFailed(FOnPdExperienceLoadFailed::FDelegate Delegate);
	void RemoveOnExperienceLoaded(FDelegateHandle DelegateHandle);

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Replication Events
	UFUNCTION()
	void HandleCurrentExperienceIdReplicated();

	//------------------------------------------------------------------------------------------------------------------
	//--- Load Flow
	void StartExperienceLoad();
	void HandleExperienceAssetLoaded(FPrimaryAssetId LoadedExperienceId);
	void StartGameFeatureLoads();
	void HandleGameFeatureLoaded(const UE::GameFeatures::FResult& Result, FString PluginURL);
	void FinishExperienceLoad();
	void FailExperienceLoad(FPrimaryAssetId FailedExperienceId, FString FailureMessage);
	void DeactivateExperience();
	void ReleaseGameFeaturePluginReferences();

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Replicated State
	UPROPERTY(ReplicatedUsing = HandleCurrentExperienceIdReplicated)
	FPrimaryAssetId CurrentExperienceId;

	//------------------------------------------------------------------------------------------------------------------
	//--- Runtime State
	UPROPERTY(Transient)
	TObjectPtr<const UExperienceDefinition> CurrentExperience;

	EPdExperienceLoadState LoadState = EPdExperienceLoadState::Unloaded;
	TSharedPtr<FStreamableHandle> ExperienceLoadHandle;
	TArray<FString> GameFeaturePluginURLs;
	int32 PendingGameFeatureLoadCount = 0;
	FPrimaryAssetId LastFailedExperienceId;
	FString LastLoadFailureMessage;
	FOnPdExperienceLoaded OnExperienceLoaded;
	FOnPdExperienceLoadFailed OnExperienceLoadFailed;
};
