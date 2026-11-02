#include "Cinematic/CinematicSequenceActor.h"

#include "Component/Player/ControllerPresentationComponent.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "Engine/World.h"
#include "LevelSequencePlayer.h"
#include "Mode/PdPlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CinematicSequenceActor)

ACinematicSequenceActor::ACinematicSequenceActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicatePlayback = true;
	bAlwaysRelevant = true;
	bOverrideInstanceData = true;
}

// 원점을 정하는 인스턴스 데이터는 복제되지 않는다. 서버와 클라이언트가 각자 복제된 이 액터의 위치를 원점으로 쓴다.
void ACinematicSequenceActor::PostInitializeComponents()
{
	if (UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(DefaultInstanceData))
	{
		InstanceData->TransformOriginActor = this;
	}
	Super::PostInitializeComponents();
}

// 클라이언트는 복제된 재생 상태를 따라 재생하고 멈춘다. 그 변화를 이 화면의 로컬 플레이어에게 알린다.
void ACinematicSequenceActor::BeginPlay()
{
	Super::BeginPlay();
	ULevelSequencePlayer* Player = GetSequencePlayer();
	if (GetNetMode() == NM_DedicatedServer || !Player)
	{
		return;
	}
	Player->OnPlay.AddDynamic(this, &ThisClass::HandlePlaybackChanged);
	Player->OnStop.AddDynamic(this, &ThisClass::HandlePlaybackChanged);
	HandlePlaybackChanged();
}

void ACinematicSequenceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetLocalPlayersInCinematic(false);
	Super::EndPlay(EndPlayReason);
}

// 끝까지 재생해도 멈춤으로 끝나므로 재생과 멈춤만 보면 된다.
void ACinematicSequenceActor::HandlePlaybackChanged()
{
	const ULevelSequencePlayer* Player = GetSequencePlayer();
	SetLocalPlayersInCinematic(Player && Player->IsPlaying());
}

void ACinematicSequenceActor::SetLocalPlayersInCinematic(const bool bInCinematic) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APdPlayerController* Controller = Cast<APdPlayerController>(It->Get());
		UControllerPresentationComponent* Presentation =
			Controller && Controller->IsLocalController() ? Controller->GetControllerPresentationComponent() : nullptr;
		if (Presentation)
		{
			Presentation->SetInCinematic(bInCinematic);
		}
	}
}
