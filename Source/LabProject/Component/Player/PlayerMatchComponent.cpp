#include "Component/Player/PlayerMatchComponent.h"

#include "GameFramework/PlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerMatchComponent)

// 경기 중 플레이어 정보를 네트워크로 공유하도록 설정하고, 매 프레임 갱신은 사용하지 않는다.
UPlayerMatchComponent::UPlayerMatchComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// 이름·팀·스폰 번호·선택 업적과 사망 횟수·현재 맵 구역을 클라이언트에 복제할 대상으로 등록한다.
void UPlayerMatchComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UPlayerMatchComponent, PlayerMatchIdentity, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPlayerMatchComponent, DeathCount, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPlayerMatchComponent, PlayerMapRegion, Params);
}

// 서버에서 확정한 플레이어 식별 정보를 반영하고, 이름과 팀 표시를 갱신하도록 변경을 알린다.
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

	BroadcastPlayerMatchIdentityChanged(PreviousIdentity);
}

// 스코어보드와 킬 로그 등에서 사용할 경기 표시 이름을 서버에서 갱신한다.
void UPlayerMatchComponent::SetMatchDisplayName(const FText& InDisplayName)
{
	FPlayerMatchIdentity NewMatchIdentity = PlayerMatchIdentity;
	NewMatchIdentity.DisplayName = InDisplayName;
	SetPlayerMatchIdentity(NewMatchIdentity);
}

// 로비에서 배정한 스폰 번호를 저장해 경기 입장 시 시작 위치를 고르는 데 사용한다.
void UPlayerMatchComponent::SetMatchSpawnIndex(const int32 InSpawnIndex)
{
	FPlayerMatchIdentity NewMatchIdentity = PlayerMatchIdentity;
	NewMatchIdentity.SpawnIndex = InSpawnIndex;
	SetPlayerMatchIdentity(NewMatchIdentity);
}

// 캐릭터와 HUD에서 플레이어를 구분할 팀 색상 인덱스를 서버에서 갱신한다.
void UPlayerMatchComponent::SetMatchTeamColorIndex(const int32 InTeamColorIndex)
{
	FPlayerMatchIdentity NewMatchIdentity = PlayerMatchIdentity;
	NewMatchIdentity.TeamColorIndex = InTeamColorIndex;
	SetPlayerMatchIdentity(NewMatchIdentity);
}

// 캐릭터의 업적 아이콘 표시에 사용할 선택 업적 ID를 서버에서 갱신한다.
void UPlayerMatchComponent::SetSelectedAchievementId(const FName InAchievementId)
{
	FPlayerMatchIdentity NewMatchIdentity = PlayerMatchIdentity;
	NewMatchIdentity.SelectedAchievementId = InAchievementId;
	SetPlayerMatchIdentity(NewMatchIdentity);
}

// 킬당 1점인 현재 경기 규칙에 따라 PlayerState의 Score를 스코어보드와 경기 결과에 사용할 킬 수로 반환한다.
int32 UPlayerMatchComponent::GetKillCount() const
{
	const APlayerState* OwnerPlayerState = GetPlayerState<APlayerState>();
	return OwnerPlayerState
		? FMath::Max(FMath::RoundToInt(OwnerPlayerState->GetScore()), 0)
		: 0;
}

// 플레이어 사망 처리에서 호출해 서버에서 현재 경기의 사망 횟수를 누적한다.
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

// 구역 진입 시 현재 맵 구역을 서버에서 갱신해 지도 UI가 플레이어를 알맞은 구역에 표시하도록 한다.
void UPlayerMatchComponent::SetPlayerMapRegion(const EPlayerMapRegion InMapRegion)
{
	if (!HasAuthority() || PlayerMapRegion == InMapRegion)
	{
		return;
	}

	PlayerMapRegion = InMapRegion;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPlayerMatchComponent, PlayerMapRegion, this);
	GetOwner()->ForceNetUpdate();
}

// 새 경기 입장 시 식별 정보는 유지하고 킬·사망 기록과 맵 구역을 초기화한다. 리스폰 때는 호출하지 않는다.
void UPlayerMatchComponent::ResetForNewMatch(const EPlayerMapRegion InitialMapRegion)
{
	APlayerState* PlayerState = GetPlayerState<APlayerState>();
	if (!PlayerState || !HasAuthority())
	{
		return;
	}

	PlayerState->SetScore(0.0f);
	SetDeathCount(0);
	SetPlayerMapRegion(InitialMapRegion);
}

// 사망 집계나 새 경기 초기화에서 정한 사망 횟수를 저장하고 클라이언트에 복제되도록 갱신을 요청한다.
void UPlayerMatchComponent::SetDeathCount(const int32 InDeathCount)
{
	if (DeathCount == InDeathCount)
	{
		return;
	}

	DeathCount = InDeathCount;
	MARK_PROPERTY_DIRTY_FROM_NAME(UPlayerMatchComponent, DeathCount, this);
	GetOwner()->ForceNetUpdate();
}

// 개별 표시를 갱신한 뒤, 식별 정보 변경 한 건을 로비 등의 구독자에게 알린다.
void UPlayerMatchComponent::BroadcastPlayerMatchIdentityChanged(const FPlayerMatchIdentity& PreviousIdentity)
{
	if (!PreviousIdentity.DisplayName.EqualTo(PlayerMatchIdentity.DisplayName))
	{
		OnMatchDisplayNameChanged.Broadcast(PlayerMatchIdentity.DisplayName);
	}
	if (PreviousIdentity.TeamColorIndex != PlayerMatchIdentity.TeamColorIndex)
	{
		OnMatchTeamColorChanged.Broadcast(PlayerMatchIdentity.TeamColorIndex);
	}
	OnMatchIdentityChanged.Broadcast();
}

// 클라이언트가 서버의 식별 정보를 전달받으면 이름과 팀의 변경을 알려 화면 표시를 갱신하게 한다.
void UPlayerMatchComponent::OnRep_PlayerMatchIdentity(const FPlayerMatchIdentity& PreviousIdentity)
{
	BroadcastPlayerMatchIdentityChanged(PreviousIdentity);
}
