#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "AOEIndicatorGameplayCue.generated.h"

class UDecalComponent;
class UMaterialInterface;

UCLASS(Blueprintable)
class LABPROJECT_API AAOEIndicatorGameplayCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AAOEIndicatorGameplayCue();

	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|AOE")
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|AOE", meta = (ClampMin = "0.0"))
	float DecalDepth = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|AOE", meta = (ClampMin = "0.0"))
	float FallbackDecalDiameter = 512.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|AOE")
	bool bOverrideLocalDecalColor = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|AOE")
	FLinearColor LocalDecalColor = FLinearColor(0.0f, 8.0f, 0.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|AOE")
	FLinearColor NonLocalDecalColor = FLinearColor(40.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!GameplayCue|AOE")
	TObjectPtr<UDecalComponent> SpawnedDecalComponent = nullptr;

private:
	void DestroySpawnedDecal();
};
