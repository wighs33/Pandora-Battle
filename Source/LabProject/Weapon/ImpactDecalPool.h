#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ImpactDecalPool.generated.h"

class UDecalComponent;
class UMaterialInterface;

USTRUCT()
struct FImpactDecalSlot
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> Decal;

	FTimerHandle HideTimer;
};

/**
 * 총알 탄흔 데칼을 정해진 수만큼만 만들어 차례로 돌려 쓴다.
 *
 * 한 발마다 데칼 컴포넌트를 새로 만들고 수명이 다하면 지우면, 연사하는 동안 컴포넌트가 계속 생겼다 사라진다.
 * 칸을 다 쓰면 가장 먼저 놓은 탄흔 자리에 새 탄흔을 놓고, 수명이 다한 탄흔은 숨겨 둔다. 전용 서버는 그리지 않으므로 만들지 않는다.
 */
UCLASS()
class LABPROJECT_API UImpactDecalPool : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 탄흔 하나를 놓고 LifeSpan초 뒤에 숨긴다. LifeSpan이 0이면 그 칸을 다시 쓸 때까지 남는다. */
	void PlaceDecal(UMaterialInterface& Material, const FVector& Size, const FVector& Location, const FRotator& Rotation, float LifeSpan);

private:
	UDecalComponent* CreateDecal() const;
	void HideDecal(int32 SlotIndex);

	UPROPERTY(Transient)
	TArray<FImpactDecalSlot> Slots;

	int32 NextSlotIndex = 0;
};
