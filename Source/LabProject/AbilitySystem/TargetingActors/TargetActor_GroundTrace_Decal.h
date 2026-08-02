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

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "!Targeting|Decal")
	void ConfigureDecalGrowth(double InStartSize, double InTargetSize, double InDuration);

	void ConfigureGroundProjection(double InTraceStartHeight, double InTraceDepth);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Targeting|Components")
	TObjectPtr<USceneComponent> DefaultSceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Decal")
	TObjectPtr<UMaterialInterface> Decal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Decal", meta = (ClampMin = "0.0"))
	double DecalSize = 512.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Decal", meta = (ClampMin = "1.0"))
	double DecalDepth = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Decal")
	bool bOverrideDecalColor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Decal")
	FLinearColor DecalColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Ground", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double GroundProjectionTraceStartHeight = 500.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Targeting|Ground", meta = (ClampMin = "100.0", ForceUnits = "cm"))
	double GroundProjectionTraceDepth = 100000.0;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FHitResult PerformTrace(AActor* InSourceActor) override;

private:
	void DestroySpawnedDecal();
	void ApplyDecalSize(double InDecalSize) const;
	void ApplyGroundHitToDecal(const FHitResult& GroundHit);
	void ApplyCachedGroundHitToDecal() const;
	void SetTargetingDecalVisible(bool bVisible) const;
	void UpdateDecalGrowth();
	void MarkGroundTraceFailure();
	void MarkGroundTraceSuccess();

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> SpawnedDecalComponent;

	UPROPERTY(Transient)
	bool bDecalGrowthActive = false;

	UPROPERTY(Transient)
	double DecalGrowthStartSize = 512.0;

	UPROPERTY(Transient)
	double DecalGrowthTargetSize = 512.0;

	UPROPERTY(Transient)
	double DecalGrowthDuration = 0.0;

	UPROPERTY(Transient)
	double DecalGrowthStartTime = 0.0;

	UPROPERTY(Transient)
	FVector LastGroundDecalLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bHasLastGroundDecalLocation = false;

	bool bLastGroundTraceSucceeded = false;
};
