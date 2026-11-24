#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Story/StoryTypes.h"
#include "StoryCharacter.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UStoryAnimInstance;
class UStoryCharacterDefinition;
struct FInputActionValue;

/** 인물이 혼자 돌아다니는 범위. 가운데 인물 둘레를 느긋하게 돌고, 말하는 동안에는 멈춘다. */
struct FStoryWander
{
	TWeakObjectPtr<const AActor> Center;
	/** 비켜 갈 인물 */
	TArray<TWeakObjectPtr<const AActor>> Avoid;
	float NearRadius = 220.f;
	float FarRadius = 420.f;
	/** 가운데 인물을 기준으로 돌아다닐 방향(월드 yaw)과 그 양옆 폭(도) */
	float FacingYaw = 0.f;
	float HalfArc = 80.f;
	float Speed = 70.f;
};

/**
 * 이야기 인물. 스틱맨 몸에 옷·머리카락·얼굴판을 입히고, 연출이 시키는 동작·표정·이동을 한다.
 * 연출 중에는 자리를 직접 옮기고(충돌 없음), 플레이어가 조종할 때만 캐릭터 이동을 쓴다.
 */
UCLASS()
class LABPROJECT_API AStoryCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Public API ------------------------------------------------------------------------------------------------------
	AStoryCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void Initialize(const UStoryCharacterDefinition& InDefinition, const TMap<EStoryEmote, TObjectPtr<UStaticMesh>>& InEmoteMeshes);
	const UStoryCharacterDefinition* GetDefinition() const { return Definition; }

	// 동작과 표정
	/** 이름이 비면 기본 동작으로 돌아간다. 되풀이하지 않는 동작은 끝난 자세로 멈춘다. */
	void PlayClip(FName ClipName, bool bLoop, float BlendSeconds);
	float GetClipLength(FName ClipName) const;
	void SetExpression(EStoryExpression InExpression);
	/** 말하는 동안 입을 움직인다. */
	void SetTalking(bool bInTalking);
	bool IsTalking() const { return bTalking; }
	void ShowEmote(EStoryEmote Emote, float Seconds);
	/** 가슴에서 새어 나오는 빛. 세기 0이면 꺼진다. */
	void SetGlow(float InTargetIntensity);

	// 보는 곳
	void LookAt(const AActor* Target) { LookTarget = Target; }
	/** 누워 있을 때처럼 머리를 돌리면 어색한 동작에서는 끈다. */
	void SetHeadLookEnabled(const bool bEnabled) { bHeadLookEnabled = bEnabled; }
	void FaceYaw(float WorldYaw, bool bInstant);
	void FaceLocation(const FVector& Location, bool bInstant);
	/** 몸이 향하려는 쪽. 아직 돌고 있어도 다 돈 뒤의 방향이다. */
	float GetFacingYaw() const;
	FVector GetHeadLocation() const;

	// 이동
	/** 바닥 좌표로 옮긴다(높이는 인물이 맞춘다). bKeepFacing이면 보던 쪽을 본 채 움직인다. */
	void MoveTo(const FVector& GroundLocation, float Speed, bool bKeepFacing);
	bool IsMoving() const { return MoveTarget.IsSet(); }
	void PlaceAt(const FVector& GroundLocation, float WorldYaw);
	FVector GetGroundLocation() const;
	void StartWander(const FStoryWander& InWander);
	void StopWander();
	bool IsWandering() const { return Wander.IsSet(); }

	/** 플레이어가 직접 걷는 상태. 카메라가 등 뒤를 따라간다. */
	void SetPlayerControlled(bool bControlled);
	/** 문 속으로 빨려 들 때처럼 크기를 바꾼다. */
	void SetBodyScale(float Scale);
	void AddMeshSpin(float Degrees);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	USkeletalMeshComponent* AddOutfitMesh(USkeletalMesh* InMesh);
	UStoryAnimInstance* GetStoryAnimInstance() const;
	void TickMovement(float DeltaSeconds);
	void TickWander(float DeltaSeconds);
	void TickFacing(float DeltaSeconds);
	void TickHeadLook(float DeltaSeconds);
	void TickFace(float DeltaSeconds);
	void TickEmote(float DeltaSeconds);
	void TickBob(float DeltaSeconds);
	void PlayLocomotion(const FVector& Velocity);
	bool ChooseWanderSpot(FVector& OutSpot) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<const UStoryCharacterDefinition> Definition;

	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<USkeletalMeshComponent> FaceMeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<UStaticMeshComponent> EmoteComponent;

	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "!Story")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient)
	TMap<EStoryEmote, TObjectPtr<UStaticMesh>> EmoteMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FaceMaterial;

	TWeakObjectPtr<const AActor> LookTarget;
	bool bHeadLookEnabled = true;
	float HeadYaw = 0.f;
	float HeadPitch = 0.f;

	EStoryExpression Expression = EStoryExpression::Neutral;
	bool bTalking = false;
	float TalkElapsed = 0.f;
	float BlinkCountdown = 2.f;
	int32 ShownFaceCell = INDEX_NONE;

	/** 정해 둔 동작을 재생 중이면 걷기가 끝난 뒤에도 그 동작으로 돌아온다. 비면 기본 동작이다. */
	FName HeldClip;
	bool bHeldClipLoops = true;
	bool bPlayingLocomotion = false;

	TOptional<FVector> MoveTarget;
	float MoveSpeed = 0.f;
	bool bMoveKeepsFacing = false;
	float DesiredYaw = 0.f;

	TOptional<FStoryWander> Wander;
	float WanderPause = 0.f;

	float EmoteElapsed = 0.f;
	float EmoteSeconds = 0.f;
	float GlowTarget = 0.f;
	float BobElapsed = 0.f;
	float MeshBaseHeight = 0.f;
	bool bPlayerControlled = false;
};
