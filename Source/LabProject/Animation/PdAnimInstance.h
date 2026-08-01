#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PdAnimInstance.generated.h"

class UCharacterMovementComponent;

DECLARE_LOG_CATEGORY_EXTERN(CommonAnimInstanceLog, Log, All);

/**
 * UObject 상태는 게임 스레드에서 값 스냅샷으로 수집하고,
 * 애니메이션 워커 스레드에서는 스냅샷만 소비합니다.
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
	virtual void NativeUninitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	// Anim notify callbacks
	UFUNCTION()
	void AnimNotify_RedrawBow();

protected:
	//-----------------------------------------------------------------------------
	// Game-thread snapshot
	//-----------------------------------------------------------------------------

	struct FGameThreadSnapshot
	{
		FVector3f Velocity = FVector3f::ZeroVector;
		float Direction = 0.f;
		float AimYaw = 0.f;
		float AimPitch = 0.f;
		uint32 SourceRevision = 0;
		bool bIsFalling = false;
		bool bIsOnGround = true;
		bool bIsCrouching = false;
		bool bIsValid = false;
	};

	void UpdateLocationData(const FGameThreadSnapshot& Snapshot);

	void UpdateMovementStates(const FGameThreadSnapshot& Snapshot);

	void UpdateAcceleration(float DeltaSeconds);

	void UpdateAimingData(const FGameThreadSnapshot& Snapshot);

	void UpdateGrappleState();

protected:
	//-----------------------------------------------------------------------------
	// Cached UObject references (game thread only)
	//-----------------------------------------------------------------------------

	UPROPERTY(Transient, BlueprintReadOnly, Category = "References")
	TObjectPtr<ACharacter> CachedCharacter;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "References")
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	//-----------------------------------------------------------------------------
	// Location data
	//-----------------------------------------------------------------------------

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	FVector3f Velocity = FVector3f::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	float GroundSpeed = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	float Direction = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	float VerticalVelocity = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Location")
	FVector3f Acceleration = FVector3f::ZeroVector;

	//-----------------------------------------------------------------------------
	// Movement states
	//-----------------------------------------------------------------------------

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsMoving = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsFalling = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsJumping = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsCrouching = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States")
	bool bIsOnGround = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|States", meta = (DisplayName = "Is Grappling"))
	bool bIsGrappling = false;

	//-----------------------------------------------------------------------------
	// Aiming data
	//-----------------------------------------------------------------------------

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Aiming")
	float AimYaw = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation Data|Aiming")
	float AimPitch = 0.f;

	//-----------------------------------------------------------------------------
	// Settings
	//-----------------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation Data|Settings")
	float MovingSpeedThreshold = 3.f;

private:
	void RefreshGameThreadReferences(bool bForceNewRevision = false);
	void CaptureGameThreadSnapshot();
	void ResetThreadSafeAnimationData();

	FGameThreadSnapshot GameThreadSnapshot;
	FVector3f PreviousVelocity = FVector3f::ZeroVector;
	uint32 GameThreadSourceRevision = 0;
	uint32 LastProcessedSourceRevision = 0;
	bool bHasPreviousVelocity = false;
};
