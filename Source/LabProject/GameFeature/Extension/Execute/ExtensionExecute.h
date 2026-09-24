#pragma once

#include "CoreMinimal.h"
#include "ExtensionExecute.generated.h"

class AActor;
struct FAssetBundleData;

USTRUCT(BlueprintType, meta = (Hidden))
struct LABPROJECT_API FExtensionExecute
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	virtual ~FExtensionExecute() = default;

#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) const;
#endif

	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnActivate(AActor* Owner) const {}
	virtual void OnDeactivate(AActor* Owner) const {}
};
