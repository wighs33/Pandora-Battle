#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackTelegraphActor.generated.h"

class ACharacter;
class UAnimMontage;
class UDecalComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EAttackTelegraphShape : uint8
{
	None,
	/** 공격 대상 발밑의 원. 도약이 시작될 때까지 대상을 따라가다 그 자리에 멈춘다. */
	TargetCircle,
	/** 몬스터 앞쪽 부채꼴. 몬스터를 따라 움직이고 돈다. */
	FrontCone,
};

/** 공격 예고 장판의 모양. 언제 가득 차고 사라질지는 공격 몽타주의 노티파이에서 읽는다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FAttackTelegraphSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Telegraph")
	EAttackTelegraphShape Shape = EAttackTelegraphShape::None;

	/** 원은 반지름, 부채꼴은 몬스터 중심부터의 길이. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Telegraph", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float Radius = 400.0f;

	/** 부채꼴의 반각. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AI|Monster|Telegraph",
		meta = (ClampMin = "1.0", ClampMax = "180.0", ForceUnits = "deg", EditCondition = "Shape == EAttackTelegraphShape::FrontCone"))
	float HalfAngle = 30.0f;
};

/**
 * 공격 예고 장판을 그리는 로컬 데칼. 복제하지 않고, 화면을 그리는 인스턴스마다 공격 몽타주와 함께 만든다.
 * 채움·깜빡임·사라짐은 셰이더(Shaders/AttackTelegraph.ush)가 월드 시간으로 계산하고, 이 액터는 위치만 맞춘다.
 * 공격 피해 구간이 시작될 때 가득 차고, 피해 구간이 끝나면 사라진다.
 */
UCLASS(NotBlueprintable)
class LABPROJECT_API AAttackTelegraphActor : public AActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Tick(float DeltaSeconds) override;

	// Public API ------------------------------------------------------------------------------------------------------
	AAttackTelegraphActor();

	/** Monster가 AttackMontage를 재생할 때 장판을 띄운다. 모양이 없거나 몽타주에 피해 구간이 없으면 nullptr이다. */
	static AAttackTelegraphActor* SpawnForAttack(ACharacter& Monster, const FAttackTelegraphSettings& Settings,
		UMaterialInterface* Material, const UAnimMontage& AttackMontage, float PlayRate, AActor* Target);

private:
	UPROPERTY(VisibleAnywhere, Category = "!AI|Monster|Telegraph")
	TObjectPtr<UDecalComponent> Decal;

	TWeakObjectPtr<AActor> FollowTarget;
	double FollowUntilTime = 0.0;
};
