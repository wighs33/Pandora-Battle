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

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	AMapLayerTrigger(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ConfigureTriggerCollision() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Map")
	TObjectPtr<UBoxComponent> TriggerBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Map")
	EPlayerMapRegion TargetMapRegion = EPlayerMapRegion::Dome;
};
