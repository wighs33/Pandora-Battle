#pragma once

#include "CoreMinimal.h"
#include "ExtensionExecute.generated.h"

class AActor;
struct FAssetBundleData;

USTRUCT(BlueprintType, meta = (Hidden))
struct LABPROJECT_API FExtensionExecute
{
	GENERATED_BODY()

	virtual ~FExtensionExecute() = default;

	virtual void OnActivate(AActor* Owner) const {}
	virtual void OnDeactivate(AActor* Owner) const {}

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) const;
#endif
};
