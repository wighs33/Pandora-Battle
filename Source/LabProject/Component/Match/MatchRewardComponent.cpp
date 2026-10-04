#include "Component/Match/MatchRewardComponent.h"

#include "Component/Match/MatchOutcomeRules.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Item/RewardDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Item/RewardChest.h"
#include "Mode/ExperienceGameMode.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Profile/PlayerProfileSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MatchRewardComponent)

DEFINE_LOG_CATEGORY_STATIC(LogMatchReward, Log, All);

namespace
{
	AController* FindControllerForPlayerState(const UWorld* World, const APlayerState* PlayerState)
	{
		if (!World || !PlayerState)
		{
			return nullptr;
		}

		for (FConstControllerIterator Iterator = World->GetControllerIterator(); Iterator; ++Iterator)
		{
			AController* Controller = Iterator->Get();
			if (Controller && Controller->PlayerState == PlayerState)
			{
				return Controller;
			}
		}
		return nullptr;
	}
}

UMatchRewardComponent::UMatchRewardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMatchRewardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindChestConfigurationEvents();
	RewardContentLease.Reset();

	Super::EndPlay(EndPlayReason);
}

AExperienceGameMode* UMatchRewardComponent::GetExperienceGameMode() const
{
	return Cast<AExperienceGameMode>(GetOwner());
}

void UMatchRewardComponent::PreloadRewardContent()
{
	const TSoftObjectPtr<URewardDefinition>& Reward = GetExperienceGameMode()->GetChestSpawnRewardDefinition();
	if (Reward.IsNull())
	{
		HandleRewardContentLoaded();
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		HandleRewardContentLoaded();
		return;
	}

	RewardContentLease = ContentSubsystem->AcquireContent(TArray<FSoftObjectPath>{Reward.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRewardContentLoaded));
}

void UMatchRewardComponent::StopChestConfiguration()
{
	bChestConfigurationStopped = true;
	UnbindChestConfigurationEvents();
}

void UMatchRewardComponent::UnbindChestConfigurationEvents()
{
	if (UWorld* World = GetWorld(); World && WorldBeginPlayHandle.IsValid())
	{
		World->OnWorldBeginPlay.Remove(WorldBeginPlayHandle);
	}
	WorldBeginPlayHandle.Reset();
	for (const TPair<TWeakObjectPtr<ARewardChest>, FDelegateHandle>& Pending : PendingChestContentHandles)
	{
		if (ARewardChest* RewardChest = Pending.Key.Get())
		{
			RewardChest->OnRewardContentReady().Remove(Pending.Value);
		}
	}
	PendingChestContentHandles.Reset();
}

// 선택적 상자 설정만 로드하고, 맵의 모든 액터가 BeginPlay를 마친 뒤 배치를 적용한다.
void UMatchRewardComponent::HandleRewardContentLoaded()
{
	if (bChestConfigurationStopped)
	{
		return;
	}

	const TSoftObjectPtr<URewardDefinition>& Reward = GetExperienceGameMode()->GetChestSpawnRewardDefinition();
	if (!Reward.IsNull() && !Reward.IsValid())
	{
		UE_LOG(LogMatchReward, Error, TEXT("Chest spawn reward definition failed to load: %s"), *Reward.ToString());
		return;
	}
	UWorld* World = GetWorld();
	if (World && World->GetBegunPlay())
	{
		ConfigureRewardChestSpawns();
	}
	else if (World && !WorldBeginPlayHandle.IsValid())
	{
		WorldBeginPlayHandle = World->OnWorldBeginPlay.AddUObject(this, &ThisClass::HandleWorldBeginPlay);
	}
}

void UMatchRewardComponent::HandleWorldBeginPlay()
{
	UnbindChestConfigurationEvents();
	ConfigureRewardChestSpawns();
}

// 맵에 놓인 상자 중 보상 설정이 정한 수만큼만 무작위로 남기고 나머지는 이번 경기에서 끈다.
void UMatchRewardComponent::ConfigureRewardChestSpawns()
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	UWorld* World = GetWorld();
	if (bChestConfigurationStopped || !GameMode || !GameMode->HasAuthority() || !World)
	{
		return;
	}

	TArray<ARewardChest*> RewardChests;
	for (TActorIterator<ARewardChest> Iterator(World); Iterator; ++Iterator)
	{
		if (ARewardChest* RewardChest = *Iterator; IsValid(RewardChest))
		{
			RewardChests.Add(RewardChest);
		}
	}
	RewardChests.Sort([](const ARewardChest& A, const ARewardChest& B)
	{
		return A.GetName() < B.GetName();
	});

	// 공용 설정이 없으면 상자의 보상 설정을 쓰므로, 아직 설정을 읽는 상자가 있으면 그 준비 알림에서 다시 고른다.
	UnbindChestConfigurationEvents();
	if (GameMode->GetChestSpawnRewardDefinition().IsNull())
	{
		for (ARewardChest* RewardChest : RewardChests)
		{
			if (!RewardChest->GetRewardDefinitionAsset().IsNull() && !RewardChest->IsRewardContentReady())
			{
				PendingChestContentHandles.Emplace(RewardChest,
					RewardChest->OnRewardContentReady().AddUObject(this, &ThisClass::ConfigureRewardChestSpawns));
			}
		}
		if (!PendingChestContentHandles.IsEmpty())
		{
			return;
		}
	}

	const URewardDefinition* RewardDefinition = ResolveRewardDefinitionForChestSpawns(RewardChests);
	if (RewardChests.IsEmpty() || !RewardDefinition)
	{
		return;
	}

	const int32 ActiveChestCount = RewardDefinition->ResolveActiveRewardChestCount(RewardChests.Num());
	if (ActiveChestCount >= RewardChests.Num())
	{
		return;
	}

	TArray<int32> ChestIndices;
	for (int32 Index = 0; Index < RewardChests.Num(); ++Index)
	{
		ChestIndices.Add(Index);
	}
	for (int32 Index = 0; Index < ActiveChestCount; ++Index)
	{
		ChestIndices.Swap(Index, FMath::RandRange(Index, ChestIndices.Num() - 1));
	}

	TSet<ARewardChest*> ActiveChests;
	for (int32 Index = 0; Index < ActiveChestCount; ++Index)
	{
		ActiveChests.Add(RewardChests[ChestIndices[Index]]);
	}

	for (ARewardChest* RewardChest : RewardChests)
	{
		if (!ActiveChests.Contains(RewardChest))
		{
			RewardChest->DeactivateForSpawnPool();
		}
	}
}

