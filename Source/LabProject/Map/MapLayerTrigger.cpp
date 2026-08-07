#include "Map/MapLayerTrigger.h"

#include "Components/BoxComponent.h"
#include "Common/CollisionChannels.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MapLayerTrigger)

AMapLayerTrigger::AMapLayerTrigger(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);

	TriggerBox->SetBoxExtent(FVector(100.0f));
	ConfigureTriggerCollision();
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleTriggerBeginOverlap);
}

void AMapLayerTrigger::BeginPlay()
{
	Super::BeginPlay();

	ConfigureTriggerCollision();
}

void AMapLayerTrigger::HandleTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	APdPlayerState* PdPlayerState = Pawn->GetPlayerState<APdPlayerState>();
	if (!PdPlayerState)
	{
		return;
	}

	PdPlayerState->GetPlayerMatchComponent()->SetPlayerMapRegion(TargetMapRegion);
}

void AMapLayerTrigger::ConfigureTriggerCollision() const
{
	if (!TriggerBox)
	{
		return;
	}

	TriggerBox->SetCollisionProfileName(TEXT("Custom"));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(LabCollisionChannels::OverlapBox());
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
}
