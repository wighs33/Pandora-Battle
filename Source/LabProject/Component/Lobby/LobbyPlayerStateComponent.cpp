#include "Component/Lobby/LobbyPlayerStateComponent.h"

#include "Component/Player/PlayerMatchComponent.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPlayerStateComponent)

// 로비 전용 상태만 복제하고 프레임마다 실행할 작업은 만들지 않는다.
ULobbyPlayerStateComponent::ULobbyPlayerStateComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

// 공통 경기 정보에서 직접 바뀐 이름과 팀도 로비 구독자에게 전달한다.
void ULobbyPlayerStateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UPlayerMatchComponent* MatchComponent = GetPlayerMatchComponent())
	{
		MatchComponent->OnMatchDisplayNameChanged.AddUObject(this, &ThisClass::HandleMatchDisplayNameChanged);
		MatchComponent->OnMatchTeamColorChanged.AddUObject(this, &ThisClass::HandleMatchTeamColorChanged);
	}
}

// 로비를 떠날 때 공통 경기 정보의 변경 구독을 해제한다.
void ULobbyPlayerStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPlayerMatchComponent* MatchComponent = GetPlayerMatchComponent())
	{
		MatchComponent->OnMatchDisplayNameChanged.RemoveAll(this);
		MatchComponent->OnMatchTeamColorChanged.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

// 이름과 팀은 MatchComponent에 맡기고 로비 전용 값만 전송한다.
void ULobbyPlayerStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ULobbyPlayerStateComponent, bLeavingLobby, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ULobbyPlayerStateComponent, NicknameHint, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(ULobbyPlayerStateComponent, bUsingNicknameHint, Params);
}

// 서버에서 퇴장 처리가 시작된 플레이어를 표시한다.
void ULobbyPlayerStateComponent::SetLeavingLobby(const bool bInLeavingLobby)
{
	if (!HasAuthority() || bLeavingLobby == bInLeavingLobby)
	{
		return;
	}
	bLeavingLobby = bInLeavingLobby;
	MARK_PROPERTY_DIRTY_FROM_NAME(ULobbyPlayerStateComponent, bLeavingLobby, this);
	GetOwner()->ForceNetUpdate();
	NotifyLobbyRuntimeStateChanged();
}

// 직접 입력한 닉네임을 적용하되 기존 기본 이름 힌트는 유지한다.
void ULobbyPlayerStateComponent::SetNickname(const FText& InNickname)
{
	const bool bUseHint = !NicknameHint.IsEmpty() && InNickname.EqualTo(NicknameHint);
	SetNicknameInternal(InNickname, NicknameHint.IsEmpty() ? InNickname : NicknameHint, bUseHint);
}

// 기본 닉네임을 실제 이름과 입력 힌트에 함께 반영한다.
void ULobbyPlayerStateComponent::SetDefaultNickname(const FText& InNickname)
{
	SetNicknameInternal(InNickname, InNickname, true);
}

// 입력을 지운 플레이어의 닉네임을 기본 이름으로 복원한다.
void ULobbyPlayerStateComponent::ClearCustomNickname()
{
	if (!NicknameHint.IsEmpty())
	{
		SetNicknameInternal(NicknameHint, NicknameHint, true);
	}
}

// 로비 전용 복사본 없이 공통 경기 정보의 표시 이름을 읽는다.
FText ULobbyPlayerStateComponent::GetNickname() const
{
	const UPlayerMatchComponent* MatchComponent = GetPlayerMatchComponent();
	return MatchComponent ? MatchComponent->GetMatchDisplayName() : FText::GetEmpty();
}

// 서버가 확정한 팀 선택을 공통 경기 정보에 전달한다.
void ULobbyPlayerStateComponent::SetTeamColorIndex(const int32 InTeamColorIndex)
{
	if (HasAuthority())
	{
		if (UPlayerMatchComponent* MatchComponent = GetPlayerMatchComponent())
		{
			MatchComponent->SetMatchTeamColorIndex(InTeamColorIndex);
		}
	}
}

// 로비 팀 표시와 인게임 팀 판정이 같은 값을 사용하게 한다.
int32 ULobbyPlayerStateComponent::GetTeamColorIndex() const
{
	const UPlayerMatchComponent* MatchComponent = GetPlayerMatchComponent();
	return MatchComponent ? MatchComponent->GetMatchTeamColorIndex() : INDEX_NONE;
}

// 상태 변경을 서버로 제한한다.
bool ULobbyPlayerStateComponent::HasAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

// 같은 PlayerState가 소유한 식별 정보 컴포넌트를 조회한다.
UPlayerMatchComponent* ULobbyPlayerStateComponent::GetPlayerMatchComponent() const
{
	const APdPlayerState* PlayerState = GetPlayerState<APdPlayerState>();
	return PlayerState ? PlayerState->GetPlayerMatchComponent() : nullptr;
}

// 공통 표시 이름과 로비 입력 상태를 함께 갱신하고 변경된 로비 값만 복제 대상으로 표시한다.
void ULobbyPlayerStateComponent::SetNicknameInternal(
	const FText& InNickname, const FText& InNicknameHint, const bool bInUsingNicknameHint)
{
	UPlayerMatchComponent* MatchComponent = GetPlayerMatchComponent();
	if (!HasAuthority() || !MatchComponent)
	{
		return;
	}

	const bool bNicknameChanged = !MatchComponent->GetMatchDisplayName().EqualTo(InNickname);
	const bool bHintChanged = !NicknameHint.EqualTo(InNicknameHint);
	const bool bUsingHintChanged = bUsingNicknameHint != bInUsingNicknameHint;
	if (!bNicknameChanged && !bHintChanged && !bUsingHintChanged)
	{
		return;
	}

	NicknameHint = InNicknameHint;
	bUsingNicknameHint = bInUsingNicknameHint;
	if (bHintChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ULobbyPlayerStateComponent, NicknameHint, this);
	}
	if (bUsingHintChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(ULobbyPlayerStateComponent, bUsingNicknameHint, this);
	}
	MatchComponent->SetMatchDisplayName(InNickname);
	GetOwner()->ForceNetUpdate();
	// BeginPlay 전의 기본 닉네임 설정과 힌트만 바뀐 경우에도 알림을 보장한다.
	if (!bNicknameChanged || !HasBegunPlay())
	{
		NotifyLobbyRuntimeStateChanged();
	}
}

// 공통 이름이 직접 변경되거나 복제되어도 로비 목록을 갱신하게 한다.
void ULobbyPlayerStateComponent::HandleMatchDisplayNameChanged(const FText& NewDisplayName)
{
	NotifyLobbyRuntimeStateChanged();
}

// 공통 팀이 변경되거나 복제되면 로비 팀 표시를 갱신하게 한다.
void ULobbyPlayerStateComponent::HandleMatchTeamColorChanged(const int32 NewTeamColorIndex)
{
	NotifyLobbyRuntimeStateChanged();
}

// 구체적인 HUD를 찾지 않고 상태 변경만 구독자에게 알린다.
void ULobbyPlayerStateComponent::NotifyLobbyRuntimeStateChanged()
{
	OnLobbyRuntimeStateChanged.Broadcast();
}

// 클라이언트에 로비 전용 상태가 도착한 뒤 구독자에게 알린다.
void ULobbyPlayerStateComponent::OnRep_LobbyState()
{
	NotifyLobbyRuntimeStateChanged();
}
