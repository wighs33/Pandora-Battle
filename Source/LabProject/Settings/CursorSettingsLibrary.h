#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CursorSettingsLibrary.generated.h"

class APlayerController;

UCLASS()
class LABPROJECT_API UCursorSettingsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!Setting|Mouse Cursor", meta = (WorldContext = "WorldContextObject"))
	static bool ApplyConfiguredMouseCursor(UObject* WorldContextObject, APlayerController* PlayerController);
};
