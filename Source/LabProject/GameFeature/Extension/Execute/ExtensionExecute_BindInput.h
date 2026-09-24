#pragma once

#include "CoreMinimal.h"
#include "GameFeature/Extension/Execute/ExtensionExecute.h"
#include "ExtensionExecute_BindInput.generated.h"

class UControllerInputComponent;
class UControllerInputDefinition;

USTRUCT(BlueprintType, meta = (DisplayName = "Bind Input"))
struct LABPROJECT_API FExtensionExecute_BindInput : public FExtensionExecute
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) const override;
#endif

	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual void OnActivate(AActor* Owner) const override;
	virtual void OnDeactivate(AActor* Owner) const override;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	UControllerInputComponent* FindControllerInputComponent(AActor* Owner) const;

public:
	UPROPERTY(EditAnywhere, Category = "Input", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UControllerInputDefinition> InputDefinition;

private:
	mutable TWeakObjectPtr<UControllerInputComponent> WeakInputComponent;
	mutable TSoftObjectPtr<UControllerInputDefinition> PreviousInputDefinition;
};
