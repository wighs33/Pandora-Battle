#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/PdMapTypes.h"
#include "MapLayerTrigger.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS(Blueprintable)
class LABPROJECT_API AMapLayerTrigger : public AActor
{
	GENERATED_BODY()

public:
	AMapLayerTrigger(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Map")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Map")
	EPlayerMapRegion TargetMapRegion = EPlayerMapRegion::Dome;

	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	void ConfigureTriggerCollision() const;
};
