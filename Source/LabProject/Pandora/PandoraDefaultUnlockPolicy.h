#pragma once

#include "CoreMinimal.h"

class UPandoraDefinition;

namespace PandoraDefaultUnlockPolicy
{
	LABPROJECT_API FString NormalizePandoraKey(FName PandoraName);
	LABPROJECT_API bool IsDefaultUnlockedPandoraName(FName PandoraName);
	LABPROJECT_API bool IsDefaultUnlockedPandoraDefinition(const UPandoraDefinition* PandoraDefinition);
}
