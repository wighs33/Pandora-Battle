#pragma once

#include "CoreMinimal.h"
#include "Story/StoryStep.h"
#include "Story/StoryTypes.h"
#include "StoryDialogueSteps.generated.h"

/** 대사 한 줄. 플레이어가 넘길 때까지 기다린다. */
UCLASS(DisplayName = "Line")
class LABPROJECT_API UStoryLineStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Line")
	FName Speaker;

	/** StoryText 표의 키 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Line")
	FName TextKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Line")
	EStoryExpression Expression = EStoryExpression::Neutral;

	/** 말하면서 하는 동작. 비우면 하던 동작을 이어 간다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Line")
	FName Clip;

	/** 외치는 대사: 글자가 크고 화면이 울린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Line")
	bool bShout = false;

	/** 말하는 인물이 보이지 않는 대사(검은 화면, 컷 밖). 인물의 표정과 동작을 바꾸지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Line")
	bool bUnseen = false;
};

/** 장 제목을 화면 가운데에 띄운다. */
UCLASS(DisplayName = "Title Card")
class LABPROJECT_API UStoryTitleCardStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Title")
	FName ChapterKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Title")
	FName TitleKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Title", meta = (ClampMin = "0.1", Units = "s"))
	float Seconds = 3.2f;
};

/** 화면을 한 색으로 덮거나 걷는다. */
UCLASS(DisplayName = "Fade")
class LABPROJECT_API UStoryFadeStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Fade")
	FLinearColor Color = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Fade", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Alpha = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Fade", meta = (ClampMin = "0.0", Units = "s"))
	float Seconds = 0.6f;
};

/** 눈꺼풀을 여닫는다(주인공의 눈으로 보는 장면). */
UCLASS(DisplayName = "Eyelids")
class LABPROJECT_API UStoryEyelidStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	/** 눈을 뜬 정도(0 감음, 1 다 뜸) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Eyelids", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Openness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Eyelids", meta = (ClampMin = "0.0", Units = "s"))
	float Seconds = 0.5f;
};

/** 넘김 키를 여러 번 눌러 버티거나 눈을 뜬다. 시간이 다 되면 누르지 않아도 넘어간다. */
UCLASS(DisplayName = "Mash")
class LABPROJECT_API UStoryMashStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Mash")
	FName PromptKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Mash", meta = (ClampMin = "1"))
	int32 Presses = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Mash", meta = (ClampMin = "0.1", Units = "s"))
	float TimeLimit = 8.f;

	/** 차오른 막대가 1초에 줄어드는 양(0~1). 0이면 줄지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Mash", meta = (ClampMin = "0.0"))
	float DecayPerSecond = 0.f;

	/** 차오른 만큼 눈을 뜬다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Mash")
	bool bOpensEyes = false;

	/** 다 채워도 시간이 다 될 때까지 끝나지 않는다(버텨도 결국 떨어지는 장면). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Mash")
	bool bAlwaysRunsOut = false;
};

/** 플레이어가 직접 걸어 자리까지 간다. */
UCLASS(DisplayName = "Player Walk")
class LABPROJECT_API UStoryPlayerWalkStep : public UStoryStep
{
	GENERATED_BODY()

public:
	virtual TUniquePtr<FStoryTask> Start(AStoryDirector& Director) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Walk")
	FName GoalMark;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Walk", meta = (ClampMin = "10.0", Units = "cm"))
	float Radius = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Walk")
	FName ObjectiveKey;
};