const URewardDefinition* UMatchRewardComponent::ResolveRewardDefinitionForChestSpawns(
	const TArray<ARewardChest*>& RewardChests) const
{
	if (const URewardDefinition* RewardDefinition = GetExperienceGameMode()->GetChestSpawnRewardDefinition().Get())
	{
		return RewardDefinition;
	}

	for (const ARewardChest* RewardChest : RewardChests)
	{
		if (const URewardDefinition* RewardDefinition = RewardChest->GetRewardDefinitionAsset().Get())
		{
			return RewardDefinition;
		}
	}
	return nullptr;
}

void UMatchRewardComponent::GrantVictoryGold(const APlayerState* WinnerPlayerState, const int32 WinnerTeamColorIndex,
	const int32 WinnerTeamMemberCount, const APlayerState* ExcludedPlayerState) const
{
	if (!WinnerPlayerState || WinnerPlayerState == ExcludedPlayerState)
	{
		return;
	}

	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	const AGameStateBase* GameState = GameMode ? GameMode->GetGameState<AGameStateBase>() : nullptr;
	int32 RewardedWinnerCount = 0;
	if (WinnerTeamColorIndex != INDEX_NONE && GameState)
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			const APdPlayerState* PdPlayerState = Cast<APdPlayerState>(PlayerState);
			if (!PdPlayerState || PlayerState == ExcludedPlayerState
				|| PdPlayerState->GetPlayerMatchComponent()->GetMatchTeamColorIndex() != WinnerTeamColorIndex)
			{
				continue;
			}

			if (AController* TeamWinnerController = FindControllerForPlayerState(GetWorld(), PdPlayerState))
			{
				GrantVictoryGoldToController(TeamWinnerController, WinnerTeamMemberCount);
				++RewardedWinnerCount;
			}
		}
	}

	// 팀이 없거나 팀원 컨트롤러를 찾지 못했으면 승자 한 명에게만 준다.
	if (RewardedWinnerCount == 0)
	{
		GrantVictoryGoldToController(FindControllerForPlayerState(GetWorld(), WinnerPlayerState), WinnerTeamMemberCount);
	}
}

// 골드는 각 플레이어의 로컬 프로필에 쌓인다. 호스트 자신은 바로 저장하고, 원격 참가자에게는 RPC로 지급을 맡긴다.
void UMatchRewardComponent::GrantVictoryGoldToController(AController* WinnerController, const int32 WinnerTeamMemberCount) const
{
	const AExperienceGameMode* GameMode = GetExperienceGameMode();
	UPlayerProfileSubsystem* ProfileSubsystem = GameMode
		? UGameInstance::GetSubsystem<UPlayerProfileSubsystem>(GameMode->GetGameInstance())
		: nullptr;
	if (!WinnerController || !ProfileSubsystem || !GameMode->HasAuthority())
	{
		return;
	}

	const APdPlayerState* WinnerPlayerState = Cast<APdPlayerState>(WinnerController->PlayerState);
	const UPlayerMatchComponent* MatchComponent = WinnerPlayerState ? WinnerPlayerState->GetPlayerMatchComponent() : nullptr;
	const int32 GoldReward = MatchComponent
		? MatchOutcomeRules::CalculateVictoryGold(
			MatchComponent->GetKillCount(),
			MatchComponent->GetDeathCount(),
			WinnerTeamMemberCount,
			GameMode->GetVictoryGoldRates())
		: 0;
	if (GoldReward <= 0)
	{
		return;
	}

	const APlayerController* WinnerPlayerController = Cast<APlayerController>(WinnerController);
	if (WinnerPlayerController && WinnerPlayerController->IsLocalController())
	{
		ProfileSubsystem->AddGold(GoldReward, false);
		ProfileSubsystem->SaveProfile();
	}
	else if (APdPlayerController* WinnerPdPlayerController = Cast<APdPlayerController>(WinnerController))
	{
		WinnerPdPlayerController->Client_AddGameVictoryGoldReward(GoldReward);
	}
}
