#include "Component/Experience/ExperiencePlayerProfileService.h"

#include "Character/CharacterBase.h"
#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Experience/ExperienceGameModeSettings.h"
#include "Definition/Skin/SkinDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Mode/ExperienceGameMode.h"
#include "Engine/GameInstance.h"
#include "Data/ContentDataSubsystem.h"
#include "SavedGameData/PlayerProfileSubsystem.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
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

	if (UPlayerProfileSubsystem* ProfileSubsystem =
		UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GameMode->GetGameInstance()))
	{
		const APlayerState* NewPlayerState = NewPlayer->PlayerState;
		const FString PlayerId = ProfileSubsystem->ResolveSavePlayerId(
			NewPlayer,
			NewPlayerState);
		if (NewPlayer->IsLocalController() && !PlayerId.IsEmpty())
		{
			ProfileSubsystem->LoadGame(PlayerId);
		}
	}
	if (APdPlayerController* PdPlayerController =
		Cast<APdPlayerController>(NewPlayer))
	{
		PdPlayerController->Client_RequestLocalCosmeticProfileSync();
	}
}

void UExperiencePlayerProfileService::InitializeMatchIdentity(APlayerController* NewPlayer) const
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	const ULobbyRuntimeSubsystem* LobbySubsystem =
		GameMode ? UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance()) : nullptr;
	APdPlayerState* PdPlayerState = NewPlayer ? NewPlayer->GetPlayerState<APdPlayerState>() : nullptr;
	UPlayerMatchComponent* PlayerMatchComponent = PdPlayerState ? PdPlayerState->GetPlayerMatchComponent() : nullptr;
	if (!GameMode || !GameMode->HasAuthority() || !PlayerMatchComponent)
	{
		return;
	}

	// 로비 캐시가 없어도 기본 이름과 팀은 초기화하며, 전달받은 식별 정보는 덮어쓰지 않는다.
	FPlayerMatchIdentity CachedMatchIdentity;
	if (LobbySubsystem && PlayerMatchComponent->GetPlayerMatchIdentity().Matches(FPlayerMatchIdentity())
		&& LobbySubsystem->TryGetCachedPlayerMatchIdentityForPlayerState(PdPlayerState, CachedMatchIdentity))
	{
		PlayerMatchComponent->SetPlayerMatchIdentity(CachedMatchIdentity);
	}

	if (PlayerMatchComponent->GetMatchDisplayName().IsEmpty())
	{
		int32 FallbackDisplayNameIndex = 1;
		if (const AGameStateBase* CurrentGameState = GameMode->GetGameState<AGameStateBase>())
		{
			const int32 PlayerIndex = CurrentGameState->PlayerArray.IndexOfByKey(PdPlayerState);
			FallbackDisplayNameIndex = PlayerIndex != INDEX_NONE ? PlayerIndex + 1 : CurrentGameState->PlayerArray.Num() + 1;
		}
		const FText DefaultNickname = LobbySubsystem
			? LobbySubsystem->ResolveDefaultPlayerNickname(NewPlayer, PdPlayerState, FallbackDisplayNameIndex)
			: FText::Format(NSLOCTEXT("Lobby", "DefaultNicknameFormat", "User{0}"), FallbackDisplayNameIndex);
		PlayerMatchComponent->SetMatchDisplayName(DefaultNickname);
	}

	if (bAssignDefaultTeamWhenLobbyTeamMissing
		&& PlayerMatchComponent->GetMatchTeamColorIndex() == INDEX_NONE)
	{
		PlayerMatchComponent->SetMatchTeamColorIndex(
			DefaultLobbyTeamColorIndex);
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

	ULobbyRuntimeSubsystem* LobbySubsystem = UGameInstance::GetSubsystem<ULobbyRuntimeSubsystem>(GameMode->GetGameInstance());
	UContentDataSubsystem* ContentDataSubsystem = UGameInstance::GetSubsystem<UContentDataSubsystem>(GameMode->GetGameInstance());
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
	if (!LobbySubsystem || !ContentDataSubsystem || !PdPlayerState || !PlayerCharacter
		|| !SkinComponent || !SkinEquipmentComponent)
	{
		return;
	}

	TMap<FGameplayTag, FName> EquippedSkinNamesBySlot;
	if (!LobbySubsystem->TryGetCachedLobbyEquippedSkinSlotsForPlayerState(
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
			ContentDataSubsystem->GetSkinDefinitionByName(
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
