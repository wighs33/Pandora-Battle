#pragma once

#include "CoreMinimal.h"
#include "KillLogTypes.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FKillLogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog")
	FText KillerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog")
	FText VictimName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog")
	bool bSelfKill = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog")
	bool bEnvironmentKill = false;
};
