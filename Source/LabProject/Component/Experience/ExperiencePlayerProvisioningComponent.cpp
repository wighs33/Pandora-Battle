#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"

#include "Component/Experience/ExperiencePlayerProfileService.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Provision/DefaultPlayerProvisioner.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperiencePlayerProvisioningComponent)

UExperiencePlayerProvisioningComponent::UExperiencePlayerProvisioningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	PlayerProfileService =
		CreateDefaultSubobject<UExperiencePlayerProfileService>(
			TEXT("PlayerProfileService"));
	DefaultPlayerProvisioner =
		CreateDefaultSubobject<UDefaultPlayerProvisioner>(
			TEXT("DefaultPlayerProvisioner"));

	// GameMode 외부에서 생성해도 동일한 기본 설정을 사용한다.
	ApplySettings(FExperiencePlayerProvisioningSettings());
}

void UExperiencePlayerProvisioningComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginProvisioningContentPreload();
}

void UExperiencePlayerProvisioningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseProvisioningContentPreload();
	PendingGameplayPlayers.Reset();
	ReadyGameplayPawns.Reset();
	OnPlayerGameplayReady.Clear();
	bProvisioningContentReady = false;
	DefaultPlayerProvisioner->OnPlayerProvisioned.RemoveAll(this);
	DefaultPlayerProvisioner->Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UExperiencePlayerProvisioningComponent::ApplySettings(
	const FExperiencePlayerProvisioningSettings& InSettings)
{
	CachedSettings = InSettings;
	if (PlayerProfileService)
	{
		PlayerProfileService->ApplySettings(InSettings);
	}
	if (DefaultPlayerProvisioner)
	{
		DefaultPlayerProvisioner->SetDefinition(
			InSettings.DefaultProvisionDefinition);
	}
}

void UExperiencePlayerProvisioningComponent::InitializeLoggedInPlayer(
	APlayerController* NewPlayer)
{
	if (PlayerProfileService)
	{
		PlayerProfileService->InitializeLoggedInPlayer(NewPlayer);
	}
}

void UExperiencePlayerProvisioningComponent::InitializeMatchIdentity(APlayerController* NewPlayer)
{
	if (PlayerProfileService)
	{
		PlayerProfileService->InitializeMatchIdentity(NewPlayer);
	}
}

// 로딩 중의 입장 요청은 중복 없이 보관하고, 같은 Pawn에 완료된 지급은 반복하지 않는다.
void UExperiencePlayerProvisioningComponent::PreparePlayerForGameplay(APlayerController* NewPlayer)
{
	if (!IsValid(NewPlayer) || !IsValid(NewPlayer->GetPawn()) || IsPlayerReadyForGameplay(NewPlayer))
	{
		return;
	}

	if (!DefaultPlayerProvisioner->OnPlayerProvisioned.IsBoundToObject(this))
	{
		DefaultPlayerProvisioner->OnPlayerProvisioned.AddUObject(this, &ThisClass::HandlePlayerProvisioned);
	}
	if (!bProvisioningContentReady)
	{
		PendingGameplayPlayers.AddUnique(NewPlayer);
		BeginProvisioningContentPreload();
		return;
	}
	PreparePlayerForGameplayInternal(NewPlayer);
}

// 로비에서 선택한 외형을 현재 Pawn에 복원한 뒤 게임 모드에 맞는 기본 지급을 실행한다.
void UExperiencePlayerProvisioningComponent::PreparePlayerForGameplayInternal(APlayerController* NewPlayer)
{
	if (!IsValid(NewPlayer) || !IsValid(NewPlayer->GetPawn()) || IsPlayerReadyForGameplay(NewPlayer))
	{
		return;
	}

	PlayerProfileService->ApplyCachedLobbySkinEquipment(NewPlayer);
	const EDefaultProvisionMode Mode = IsTrainingRoomMap() ? EDefaultProvisionMode::TrainingRoom : EDefaultProvisionMode::Gameplay;
	DefaultPlayerProvisioner->ProvisionPlayer(NewPlayer, Mode);
}

