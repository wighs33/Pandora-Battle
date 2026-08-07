#include "Pandora/PandoraDefaultUnlockPolicy.h"

#include "Definition/Pandora/PandoraDefinition.h"
#include "Definition/Provision/DefaultProvisionDefinition.h"

const TArray<FName>& PandoraDefaultUnlockPolicy::GetDefaultUnlockedPandoraKeys()
{
	static TArray<FName> DefaultUnlockedPandoraKeys;
	UDefaultProvisionDefinition::ResolveDefaultDefinition()
		->GetPandoraKeys(
			EDefaultProvisionMode::Gameplay,
			DefaultUnlockedPandoraKeys);
	return DefaultUnlockedPandoraKeys;
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

	return UDefaultProvisionDefinition::ResolveDefaultDefinition()
		->IsPandoraKeyGranted(
			PandoraName,
			EDefaultProvisionMode::Gameplay);
}

bool PandoraDefaultUnlockPolicy::IsDefaultUnlockedPandoraDefinition(const UPandoraDefinition* PandoraDefinition)
{
	return IsValid(PandoraDefinition) && IsDefaultUnlockedPandoraName(PandoraDefinition->GetFName());
}
