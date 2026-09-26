#include "Component/Match/MatchPlayerSetupComponent.h"

#include "Data/ContentDataSubsystem.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Provision/DefaultPlayerProvisioner.h"

#include "Character/CharacterBase.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Definition/Skin/SkinDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "Mode/ExperienceGameMode.h"
#include "SavedGameData/PlayerProfileSubsystem.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MatchPlayerSetupComponent)

UMatchPlayerSetupComponent::UMatchPlayerSetupComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	DefaultPlayerProvisioner =
		CreateDefaultSubobject<UDefaultPlayerProvisioner>(
			TEXT("DefaultPlayerProvisioner"));

	// GameMode 외부에서 생성해도 동일한 기본 설정을 사용한다.
	ApplySettings(FMatchPlayerSetupSettings());
}

void UMatchPlayerSetupComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginSkinContentPreload();
}

void UMatchPlayerSetupComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseSkinContentPreload();
	PendingGameplayPlayers.Reset();
	ReadyGameplayPawns.Reset();
	OnPlayerGameplayReady.Clear();
	bSkinContentReady = false;
	DefaultPlayerProvisioner->OnPlayerProvisioned.RemoveAll(this);
	DefaultPlayerProvisioner->Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UMatchPlayerSetupComponent::ApplySettings(
	const FMatchPlayerSetupSettings& InSettings)
{
	CachedSettings = InSettings;
	if (DefaultPlayerProvisioner && InSettings.DefaultProvisionDefinition)
	{
		DefaultPlayerProvisioner->Initialize(InSettings.DefaultProvisionDefinition,
			IsTrainingRoomMap() ? EDefaultProvisionMode::TrainingRoom : EDefaultProvisionMode::Gameplay);
	}
}

void UMatchPlayerSetupComponent::InitializeLoggedInPlayer(
	APlayerController* NewPlayer)
{
	AExperienceGameMode* GameMode = Cast<AExperienceGameMode>(GetOwner());
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

void UMatchPlayerSetupComponent::InitializeMatchIdentity(APlayerController* NewPlayer)
{
	const AExperienceGameMode* GameMode = Cast<AExperienceGameMode>(GetOwner());
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

	if (CachedSettings.bAssignDefaultTeamWhenLobbyTeamMissing
		&& PlayerMatchComponent->GetMatchTeamColorIndex() == INDEX_NONE)
	{
		PlayerMatchComponent->SetMatchTeamColorIndex(
			CachedSettings.DefaultLobbyTeamColorIndex);
	}
}

// 로딩 중의 입장 요청은 중복 없이 보관하고, 같은 Pawn에 완료된 지급은 반복하지 않는다.
void UMatchPlayerSetupComponent::PreparePlayerForGameplay(APlayerController* NewPlayer)
{
	if (!IsValid(NewPlayer) || !IsValid(NewPlayer->GetPawn()) || IsPlayerReadyForGameplay(NewPlayer))
	{
		return;
	}

	if (!DefaultPlayerProvisioner->OnPlayerProvisioned.IsBoundToObject(this))
	{
		DefaultPlayerProvisioner->OnPlayerProvisioned.AddUObject(this, &ThisClass::HandlePlayerProvisioned);
	}
	if (!bSkinContentReady)
	{
		PendingGameplayPlayers.AddUnique(NewPlayer);
		BeginSkinContentPreload();
		return;
	}
	PreparePlayerForGameplayInternal(NewPlayer);
}

// 로비에서 선택한 외형을 현재 Pawn에 복원한 뒤 게임 모드에 맞는 기본 지급을 실행한다.
void UMatchPlayerSetupComponent::PreparePlayerForGameplayInternal(APlayerController* NewPlayer)
{
	if (!IsValid(NewPlayer) || !IsValid(NewPlayer->GetPawn()) || IsPlayerReadyForGameplay(NewPlayer))
	{
		return;
	}

	ApplyCachedLobbySkinEquipment(NewPlayer);
	DefaultPlayerProvisioner->ProvisionPlayer(NewPlayer);
}

// 지급 함수 호출 시점이 아니라 비동기 지급까지 성공한 시점에 현재 Pawn을 준비 완료로 기록한다.
void UMatchPlayerSetupComponent::HandlePlayerProvisioned(APlayerController* PlayerController)
{
	if (!bSkinContentReady || !IsValid(PlayerController) || !IsValid(PlayerController->GetPawn()))
	{
		return;
	}

	ReadyGameplayPawns.Add(PlayerController, PlayerController->GetPawn());
	OnPlayerGameplayReady.Broadcast();
}

// 이전 Pawn의 준비 완료 상태가 새 Pawn까지 준비됐다고 오인되지 않도록 인스턴스를 비교한다.
bool UMatchPlayerSetupComponent::IsPlayerReadyForGameplay(APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController) || !IsValid(PlayerController->GetPawn()))
	{
		return false;
	}

	const TWeakObjectPtr<APawn>* ReadyPawn = ReadyGameplayPawns.Find(PlayerController);
	return ReadyPawn && ReadyPawn->Get() == PlayerController->GetPawn();
}

