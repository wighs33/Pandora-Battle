#include "Lobby/Contents/TitleGameMode.h"

#include "Definition/Level/LevelDefinition.h"
#include "Engine/World.h"
#include "Lobby/Contents/TitleHUD.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TitleGameMode)

DEFINE_LOG_CATEGORY_STATIC(LogTitleGameMode, Log, All);

ATitleGameMode::ATitleGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HUDClass = ATitleHUD::StaticClass();
	DefaultPawnClass = nullptr;
}

// 타이틀은 로컬 플레이어의 메뉴 화면이다. 전용 서버가 기본 맵으로 타이틀을 열면 바로 로비로 이동해 접속을 받는다.
void ATitleGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::TravelDedicatedServerToLobby);
	}
}

void ATitleGameMode::TravelDedicatedServerToLobby()
{
	UWorld* World = GetWorld();
	const ULevelDefinition* LevelDefinition = ULevelDefinition::ResolveDefaultDefinition();
	const FString LobbyMapName = LevelDefinition ? LevelDefinition->GetLobbyTravelMapName() : FString();
	if (!World || LobbyMapName.IsEmpty())
	{
		UE_LOG(LogTitleGameMode, Error, TEXT("Dedicated server could not resolve the lobby map from the level definition."));
		return;
	}

	UE_LOG(LogTitleGameMode, Log, TEXT("Dedicated server started on the title map. Traveling to %s."), *LobbyMapName);
	World->ServerTravel(LobbyMapName);
}
