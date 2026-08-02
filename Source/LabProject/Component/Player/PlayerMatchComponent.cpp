#include "Component/Player/PlayerMatchComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Mode/PdGameInstance.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerMatchComponent)

UPlayerMatchComponent::UPlayerMatchComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPlayerMatchComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeDefaultMatchDisplayNameIfNeeded();
}

void UPlayerMatchComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UPlayerMatchComponent, PlayerMatchIdentity, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPlayerMatchComponent, DeathCount, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPlayerMatchComponent, PlayerMapRegion, Params);
}

void UPlayerMatchComponent::SetPlayerMatchIdentity(const FPlayerMatchIdentity& InMatchIdentity)
{
	if (!HasAuthority() || PlayerMatchIdentity.Matches(InMatchIdentity))
	{
		return;
	}

	const FPlayerMatchIdentity PreviousIdentity = PlayerMatchIdentity;
	PlayerMatchIdentity = InMatchIdentity;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPlayerMatchComponent, PlayerMatchIdentity, this);
	GetOwner()->ForceNetUpdate();

	BroadcastPlayerMatchIdentityChanged(&PreviousIdentity);
}

void UPlayerMatchComponent::SetMatchDisplayName(const FText& InDisplayName)
{
	FPlayerMatchIdentity NewMatchIdentity = PlayerMatchIdentity;
	NewMatchIdentity.DisplayName = InDisplayName;
	SetPlayerMatchIdentity(NewMatchIdentity);
}

void UPlayerMatchComponent::SetMatchSpawnIndex(const int32 InSpawnIndex)
{
	FPlayerMatchIdentity NewMatchIdentity = PlayerMatchIdentity;
	NewMatchIdentity.SpawnIndex = InSpawnIndex;
	SetPlayerMatchIdentity(NewMatchIdentity);
}

void UPlayerMatchComponent::SetMatchTeamColorIndex(const int32 InTeamColorIndex)
{
	FPlayerMatchIdentity NewMatchIdentity = PlayerMatchIdentity;
	NewMatchIdentity.TeamColorIndex = InTeamColorIndex;
	SetPlayerMatchIdentity(NewMatchIdentity);
}

int32 UPlayerMatchComponent::GetKillCount() const
{
	const APlayerState* OwnerPlayerState = Cast<APlayerState>(GetOwner());
	return OwnerPlayerState
		? FMath::Max(FMath::RoundToInt(OwnerPlayerState->GetScore()), 0)
		: 0;
}

bool UPlayerMatchComponent::RecordDeath(const int32 Amount)
{
	if (!HasAuthority() || Amount <= 0)
	{
		return false;
	}

	const int64 NewDeathCount = static_cast<int64>(DeathCount) + Amount;
	SetDeathCount(static_cast<int32>(FMath::Min<int64>(NewDeathCount, MAX_int32)));
	return true;
}

void UPlayerMatchComponent::SetInitialSpawnTransform(const FTransform& InSpawnTransform)
{
	if (!HasAuthority())
	{
		return;
	}

	bHasInitialSpawnTransform = true;
	InitialSpawnTransform = InSpawnTransform;
}

void UPlayerMatchComponent::ClearInitialSpawnTransform()
{
	if (!HasAuthority())
	{
		return;
	}

	bHasInitialSpawnTransform = false;
	InitialSpawnTransform = FTransform::Identity;
}

bool UPlayerMatchComponent::TryGetInitialSpawnTransform(FTransform& OutSpawnTransform) const
{
	if (!bHasInitialSpawnTransform)
	{
		return false;
	}

	OutSpawnTransform = InitialSpawnTransform;
	return true;
}

void UPlayerMatchComponent::SetPlayerMapRegion(const EPdPlayerMapRegion InMapRegion)
{
	if (!HasAuthority() || PlayerMapRegion == InMapRegion)
	{
		return;
	}

	PlayerMapRegion = InMapRegion;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPlayerMatchComponent, PlayerMapRegion, this);
	GetOwner()->ForceNetUpdate();

	OnPlayerMapRegionChanged.Broadcast(PlayerMapRegion);
}

