#include "Skin/SkinDefaultUnlockPolicy.h"

#include "Definition/Skin/SkinDefinition.h"

namespace
{
	const TArray<FName>& DefaultUnlockedSkinNames()
	{
		static const TArray<FName> Names = {
			TEXT("DA_HandRaising"),
			TEXT("DA_SantaHat")
		};
		return Names;
	}
}

const TArray<FName>& SkinDefaultUnlockPolicy::GetDefaultUnlockedSkinNames()
{
	return DefaultUnlockedSkinNames();
}

bool SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinName(const FName SkinName)
{
	if (SkinName.IsNone())
	{
		return false;
	}

	for (const FName DefaultSkinName : GetDefaultUnlockedSkinNames())
	{
		if (SkinName.IsEqual(DefaultSkinName, ENameCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool SkinDefaultUnlockPolicy::IsDefaultUnlockedSkinDefinition(const USkinDefinition* SkinDefinition)
{
	return IsValid(SkinDefinition) && IsDefaultUnlockedSkinName(SkinDefinition->GetFName());
}
