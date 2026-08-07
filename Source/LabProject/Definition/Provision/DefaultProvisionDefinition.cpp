#include "Definition/Provision/DefaultProvisionDefinition.h"

#include "Definition/Mode/PdGameInstanceDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DefaultProvisionDefinition)

namespace
{
	FString NormalizePandoraKey(const FName PandoraKeyName)
	{
		FString PandoraKey = PandoraKeyName.ToString();
		PandoraKey.RemoveFromStart(TEXT("DA_Pandora_"), ESearchCase::IgnoreCase);
		PandoraKey.RemoveFromStart(TEXT("Pandora_"), ESearchCase::IgnoreCase);
		return PandoraKey;
	}

}

int32 FDefaultProvisionModeCounts::GetCount(
	const EDefaultProvisionMode Mode) const
{
	switch (Mode)
	{
	case EDefaultProvisionMode::Lobby:
		return FMath::Max(Lobby, 0);
	case EDefaultProvisionMode::TrainingRoom:
		return FMath::Max(TrainingRoom, 0);
	case EDefaultProvisionMode::Gameplay:
		return FMath::Max(Gameplay, 0);
	default:
		return 0;
	}
}

float FDefaultProvisionModeValues::GetValue(
	const EDefaultProvisionMode Mode) const
{
	switch (Mode)
	{
	case EDefaultProvisionMode::Lobby:
		return FMath::Max(Lobby, 0.0f);
	case EDefaultProvisionMode::TrainingRoom:
		return FMath::Max(TrainingRoom, 0.0f);
	case EDefaultProvisionMode::Gameplay:
		return FMath::Max(Gameplay, 0.0f);
	default:
		return 0.0f;
	}
}

bool FDefaultProvisionModeFlags::IsEnabled(
	const EDefaultProvisionMode Mode) const
{
	switch (Mode)
	{
	case EDefaultProvisionMode::Lobby:
		return Lobby;
	case EDefaultProvisionMode::TrainingRoom:
		return TrainingRoom;
	case EDefaultProvisionMode::Gameplay:
		return Gameplay;
	default:
		return false;
	}
}

int32 FDefaultProvisionModeLevels::GetLevel(
	const EDefaultProvisionMode Mode) const
{
	switch (Mode)
	{
	case EDefaultProvisionMode::Lobby:
		return FMath::Max(Lobby, static_cast<int32>(INDEX_NONE));
	case EDefaultProvisionMode::TrainingRoom:
		return FMath::Max(
			TrainingRoom,
			static_cast<int32>(INDEX_NONE));
	case EDefaultProvisionMode::Gameplay:
		return FMath::Max(Gameplay, static_cast<int32>(INDEX_NONE));
	default:
		return INDEX_NONE;
	}
}

UDefaultProvisionDefinition::UDefaultProvisionDefinition()
{
	StatusPointValues.TrainingRoom = 50.0f;
	SoulDustValues.TrainingRoom = 100;
	GrantAllWeapons.TrainingRoom = true;
	GrantAllEquipment.TrainingRoom = true;
}

FPrimaryAssetId UDefaultProvisionDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("DefaultProvisionDefinition"), GetFName());
}

FSoftObjectPath UDefaultProvisionDefinition::GetDefaultDefinitionPath()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.DefaultProvision.ToSoftObjectPath();
}

const UDefaultProvisionDefinition* UDefaultProvisionDefinition::ResolveDefaultDefinition()
{
	const FSoftObjectPath DefinitionPath = GetDefaultDefinitionPath();
	if (!DefinitionPath.IsValid())
	{
		return GetDefault<UDefaultProvisionDefinition>();
	}

	if (const UDefaultProvisionDefinition* LoadedDefinition =
		Cast<UDefaultProvisionDefinition>(DefinitionPath.ResolveObject()))
	{
		return LoadedDefinition;
	}

	const UDefaultProvisionDefinition* LoadedDefinition =
		Cast<UDefaultProvisionDefinition>(DefinitionPath.TryLoad());
	return LoadedDefinition
		? LoadedDefinition
		: GetDefault<UDefaultProvisionDefinition>();
}

void UDefaultProvisionDefinition::GetPandoraKeys(
	const EDefaultProvisionMode Mode,
	TArray<FName>& OutPandoraKeys) const
{
	OutPandoraKeys.Reset();
	for (const FDefaultProvisionPandoraGrant& PandoraGrant : PandoraGrants)
	{
		if (PandoraGrant.PandoraDefinitionId.IsValid()
			&& PandoraGrant.Levels.GetLevel(Mode) >= 0)
		{
			OutPandoraKeys.AddUnique(
				PandoraGrant.PandoraDefinitionId.PrimaryAssetName);
		}
	}
}

bool UDefaultProvisionDefinition::HasPandoraGrants(
	const EDefaultProvisionMode Mode) const
{
	return PandoraGrants.ContainsByPredicate(
		[Mode](const FDefaultProvisionPandoraGrant& PandoraGrant)
		{
			return PandoraGrant.PandoraDefinitionId.IsValid()
				&& PandoraGrant.Levels.GetLevel(Mode) >= 0;
		});
}

bool UDefaultProvisionDefinition::IsPandoraKeyGranted(
	const FName PandoraKeyName,
	const EDefaultProvisionMode Mode) const
{
	if (PandoraKeyName.IsNone())
	{
		return false;
	}

	const FString NormalizedCandidate = NormalizePandoraKey(PandoraKeyName);
	return PandoraGrants.ContainsByPredicate(
		[Mode, &NormalizedCandidate](
			const FDefaultProvisionPandoraGrant& PandoraGrant)
		{
			if (!PandoraGrant.PandoraDefinitionId.IsValid()
				|| PandoraGrant.Levels.GetLevel(Mode) < 0)
			{
				return false;
			}

			return NormalizedCandidate.Equals(
				NormalizePandoraKey(
					PandoraGrant.PandoraDefinitionId.PrimaryAssetName),
				ESearchCase::IgnoreCase);
		});
}
