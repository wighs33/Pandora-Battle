#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "GameFeature/Extension/Condition/ExtensionCondition.h"
#include "GameFeature/Extension/Execute/ExtensionExecute.h"
#include "ActorExtension.generated.h"

class AActor;
struct FAssetBundleData;

USTRUCT(BlueprintType)
struct LABPROJECT_API FActorExtension
{
	GENERATED_BODY()

	bool CanActivate(AActor* Owner) const;
	void OnActivate(AActor* Owner);
	void OnDeactivate(AActor* Owner);
	bool IsActivated() const { return bActivated; }

#if WITH_EDITORONLY_DATA
	void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) const;
#endif

	UPROPERTY(EditAnywhere, Category = "Extension")
	TArray<TInstancedStruct<FExtensionCondition>> Conditions;

	UPROPERTY(EditAnywhere, Category = "Extension")
	TArray<TInstancedStruct<FExtensionExecute>> Executes;

private:
	bool bActivated = false;
};
