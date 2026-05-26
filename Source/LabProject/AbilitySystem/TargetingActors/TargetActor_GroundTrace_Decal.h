#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor_GroundTrace.h"
#include "TargetActor_GroundTrace_Decal.generated.h"

class UDecalComponent;
class UMaterialInterface;
class USceneComponent;

UCLASS(Blueprintable, notplaceable)
class LABPROJECT_API ATargetActor_GroundTrace_Decal : public AGameplayAbilityTargetActor_GroundTrace
{
	GENERATED_BODY()

public:
	ATargetActor_GroundTrace_Decal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Targeting|Components")
	TObjectPtr<USceneComponent> DefaultSceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Decal")
	TObjectPtr<UMaterialInterface> Decal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Decal", meta = (ClampMin = "0.0"))
	double DecalSize = 512.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Decal")
	FLinearColor DecalColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void DestroySpawnedDecal();

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> SpawnedDecalComponent;
};
