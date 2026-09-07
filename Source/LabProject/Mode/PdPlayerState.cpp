#include "Mode/PdPlayerState.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Player/PlayerLoadoutComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/LevelingComponent.h"
#include "Component/Player/PlayerRewardComponent.h"
#include "Component/Player/StatUpgradeComponent.h"
#include "Component/Skin/SkinComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayerState)

// 플레이어 상태를 관리할 기본 컴포넌트를 구성하고 네트워크 상태 갱신 빈도를 설정한다.
APdPlayerState::APdPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetUpdateFrequency(100.0f);

	AbilitySystemComponent = CreateDefaultSubobject<UPdAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	PlayerLoadoutComponent = CreateDefaultSubobject<UPlayerLoadoutComponent>(TEXT("PlayerLoadoutComponent"));
	PlayerMatchComponent = CreateDefaultSubobject<UPlayerMatchComponent>(TEXT("PlayerMatchComponent"));
	LevelingComponent = CreateDefaultSubobject<ULevelingComponent>(TEXT("LevelingComponent"));
	SkinComponent = CreateDefaultSubobject<USkinComponent>(TEXT("SkinComponent"));
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
	PandoraComponent = CreateDefaultSubobject<UPandoraComponent>(TEXT("PandoraComponent"));
	PlayerRewardComponent = CreateDefaultSubobject<UPlayerRewardComponent>(TEXT("PlayerRewardComponent"));
	StatUpgradeComponent = CreateDefaultSubobject<UStatUpgradeComponent>(TEXT("StatUpgradeComponent"));
	PandoraTreeComponent = CreateDefaultSubobject<UPandoraTreeComponent>(TEXT("PandoraTreeComponent"));
}

// 컴포넌트는 직접 소유하되, 게임피처의 능력·속성 부여와 확장 초기화를 위해 GFCM 등록을 유지한다.
void APdPlayerState::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

// 플레이어 상태가 준비되었음을 게임피처에 알려 관련 기능의 초기화를 이어갈 수 있게 한다.
void APdPlayerState::BeginPlay()
{
	Super::BeginPlay();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

// 플레이어 상태가 종료될 때 GFCM 수신 등록을 해제하여 게임피처와의 연결을 정리한다.
void APdPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

// PlayerState 교체 시 엔진 기본 정보와 프로젝트의 플레이어 식별 정보를 인계한다.
// 프로젝트의 사망 횟수·맵 구역·로드아웃 선택은 인계하지 않는다.
void APdPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);

	APdPlayerState* TargetPlayerState = Cast<APdPlayerState>(NewPlayerState);
	if (!TargetPlayerState)
	{
		return;
	}

	if (PlayerMatchComponent)
	{
		TargetPlayerState->ReceiveMatchIdentityFromCopyProperties(BuildMatchIdentityForCopyProperties());
	}
}

// GAS와 다른 게임 로직이 플레이어의 능력·속성을 관리하는 ASC에 접근할 수 있게 한다.
UAbilitySystemComponent* APdPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

// 상호작용 보상과 플레이어 처치 보상을 처리하는 컴포넌트를 조회한다.
UPlayerRewardComponent* APdPlayerState::GetPlayerRewardComponent() const
{
	return PlayerRewardComponent.Get();
}

// 플레이어의 능력치 투자와 회수를 처리하는 컴포넌트를 조회한다.
UStatUpgradeComponent* APdPlayerState::GetStatUpgradeComponent() const
{
	return StatUpgradeComponent.Get();
}

// 소지 아이템과 무기 로드아웃을 관리하는 인벤토리 컴포넌트를 조회한다.
UInventoryComponent* APdPlayerState::GetInventoryComponent() const
{
	return InventoryComponent.Get();
}

// 보유 판도라와 로드아웃 슬롯, 현재 선택한 판도라를 관리하는 컴포넌트를 조회한다.
UPandoraComponent* APdPlayerState::GetPandoraComponent() const
{
	return PandoraComponent.Get();
}

// 판도라 획득과 레벨업을 관리하는 트리 컴포넌트를 조회한다.
UPandoraTreeComponent* APdPlayerState::GetPandoraTreeComponent() const
{
	return PandoraTreeComponent.Get();
}

// 다음 PlayerState에 넘길 표시 이름·팀·스폰 식별 정보·선택 업적을 반환한다.
FPlayerMatchIdentity APdPlayerState::BuildMatchIdentityForCopyProperties() const
{
	return PlayerMatchComponent
		? PlayerMatchComponent->GetPlayerMatchIdentity()
		: FPlayerMatchIdentity();
}

// 이전 PlayerState에서 전달받은 플레이어 식별 정보를 현재 경기 정보 컴포넌트에 적용한다.
void APdPlayerState::ReceiveMatchIdentityFromCopyProperties(const FPlayerMatchIdentity& Identity)
{
	if (PlayerMatchComponent)
	{
		PlayerMatchComponent->SetPlayerMatchIdentity(Identity);
	}
}
