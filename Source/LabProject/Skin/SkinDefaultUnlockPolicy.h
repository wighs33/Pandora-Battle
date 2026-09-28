#pragma once

#include "CoreMinimal.h"

class USkinDefinition;

namespace SkinDefaultUnlockPolicy
{
	LABPROJECT_API const TArray<FName>& GetDefaultUnlockedSkinNames();
	LABPROJECT_API bool IsDefaultUnlockedSkinDefinition(const USkinDefinition* SkinDefinition);
}
