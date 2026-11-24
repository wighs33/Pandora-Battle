#pragma once

#include "CoreMinimal.h"
#include "Story/StoryStep.h"
#include "Story/StoryTypes.h"
#include "StoryStageSteps.generated.h"

class USoundBase;

/** 카메라 컷을 바꾼다. */
UCLASS(DisplayName = "Shot")
class LABPROJECT_API UStoryShotStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shot")
	FStoryShot Shot;

	/** 이어 움직이는 컷이 다 움직일 때까지 기다린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shot")
	bool bWaitForBlend = false;
};

UCLASS(DisplayName = "Wait")
class LABPROJECT_API UStoryWaitStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Wait", meta = (ClampMin = "0.0", Units = "s"))
	float Seconds = 0.5f;
};

/** 화면을 흔든다. */
UCLASS(DisplayName = "Shake")
class LABPROJECT_API UStoryShakeStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shake", meta = (ClampMin = "0.0", Units = "cm"))
	float Strength = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shake", meta = (ClampMin = "0.0", Units = "s"))
	float Seconds = 0.8f;
};

/** 포탈을 열거나 닫는다. */
UCLASS(DisplayName = "Portal")
class LABPROJECT_API UStoryPortalStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Portal")
	bool bOpen = true;
};

UCLASS(DisplayName = "Sound")
class LABPROJECT_API UStorySoundStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Sound")
	TObjectPtr<USoundBase> Sound;
};
