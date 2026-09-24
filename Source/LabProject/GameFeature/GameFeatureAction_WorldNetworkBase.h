#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureAction_WorldNetworkBase.generated.h"

class UGameInstance;
struct FWorldContext;

UCLASS(Abstract)
class LABPROJECT_API UGameFeatureAction_WorldNetworkBase : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UGameFeatureAction_WorldNetworkBase();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext);
	void HandleGameInstanceWorldChanged(UGameInstance* GameInstance, UWorld* OldWorld, UWorld* NewWorld,
		FGameFeatureStateChangeContext ChangeContext);

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
		PURE_VIRTUAL(UGameFeatureAction_WorldNetworkBase::AddToWorld, );

	bool ShouldApplyToNetMode(ENetMode NetMode) const;

public:
	UPROPERTY(EditAnywhere, Category = "Network")
	uint8 bClientAction : 1;

	UPROPERTY(EditAnywhere, Category = "Network")
	uint8 bServerAction : 1;

private:
	TMap<FGameFeatureStateChangeContext, FDelegateHandle> GameInstanceStartHandles;
	TMap<FGameFeatureStateChangeContext, FDelegateHandle> GameInstanceWorldChangedHandles;
};
