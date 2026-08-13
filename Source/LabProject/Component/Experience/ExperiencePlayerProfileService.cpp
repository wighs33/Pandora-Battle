#include "Component/Experience/ExperiencePlayerProfileService.h"

#include "Character/CharacterBase.h"
#include "Component/Experience/ExperienceMatchFlowComponent.h"
#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Skin/SkinDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperiencePlayerProfileService)

void UExperiencePlayerProfileService::ApplySettings(
	const FExperiencePlayerProvisioningSettings& InSettings)
{
	bAssignDefaultTeamWhenLobbyTeamMissing =
		InSettings.bAssignDefaultTeamWhenLobbyTeamMissing;
	DefaultLobbyTeamColorIndex =
		InSettings.DefaultLobbyTeamColorIndex;
}

void UExperiencePlayerProfileService::InitializeLoggedInPlayer(
	APlayerController* NewPlayer) const
{
	AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !NewPlayer)
	{
		return;
	}

	if (UPdGameInstance* PdGameInstance =
		GameMode->GetGameInstance<UPdGameInstance>())
	{
		const APlayerState* NewPlayerState = NewPlayer->PlayerState;
		const FString PlayerId = PdGameInstance->ResolveSavePlayerId(
			NewPlayer,
			NewPlayerState);
		if (NewPlayer->IsLocalController() && !PlayerId.IsEmpty())
		{
			PdGameInstance->LoadGame(PlayerId);
		}
		ApplyCachedLobbyPlayerIdentity(NewPlayer, *PdGameInstance);
	}
	if (APdPlayerController* PdPlayerController =
		Cast<APdPlayerController>(NewPlayer))
	{
		PdPlayerController->Client_RequestLocalCosmeticProfileSync();
	}

	ApplyInitialPlayerMapRegion(NewPlayer);
}

void UExperiencePlayerProfileService::ApplyCachedLobbyPlayerIdentity(
	APlayerController* NewPlayer,
	UPdGameInstance& PdGameInstance) const
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	APdPlayerState* PdPlayerState =
		NewPlayer ? NewPlayer->GetPlayerState<APdPlayerState>() : nullptr;
	UPlayerMatchComponent* PlayerMatchComponent =
		PdPlayerState ? PdPlayerState->GetPlayerMatchComponent() : nullptr;
	if (!GameMode || !NewPlayer || !PdPlayerState || !PlayerMatchComponent)
	{
		return;
	}

	FPlayerMatchIdentity CachedMatchIdentity;
	if (PdGameInstance.TryGetCachedPlayerMatchIdentityForPlayerState(
		NewPlayer->PlayerState,
		CachedMatchIdentity))
	{
		PlayerMatchComponent->SetPlayerMatchIdentity(CachedMatchIdentity);
	}

	if (PlayerMatchComponent->GetMatchDisplayName().IsEmpty())
	{
		const AGameStateBase* CurrentGameState =
			GameMode->GetGameState<AGameStateBase>();
		const int32 FallbackDisplayNameIndex =
			CurrentGameState ? CurrentGameState->PlayerArray.Num() : 1;
		PlayerMatchComponent->SetMatchDisplayName(
			PdGameInstance.ResolveDefaultPlayerNickname(
				NewPlayer,
				PdPlayerState,
				FallbackDisplayNameIndex));
	}

	if (bAssignDefaultTeamWhenLobbyTeamMissing
		&& PlayerMatchComponent->GetMatchTeamColorIndex() == INDEX_NONE)
	{
		PlayerMatchComponent->SetMatchTeamColorIndex(
			DefaultLobbyTeamColorIndex);
	}
}

void UExperiencePlayerProfileService::ApplyInitialPlayerMapRegion(
	APlayerController* NewPlayer) const
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !GameMode->HasAuthority() || !NewPlayer)
	{
		return;
	}

	APdPlayerState* PdPlayerState =
		NewPlayer->GetPlayerState<APdPlayerState>();
	UPlayerMatchComponent* PlayerMatchComponent =
		PdPlayerState ? PdPlayerState->GetPlayerMatchComponent() : nullptr;
	const UExperienceMatchFlowComponent* MatchFlow =
		GameMode->GetMatchFlowComponent();
	if (!PlayerMatchComponent || !MatchFlow)
	{
		return;
	}

	FLobbyMatchMapOption MapOption;
	if (MatchFlow->FindCurrentMatchMapOption(MapOption))
	{
		PlayerMatchComponent->SetPlayerMapRegion(
			MapOption.InitialPlayerMapRegion);
	}
}

void UExperiencePlayerProfileService::ApplyCachedLobbySkinEquipment(
	APlayerController* NewPlayer) const
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	if (!GameMode || !GameMode->HasAuthority() || !NewPlayer)
	{
		return;
	}

	UPdGameInstance* PdGameInstance =
		GameMode->GetGameInstance<UPdGameInstance>();
	APdPlayerState* PdPlayerState =
		NewPlayer->GetPlayerState<APdPlayerState>();
	ACharacterBase* PlayerCharacter =
		Cast<ACharacterBase>(NewPlayer->GetPawn());
	USkinComponent* SkinComponent =
		PdPlayerState ? PdPlayerState->GetSkinComponent() : nullptr;
	USkinEquipmentComponent* SkinEquipmentComponent =
		PlayerCharacter
			? PlayerCharacter->GetSkinEquipmentComponent()
			: nullptr;
	if (!PdGameInstance || !PdPlayerState || !PlayerCharacter
		|| !SkinComponent || !SkinEquipmentComponent)
	{
		return;
	}

	TMap<FGameplayTag, FName> EquippedSkinNamesBySlot;
	if (!PdGameInstance->TryGetCachedLobbyEquippedSkinSlotsForPlayerState(
			PdPlayerState,
			EquippedSkinNamesBySlot)
		|| EquippedSkinNamesBySlot.IsEmpty())
	{
		return;
	}

	TArray<USkinDefinition*> SkinDefinitionsToGrant;
	TMap<FGameplayTag, USkinDefinition*> SkinDefinitionsBySlot;
	for (const TPair<FGameplayTag, FName>& EquippedSkinPair
		: EquippedSkinNamesBySlot)
	{
		if (!EquippedSkinPair.Key.IsValid()
			|| EquippedSkinPair.Value.IsNone())
		{
			continue;
		}

		USkinDefinition* SkinDefinition =
			PdGameInstance->GetSkinDefinitionByName(
				EquippedSkinPair.Value);
		if (!SkinDefinition)
		{
			continue;
		}

		SkinDefinitionsToGrant.AddUnique(SkinDefinition);
		SkinDefinitionsBySlot.Add(
			EquippedSkinPair.Key,
			SkinDefinition);
	}

	if (SkinDefinitionsToGrant.IsEmpty())
	{
		return;
	}

	SkinComponent->AddSkinDefinitions(SkinDefinitionsToGrant);
	for (const TPair<FGameplayTag, USkinDefinition*>& SkinDefinitionPair
		: SkinDefinitionsBySlot)
	{
		if (USkinDefinition* SkinDefinition =
			SkinDefinitionPair.Value)
		{
			SkinEquipmentComponent->RequestEquipSkinDefinition(
				SkinDefinition,
				SkinDefinitionPair.Key);
		}
	}
}

AExperienceGameMode*
UExperiencePlayerProfileService::GetExperienceGameMode() const
{
	const UExperiencePlayerProvisioningComponent* Coordinator =
		GetTypedOuter<UExperiencePlayerProvisioningComponent>();
	return Coordinator
		? Cast<AExperienceGameMode>(Coordinator->GetOwner())
		: nullptr;
}