void UPlayerMatchComponent::CopyMatchStateTo(
	UPlayerMatchComponent* TargetComponent,
	const FPlayerMatchIdentity& MatchIdentityToCopy,
	const bool bCopyMatchStats) const
{
	if (!HasAuthority() || !IsValid(TargetComponent) || !TargetComponent->HasAuthority())
	{
		return;
	}

	const APlayerState* SourcePlayerState = Cast<APlayerState>(GetOwner());
	APlayerState* TargetPlayerState = Cast<APlayerState>(TargetComponent->GetOwner());
	if (!SourcePlayerState || !TargetPlayerState)
	{
		return;
	}

	TargetComponent->SetPlayerMatchIdentity(MatchIdentityToCopy);
	TargetComponent->SetPlayerMapRegion(PlayerMapRegion);
	TargetPlayerState->SetScore(bCopyMatchStats ? SourcePlayerState->GetScore() : 0.0f);
	TargetComponent->SetDeathCount(bCopyMatchStats ? DeathCount : 0);
}

bool UPlayerMatchComponent::HasAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UPlayerMatchComponent::InitializeDefaultMatchDisplayNameIfNeeded()
{
	APlayerState* OwnerPlayerState = Cast<APlayerState>(GetOwner());
	if (!OwnerPlayerState || !OwnerPlayerState->HasAuthority() || !GetMatchDisplayName().IsEmpty())
	{
		return;
	}

	UPdGameInstance* PdGameInstance = GetWorld()
		? GetWorld()->GetGameInstance<UPdGameInstance>()
		: nullptr;
	if (!PdGameInstance)
	{
		return;
	}

	int32 FallbackDisplayNameIndex = 1;
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* CurrentGameState = World->GetGameState())
		{
			const int32 PlayerIndex = CurrentGameState->PlayerArray.IndexOfByKey(OwnerPlayerState);
			FallbackDisplayNameIndex = PlayerIndex != INDEX_NONE
				? PlayerIndex + 1
				: CurrentGameState->PlayerArray.Num() + 1;
		}
	}

	SetMatchDisplayName(PdGameInstance->ResolveDefaultPlayerNickname(
		Cast<APlayerController>(OwnerPlayerState->GetOwner()),
		OwnerPlayerState,
		FallbackDisplayNameIndex));
}

void UPlayerMatchComponent::SetDeathCount(const int32 InDeathCount)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 SanitizedDeathCount = FMath::Max(InDeathCount, 0);
	if (DeathCount == SanitizedDeathCount)
	{
		return;
	}

	DeathCount = SanitizedDeathCount;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPlayerMatchComponent, DeathCount, this);
	GetOwner()->ForceNetUpdate();
	OnPlayerDeathCountChanged.Broadcast(DeathCount);
}

void UPlayerMatchComponent::BroadcastPlayerMatchIdentityChanged(
	const FPlayerMatchIdentity* PreviousIdentity)
{
	OnPlayerMatchIdentityChanged.Broadcast(PlayerMatchIdentity);

	if (!PreviousIdentity || !PreviousIdentity->DisplayName.EqualTo(PlayerMatchIdentity.DisplayName))
	{
		OnMatchDisplayNameChanged.Broadcast(PlayerMatchIdentity.DisplayName);
	}
	if (!PreviousIdentity || PreviousIdentity->TeamColorIndex != PlayerMatchIdentity.TeamColorIndex)
	{
		OnMatchTeamColorChanged.Broadcast(PlayerMatchIdentity.TeamColorIndex);
	}
}

void UPlayerMatchComponent::OnRep_PlayerMatchIdentity(const FPlayerMatchIdentity& PreviousIdentity)
{
	BroadcastPlayerMatchIdentityChanged(&PreviousIdentity);
}

void UPlayerMatchComponent::OnRep_DeathCount(const int32 PreviousDeathCount)
{
	if (DeathCount != PreviousDeathCount)
	{
		OnPlayerDeathCountChanged.Broadcast(DeathCount);
	}
}

void UPlayerMatchComponent::OnRep_PlayerMapRegion(const EPdPlayerMapRegion PreviousPlayerMapRegion)
{
	if (PlayerMapRegion != PreviousPlayerMapRegion)
	{
		OnPlayerMapRegionChanged.Broadcast(PlayerMapRegion);
	}
}
