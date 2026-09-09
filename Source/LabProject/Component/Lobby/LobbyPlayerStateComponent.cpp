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
	OnLobbyRuntimeStateChanged.Broadcast();
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

// 힌트 사용 여부는 로비 상태와 원본 표시 이름을 함께 확인한다.
bool ULobbyPlayerStateComponent::IsUsingNicknameHint() const
{
	if (!bUsingNicknameHint)
	{
		return false;
	}
	const UPlayerMatchComponent* MatchComponent = GetPlayerMatchComponent();
	const FText DisplayName = MatchComponent ? MatchComponent->GetMatchDisplayName() : FText::GetEmpty();
	return DisplayName.EqualTo(NicknameHint);
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
	// 이름 변경은 MatchComponent가 알리고, 여기서는 로비 전용 입력 상태만 알린다.
	if (bHintChanged || bUsingHintChanged)
	{
		OnLobbyRuntimeStateChanged.Broadcast();
	}
}

// 클라이언트에 로비 전용 상태가 도착한 뒤 구독자에게 알린다.
void ULobbyPlayerStateComponent::OnRep_LobbyState()
{
	OnLobbyRuntimeStateChanged.Broadcast();
}
