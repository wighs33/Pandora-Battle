#include "Mode/PlayerEliminationSubsystem.h"

#include "Common/KillLogTypes.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Player/PlayerRewardComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerEliminationSubsystem)

namespace
{
	// 피해를 준 액터(투사체·소환물 포함)에서 그 액터를 조종한 플레이어를 찾는다.
	APlayerState* ResolvePlayerStateFromActor(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (APlayerState* PlayerState = Cast<APlayerState>(Actor))
		{
			return PlayerState;
		}

		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn->GetPlayerState();
		}

		if (const AController* Controller = Cast<AController>(Actor))
		{
			return Controller->PlayerState;
		}

		if (const APawn* InstigatorPawn = Actor->GetInstigator())
		{
			if (APlayerState* PlayerState = InstigatorPawn->GetPlayerState())
			{
				return PlayerState;
			}
		}

		AActor* OwnerActor = Actor->GetOwner();
		return OwnerActor && OwnerActor != Actor ? ResolvePlayerStateFromActor(OwnerActor) : nullptr;
	}
}

bool UPlayerEliminationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UPlayerEliminationSubsystem::HandleEliminated(AActor* VictimActor, AActor* DamageInstigator, AActor* DamageCauser)
{
	if (!VictimActor || !VictimActor->HasAuthority())
	{
		return;
	}

	APlayerState* VictimPlayerState = ResolvePlayerStateFromActor(VictimActor);
	if (!VictimPlayerState)
	{
		return;
	}

	APlayerState* KillerPlayerState = ResolvePlayerStateFromActor(DamageInstigator);
	if (!KillerPlayerState)
	{
		KillerPlayerState = ResolvePlayerStateFromActor(DamageCauser);
	}

	BroadcastKillLog(VictimPlayerState, KillerPlayerState);

	if (const APdPlayerState* VictimPdPlayerState = Cast<APdPlayerState>(VictimPlayerState))
	{
		VictimPdPlayerState->GetPlayerMatchComponent()->RecordDeath();
	}

	// 자기 자신이나 환경에 의한 사망은 누구의 점수도 아니다.
	if (!KillerPlayerState || KillerPlayerState == VictimPlayerState)
	{
		return;
	}

	// 킬당 1점이다. 점수판과 승패 판정은 PlayerState 점수를 처치 수로 읽는다.
	KillerPlayerState->SetScore(KillerPlayerState->GetScore() + 1.0f);

	const APdPlayerState* KillerPdPlayerState = Cast<APdPlayerState>(KillerPlayerState);
	if (UPlayerRewardComponent* RewardComponent = KillerPdPlayerState ? KillerPdPlayerState->GetPlayerRewardComponent() : nullptr)
	{
		RewardComponent->GrantKillExperience(VictimPlayerState);
	}

	OnPlayerKillScored.Broadcast(KillerPlayerState, VictimPlayerState);
}

void UPlayerEliminationSubsystem::BroadcastKillLog(const APlayerState* VictimPlayerState, const APlayerState* KillerPlayerState) const
{
	FKillLogEntry KillLogEntry;
	KillLogEntry.VictimName = UPlayerMatchComponent::ResolveDisplayName(VictimPlayerState);
	KillLogEntry.bEnvironmentKill = !KillerPlayerState;
	KillLogEntry.bSelfKill = KillerPlayerState && KillerPlayerState == VictimPlayerState;
	KillLogEntry.KillerName = KillerPlayerState
		? UPlayerMatchComponent::ResolveDisplayName(KillerPlayerState)
		: NSLOCTEXT("KillLog", "EnvironmentKillerName", "Environment");

	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (APdPlayerController* PlayerController = Cast<APdPlayerController>(Iterator->Get()))
		{
			PlayerController->Client_AddKillLogEntry(KillLogEntry);
		}
	}
}
