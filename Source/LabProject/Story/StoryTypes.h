#pragma once

#include "CoreMinimal.h"
#include "StoryTypes.generated.h"

/** 얼굴판에 그려 둔 표정. 칸 번호는 AStoryCharacter가 정한다. */
UENUM(BlueprintType)
enum class EStoryExpression : uint8
{
	Neutral,
	Happy,
	Laugh,
	Surprised,
	Scared,
	Worried,
	Angry,
	Smug,
	Deadpan,
	Asleep,
	Blank,
	Squeezed
};

/** 머리 위로 튀어나오는 감정 표시. */
UENUM(BlueprintType)
enum class EStoryEmote : uint8
{
	Exclaim,
	Question,
	Dots,
	Sweat,
	Anger
};

/** 카메라 한 컷. 인물 머리를 기준으로 카메라 자리와 바라보는 점을 정한다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FStoryShot
{
	GENERATED_BODY()

	/** 기준 인물. 비우면 무대 원점(마법진 가운데)이 기준이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Shot")
	FName Subject;

	/** 함께 담을 인물. 정하면 두 머리의 가운데를 기준으로 삼는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Shot")
	FName Partner;

	/** 기준점에서 카메라까지(cm). 기준 인물이 보는 쪽이 +X, 오른쪽이 +Y다. bStageAxes면 무대 축을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Shot")
	FVector CameraOffset = FVector(260.0, 60.0, 10.0);

	/** 기준점에서 카메라가 바라보는 점까지(cm). 축은 CameraOffset과 같다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Shot")
	FVector AimOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Shot")
	bool bStageAxes = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Shot", meta = (ClampMin = "10.0", ClampMax = "120.0", Units = "deg"))
	float FieldOfView = 38.f;

	/** 앞 컷에서 이어 움직이는 시간. 0이면 바로 자른다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Shot", meta = (ClampMin = "0.0", Units = "s"))
	float BlendSeconds = 0.f;

	/** 컷이 이어지는 동안 카메라가 천천히 흐르는 속도(cm/s). 축은 CameraOffset과 같다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Shot")
	FVector Drift = FVector::ZeroVector;

	/** 기준 인물이 움직이면 카메라도 따라간다. 끄면 컷이 시작할 때 자리를 지킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Shot")
	bool bFollow = true;
};
