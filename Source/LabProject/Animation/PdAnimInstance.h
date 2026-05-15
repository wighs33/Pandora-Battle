#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PdAnimInstance.generated.h"

class UCharacterMovementComponent;

DECLARE_LOG_CATEGORY_EXTERN(CommonAnimInstanceLog, Log, All);

/**
 * < Thread Safe 애니메이션 데이터를 제공하는 기본 AnimInstance >
 *
 * - 멀티스레드 환경에서 안전하게 애니메이션 데이터를 업데이트합니다.
 * - NativeThreadSafeUpdateAnimation()에서 모든 데이터를 계산하여
 * Worker Thread에서 AnimGraph 평가 시 사용할 수 있도록 합니다.
 */
UCLASS()
class LABPROJECT_API UPdAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	//-----------------------------------------------------------------------------
	// Timing hooks
	//-----------------------------------------------------------------------------

	virtual void NativeInitializeAnimation() override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	// Anim notify callbacks
	UFUNCTION()
	void AnimNotify_RedrawBow();

protected:
	//-----------------------------------------------------------------------------
	// Thread Safe 데이터 업데이트
	//-----------------------------------------------------------------------------

	/** MovementComponent에서 데이터를 가져와 Thread Safe 변수에 저장합니다 */
	void UpdateLocationData(float DeltaSeconds);

	/** 이동 상태를 업데이트합니다 */
	void UpdateMovementStates();

	/** 가속도를 계산합니다 */
	void UpdateAcceleration(float DeltaSeconds);

	/** 조준 데이터를 업데이트합니다 */
	void UpdateAimingData();

protected:
	//-----------------------------------------------------------------------------
	// 캐시된 레퍼런스
	//-----------------------------------------------------------------------------

	/** 캐시된 Character */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "References")
	TObjectPtr<ACharacter> CachedCharacter;

	/** 캐시된 CharacterMovementComponent */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "References")
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	//-----------------------------------------------------------------------------
	// Thread Safe 애니메이션 데이터 - Location
	//-----------------------------------------------------------------------------

	/** 월드 공간 속도 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	FVector3f Velocity = FVector3f::ZeroVector;

	/** 2D 이동 속도 (XY 평면) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	float GroundSpeed = 0.f;

	/** 캐릭터 기준 이동 방향 각도 (-180 ~ 180) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	float Direction = 0.f;

	/** 수직 속도 (Z축, 양수=상승, 음수=하강) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	float VerticalVelocity = 0.f;

	/** 월드 공간 가속도 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	FVector3f Acceleration = FVector3f::ZeroVector;

	//-----------------------------------------------------------------------------
	// Thread Safe 애니메이션 데이터 - States
	//-----------------------------------------------------------------------------

	/** 캐릭터가 이동 중인지 여부 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsMoving = false;

	/** 캐릭터가 낙하 중인지 여부 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsFalling = false;

	/** 캐릭터가 점프 중인지 여부 (상승 중) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsJumping = false;

	/** 캐릭터가 웅크리고 있는지 여부 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsCrouching = false;

	/** 캐릭터가 땅에 있는지 여부 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsOnGround = true;

	//-----------------------------------------------------------------------------
	// 애니메이션 데이터 - Aiming
	//-----------------------------------------------------------------------------

	/** 조준 Yaw 각도 (-180 ~ 180, 좌우) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Aiming")
	float AimYaw = 0.f;

	/** 조준 Pitch 각도 (-90 ~ 90, 상하) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Aiming")
	float AimPitch = 0.f;

	//-----------------------------------------------------------------------------
	// 애니메이션 데이터 - Settings
	//-----------------------------------------------------------------------------

	/** 이동 중으로 판정하는 최소 속도 임계값 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Data|Settings")
	float MovingSpeedThreshold = 3.f;

private:
	/** 이전 프레임 속도 (가속도 계산용) */
	FVector3f PreviousVelocity = FVector3f::ZeroVector;
};
