#include "Pandora/PandoraDefaultUnlockPolicy.h"

#include "Definition/Pandora/PandoraDefinition.h"

namespace
{
	const TArray<FName>& DefaultUnlockedPandoraKeys()
	{
		static const TArray<FName> Keys = {
			TEXT("Fire"),
			TEXT("Freeze"),
			TEXT("Darkness"),
			TEXT("Light"),
			TEXT("Machine"),
			TEXT("Lightning")
		};
		return Keys;
	}
}

const TArray<FName>& PandoraDefaultUnlockPolicy::GetDefaultUnlockedPandoraKeys()
{
	return DefaultUnlockedPandoraKeys();
}

FString PandoraDefaultUnlockPolicy::NormalizePandoraKey(const FName PandoraName)
{
	FString PandoraKey = PandoraName.ToString();
	PandoraKey.RemoveFromStart(TEXT("DA_Pandora_"), ESearchCase::IgnoreCase);
	PandoraKey.RemoveFromStart(TEXT("Pandora_"), ESearchCase::IgnoreCase);
	return PandoraKey;
}

bool PandoraDefaultUnlockPolicy::IsDefaultUnlockedPandoraName(const FName PandoraName)
{
	if (PandoraName.IsNone())
	{
		return false;
	}

	const FString NormalizedPandoraKey = NormalizePandoraKey(PandoraName);
	for (const FName DefaultPandoraKey : GetDefaultUnlockedPandoraKeys())
	{
		if (NormalizedPandoraKey.Equals(NormalizePandoraKey(DefaultPandoraKey), ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool PandoraDefaultUnlockPolicy::IsDefaultUnlockedPandoraDefinition(const UPandoraDefinition* PandoraDefinition)
{
	return IsValid(PandoraDefinition) && IsDefaultUnlockedPandoraName(PandoraDefinition->GetFName());
}