// 퇴장한 참가자의 대기·완료 기록과 지급 재시도를 함께 제거한다.
void UMatchPlayerSetupComponent::ClearRuntimeStateForController(AController* Controller, APlayerState* PlayerState)
{
	if (!Controller)
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		PendingGameplayPlayers.Remove(PlayerController);
		ReadyGameplayPawns.Remove(PlayerController);
	}
	DefaultPlayerProvisioner->ClearRuntimeStateForController(Controller, PlayerState);
}

void UMatchPlayerSetupComponent::BeginSkinContentPreload()
{
	if (bSkinContentReady || bSkinContentLoadPending)
	{
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		bSkinContentReady = true;
		FlushPendingGameplayProvisions();
		return;
	}

	// 로비 외형 이름을 해석할 스킨 카탈로그만 준비한다. 지급 에셋은 Provisioner가 로드한다.
	TArray<FPrimaryAssetId> ContentIds;
	ContentSubsystem->GetSkinDefinitionIds(ContentIds);

	bSkinContentLoadPending = true;
	TSharedPtr<FStreamableHandle> NewLoadHandle =
		ContentSubsystem->PreloadPrimaryAssetsAsync(
			ContentIds,
			FSimpleDelegate::CreateUObject(
				this,
				&ThisClass::HandleSkinContentPreloaded));
	if (NewLoadHandle.IsValid() && bSkinContentLoadPending)
	{
		SkinContentLoadHandle = MoveTemp(NewLoadHandle);
	}
}

void UMatchPlayerSetupComponent::HandleSkinContentPreloaded()
{
	bSkinContentLoadPending = false;
	bSkinContentReady = true;
	FlushPendingGameplayProvisions();
}

void UMatchPlayerSetupComponent::ReleaseSkinContentPreload()
{
	bSkinContentLoadPending = false;
	if (SkinContentLoadHandle.IsValid())
	{
		SkinContentLoadHandle->CancelHandle();
		SkinContentLoadHandle->ReleaseHandle();
		SkinContentLoadHandle.Reset();
	}
}

// 스킨 카탈로그 로딩이 끝나면 기다리던 참가자의 외형 복원과 지급을 재개한다.
void UMatchPlayerSetupComponent::FlushPendingGameplayProvisions()
{
	if (!bSkinContentReady)
	{
		return;
	}

	TArray<TWeakObjectPtr<APlayerController>> PendingPlayers = MoveTemp(PendingGameplayPlayers);
	PendingGameplayPlayers.Reset();
	for (const TWeakObjectPtr<APlayerController>& Player : PendingPlayers)
	{
		PreparePlayerForGameplayInternal(Player.Get());
	}
}

bool UMatchPlayerSetupComponent::IsTrainingRoomMap() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const ULevelDefinition* Levels =
		CachedSettings.LevelDefinition.Get();
	if (!Levels && !CachedSettings.LevelDefinition.IsNull())
	{
		Levels = CachedSettings.LevelDefinition.LoadSynchronous();
	}
	if (!Levels)
	{
		Levels = ULevelDefinition::ResolveDefaultDefinition();
	}
	return Levels
		&& Levels->IsTrainingRoomMapName(
			UGameplayStatics::GetCurrentLevelName(World, true));
}


void UMatchPlayerSetupComponent::ApplyCachedLobbySkinEquipment(
	APlayerController* NewPlayer) const
{
	const AExperienceGameMode* GameMode = Cast<AExperienceGameMode>(GetOwner());
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
