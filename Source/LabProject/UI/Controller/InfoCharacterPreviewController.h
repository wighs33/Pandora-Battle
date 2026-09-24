#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InfoCharacterPreviewController.generated.h"

class AActor;
class UInfoWidget;

/** Manages the temporary character-preview view target used by the Info screen. */
UCLASS()
class LABPROJECT_API UInfoCharacterPreviewController : public UObject
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	void Initialize(
		UInfoWidget* InOwnerWidget,
		bool bInUsePreviewCamera,
		TSubclassOf<AActor> InPreviewClass);
	void ShowPreview();
	void ReturnCameraToPawn() const;
	void Shutdown(bool bReturnCamera);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ResolvePreviewClass();

private:
	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> OwnerWidget;
	UPROPERTY(Transient)
	TSubclassOf<AActor> PreviewClass;
	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedPreview;
	bool bUsePreviewCamera = true;
};
