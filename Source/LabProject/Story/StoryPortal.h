#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StoryPortal.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/** 다른 세계로 이어지는 포탈. 열리면 튀어나오듯 커지고, 돌 조각 고리가 돌며, 닫히면 오므라든다. */
UCLASS()
class LABPROJECT_API AStoryPortal : public AActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Tick(float DeltaSeconds) override;

	// Public API ------------------------------------------------------------------------------------------------------
	AStoryPortal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void Open();
	void Close();
	/** 빨려 들어간 순간처럼 한 번 크게 번쩍인다. */
	void Flash();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<USceneComponent> Ring;

	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<UStaticMeshComponent> Swirl;

	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<UStaticMeshComponent> Shards;

	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<UStaticMeshComponent> Rim;

	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<UPointLightComponent> Light;

	/** 열려 있을 때의 빛 세기(cd) */
	UPROPERTY(EditAnywhere, Category = "!Story", meta = (ClampMin = "0.0"))
	float LightIntensity = 60.f;

	/** 돌 조각 고리와 빛 테가 도는 빠르기(도/초) */
	UPROPERTY(EditAnywhere, Category = "!Story")
	float ShardSpin = 26.f;

	float OpenScale = 0.f;
	float OpenVelocity = 0.f;
	float FlashAmount = 0.f;
	bool bOpen = false;
};
