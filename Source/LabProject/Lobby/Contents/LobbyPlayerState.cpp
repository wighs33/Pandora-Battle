#include "Lobby/Contents/LobbyPlayerState.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "Component/Player/StatUpgradeComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPlayerState)

// 로비 전용 상태와 기본 속성을 구성한다. 기본 서브오브젝트인 AttributeSet의 ASC 등록은 엔진 초기화가 담당한다.
ALobbyPlayerState::ALobbyPlayerState(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetNetUpdateFrequency(30.0f);
	LobbyPlayerStateComponent = CreateDefaultSubobject<ULobbyPlayerStateComponent>(TEXT("LobbyPlayerStateComponent"));
	LobbyBasicAttributeSet = CreateDefaultSubobject<UBasicAttributeSet>(TEXT("LobbyBasicAttributeSet"));
}

// ASC와 속성의 엔진 초기화가 끝난 뒤 서버에서 로비 프리뷰의 기본 능력치를 적용한다.
void ALobbyPlayerState::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetStatUpgradeComponent()->ApplyConfiguredAttributeDefaults();
	}
}

// 퇴장 중인 플레이어를 로비 목록과 시작 인원 계산에서 제외한다.
void ALobbyPlayerState::SetLeavingLobby(const bool bInLeavingLobby)
{
	LobbyPlayerStateComponent->SetLeavingLobby(bInLeavingLobby);
}

// 로비 화면과 시작 조건이 퇴장 중인 플레이어를 구분하게 한다.
bool ALobbyPlayerState::IsLeavingLobby() const
{
	return LobbyPlayerStateComponent->IsLeavingLobby();
}

// 플레이어가 확정한 닉네임을 공통 경기 식별 정보에 반영한다.
void ALobbyPlayerState::SetNickname(const FText& InNickname)
{
	LobbyPlayerStateComponent->SetNickname(InNickname);
}

// 첫 입장 또는 로비 복귀 시 기본 닉네임과 입력 힌트를 준비한다.
void ALobbyPlayerState::SetDefaultNickname(const FText& InNickname)
{
	LobbyPlayerStateComponent->SetDefaultNickname(InNickname);
}

// 직접 입력한 닉네임을 기본 닉네임으로 되돌린다.
void ALobbyPlayerState::ClearCustomNickname()
{
	LobbyPlayerStateComponent->ClearCustomNickname();
}

// 로비와 인게임이 같은 표시 이름을 사용하게 한다.
FText ALobbyPlayerState::GetNickname() const
{
	return LobbyPlayerStateComponent->GetNickname();
}

// 닉네임 입력칸에 표시할 기본 이름을 제공한다.
FText ALobbyPlayerState::GetNicknameHint() const
{
	return LobbyPlayerStateComponent->GetNicknameHint();
}

// 입력칸이 직접 입력된 이름과 기본 힌트 표시를 구분하게 한다.
bool ALobbyPlayerState::IsUsingNicknameHint() const
{
	return LobbyPlayerStateComponent->IsUsingNicknameHint();
}

// 로비에서 선택한 팀을 인게임에서도 사용하는 경기 정보에 반영한다.
void ALobbyPlayerState::SetTeamColorIndex(const int32 InTeamColorIndex)
{
	LobbyPlayerStateComponent->SetTeamColorIndex(InTeamColorIndex);
}

// 로비 팀 표시와 인원 균형 계산에 사용할 팀을 제공한다.
int32 ALobbyPlayerState::GetTeamColorIndex() const
{
	return LobbyPlayerStateComponent->GetTeamColorIndex();
}

// 부모의 식별 정보 인계를 유지하고, 돌아온 로비의 닉네임 입력 힌트만 새로 준비한다.
void ALobbyPlayerState::ReceiveMatchIdentityFromCopyProperties(const FPlayerMatchIdentity& Identity)
{
	Super::ReceiveMatchIdentityFromCopyProperties(Identity);
	LobbyPlayerStateComponent->SetDefaultNickname(Identity.DisplayName);
}
