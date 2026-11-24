#pragma once

#include "CoreMinimal.h"
#include "Story/StoryStep.h"
#include "Story/StoryTypes.h"
#include "StoryActorSteps.generated.h"

/** 인물이 동작을 하고 표정을 짓는다. 되풀이하지 않는 동작은 끝까지, 되풀이하는 동작은 HoldSeconds만큼 기다린다. */
UCLASS(DisplayName = "Perform")
class LABPROJECT_API UStoryPerformStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Perform")
	FName Character;

	/** 비우면 기본 동작으로 돌아간다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Perform")
	FName Clip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Perform")
	bool bLoop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Perform", meta = (ClampMin = "0.0", Units = "s"))
	float BlendSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Perform", meta = (EditCondition = "bLoop", ClampMin = "0.0", Units = "s"))
	float HoldSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Perform")
	EStoryExpression Expression = EStoryExpression::Neutral;

	/** 동작하는 동안 말하는 인물 쪽으로 머리를 돌린다. 누워 있을 때는 끈다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Perform")
	bool bHeadLook = true;
};

/** 인물을 옮긴다. 자리 표시가 없으면 보는 쪽을 기준으로 Offset만큼 움직인다. */
UCLASS(DisplayName = "Move")
class LABPROJECT_API UStoryMoveStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Move")
	FName Character;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Move")
	FName Mark;

	/** 인물이 보는 쪽이 +X인 이동량(cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Move")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Move", meta = (ClampMin = "1.0", Units = "CentimetersPerSecond"))
	float Speed = 120.f;

	/** 보던 쪽을 본 채 움직인다(뒷걸음질). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Move")
	bool bKeepFacing = false;

	/** 걷지 않고 바로 그 자리로 옮긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Move")
	bool bTeleport = false;
};

/** 인물의 몸을 다른 인물이나 자리 쪽으로 돌린다. */
UCLASS(DisplayName = "Face")
class LABPROJECT_API UStoryFaceStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Face")
	FName Character;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Face")
	FName TargetCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Face")
	FName TargetMark;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Face", meta = (ClampMin = "0.0", Units = "s"))
	float HoldSeconds = 0.5f;
};

/** 머리 위에 감정 표시를 띄운다. */
UCLASS(DisplayName = "Emote")
class LABPROJECT_API UStoryEmoteStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Emote")
	TArray<FName> Characters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Emote")
	EStoryEmote Emote = EStoryEmote::Exclaim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Emote", meta = (ClampMin = "0.1", Units = "s"))
	float Seconds = 1.f;
};

/** 가슴에서 빛이 새어 나오게 하거나 거둔다. */
UCLASS(DisplayName = "Glow")
class LABPROJECT_API UStoryGlowStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Glow")
	FName Character;

	/** 빛의 세기(cd). 0이면 거둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Glow", meta = (ClampMin = "0.0"))
	float Intensity = 30.f;
};

/** 인물이 다른 인물 둘레를 느긋하게 돌아다니거나 멈춘다. */
UCLASS(DisplayName = "Wander")
class LABPROJECT_API UStoryWanderStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wander")
	FName Character;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wander")
	bool bWander = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wander")
	FName Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wander")
	TArray<FName> Avoid;

	/** 가운데 인물에서 돌아다닐 방향(월드 yaw)과 양옆 폭 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wander", meta = (Units = "deg"))
	float FacingYaw = -90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wander", meta = (ClampMin = "1.0", ClampMax = "180.0", Units = "deg"))
	float HalfArc = 80.f;

	/** 가운데 인물에서 떨어지는 거리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wander", meta = (ClampMin = "0.0", Units = "cm"))
	float NearRadius = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wander", meta = (ClampMin = "0.0", Units = "cm"))
	float FarRadius = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wander", meta = (ClampMin = "1.0", Units = "CentimetersPerSecond"))
	float Speed = 70.f;
};

/** 인물이 펑 하고 사라지거나 나타난다. */
UCLASS(DisplayName = "Vanish")
class LABPROJECT_API UStoryVanishStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Vanish")
	FName Character;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Vanish")
	bool bAppear = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Vanish", meta = (ClampMin = "0.01", Units = "s"))
	float Seconds = 0.16f;
};

/** 인물이 빙글빙글 돌며 자리 속으로 빨려 들어 사라진다. */
UCLASS(DisplayName = "Plunge")
class LABPROJECT_API UStoryPlungeStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Plunge")
	FName Character;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Plunge")
	FName Mark;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Plunge", meta = (ClampMin = "0.1", Units = "s"))
	float Seconds = 1.1f;
};
