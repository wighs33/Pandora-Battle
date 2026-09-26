#include "Map/PlayerMapRegionTrigger.h"

#include "Components/BoxComponent.h"
#include "Common/CollisionChannels.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerMapRegionTrigger)

APlayerMapRegionTrigger::APlayerMapRegionTrigger(const FObjectInitializer& ObjectInitializer)
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

void APlayerMapRegionTrigger::BeginPlay()
{
	Super::BeginPlay();

	ConfigureTriggerCollision();
}

void APlayerMapRegionTrigger::HandleTriggerBeginOverlap(
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

void APlayerMapRegionTrigger::ConfigureTriggerCollision() const
{
	TriggerBox->SetCollisionProfileName(TEXT("Custom"));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(LabCollisionChannels::OverlapBox());
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetCollisionResponseToChannel(LabCollisionChannels::HitableBody(), ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
}