// 지급 함수 호출 시점이 아니라 비동기 지급까지 성공한 시점에 현재 Pawn을 준비 완료로 기록한다.
void UExperiencePlayerProvisioningComponent::HandlePlayerProvisioned(APlayerController* PlayerController)
{
	if (!bProvisioningContentReady || !IsValid(PlayerController) || !IsValid(PlayerController->GetPawn()))
	{
		return;
	}

	ReadyGameplayPawns.Add(PlayerController, PlayerController->GetPawn());
	OnPlayerGameplayReady.Broadcast();
}

// 이전 Pawn의 준비 완료 상태가 새 Pawn까지 준비됐다고 오인되지 않도록 인스턴스를 비교한다.
bool UExperiencePlayerProvisioningComponent::IsPlayerReadyForGameplay(APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController) || !IsValid(PlayerController->GetPawn()))
	{
		return false;
	}

	const TWeakObjectPtr<APawn>* ReadyPawn = ReadyGameplayPawns.Find(PlayerController);
	return ReadyPawn && ReadyPawn->Get() == PlayerController->GetPawn();
}

// 퇴장한 참가자의 대기·완료 기록과 지급 재시도를 함께 제거한다.
void UExperiencePlayerProvisioningComponent::ClearRuntimeStateForController(AController* Controller, APlayerState* PlayerState)
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

void UExperiencePlayerProvisioningComponent::BeginProvisioningContentPreload()
{
	if (bProvisioningContentReady || bProvisioningContentLoadPending)
	{
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		bProvisioningContentReady = true;
		FlushPendingGameplayProvisions();
		return;
	}

	TArray<FPrimaryAssetId> ContentIds;
	TArray<FPrimaryAssetId> TypeContentIds;
	const auto AppendUniqueContentIds = [&ContentIds](
		const TArray<FPrimaryAssetId>& AssetIds)
	{
		for (const FPrimaryAssetId& AssetId : AssetIds)
		{
			ContentIds.AddUnique(AssetId);
		}
	};
	ContentSubsystem->GetSkillDataAssetIds(TypeContentIds);
	AppendUniqueContentIds(TypeContentIds);
	ContentSubsystem->GetPandoraDefinitionIds(TypeContentIds);
	AppendUniqueContentIds(TypeContentIds);
	ContentSubsystem->GetSkinDefinitionIds(TypeContentIds);
	AppendUniqueContentIds(TypeContentIds);

	bProvisioningContentLoadPending = true;
	TSharedPtr<FStreamableHandle> NewLoadHandle =
		ContentSubsystem->PreloadPrimaryAssetsAsync(
			ContentIds,
			FSimpleDelegate::CreateUObject(
				this,
				&ThisClass::HandleProvisioningContentPreloaded));
	if (NewLoadHandle.IsValid() && bProvisioningContentLoadPending)
	{
		ProvisioningContentLoadHandle = MoveTemp(NewLoadHandle);
	}
}

void UExperiencePlayerProvisioningComponent::HandleProvisioningContentPreloaded()
{
	bProvisioningContentLoadPending = false;
	bProvisioningContentReady = true;
	FlushPendingGameplayProvisions();
}

void UExperiencePlayerProvisioningComponent::ReleaseProvisioningContentPreload()
{
	bProvisioningContentLoadPending = false;
	if (ProvisioningContentLoadHandle.IsValid())
	{
		ProvisioningContentLoadHandle->CancelHandle();
		ProvisioningContentLoadHandle->ReleaseHandle();
		ProvisioningContentLoadHandle.Reset();
	}
}

// 공통 콘텐츠 로딩이 끝나면 기다리던 참가자들의 지급을 재개한다.
void UExperiencePlayerProvisioningComponent::FlushPendingGameplayProvisions()
{
	if (!bProvisioningContentReady)
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

void UExperiencePlayerProvisioningComponent::ApplyConfiguredStatusPointsForPlayerState(APlayerState* PlayerState)
{
	if (DefaultPlayerProvisioner)
	{
		DefaultPlayerProvisioner
			->ApplyConfiguredStatusPointsForPlayerState(
				PlayerState,
				IsTrainingRoomMap()
					? EDefaultProvisionMode::TrainingRoom
					: EDefaultProvisionMode::Gameplay);
	}
}

bool UExperiencePlayerProvisioningComponent::IsTrainingRoomMap() const
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
