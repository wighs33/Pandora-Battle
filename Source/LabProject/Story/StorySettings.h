#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "StorySettings.generated.h"

class UInputAction;
class UInputMappingContext;
class UTexture2D;

/** 스토리 모드가 함께 쓰는 애셋. 장마다 다른 것은 장 정의가 담는다. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Story"))
class LABPROJECT_API UStorySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 대사 글자 뒤에 까는 아래쪽 그늘(위로 갈수록 투명) */
	UPROPERTY(Config, EditAnywhere, Category = "!Story|Screen")
	TSoftObjectPtr<UTexture2D> DialogueShadeTexture;

	/** 위 눈꺼풀(아래 가장자리가 둥글고 흐리다). 아래 눈꺼풀은 뒤집어 쓴다. */
	UPROPERTY(Config, EditAnywhere, Category = "!Story|Screen")
	TSoftObjectPtr<UTexture2D> EyelidTexture;

	/** 대사가 다 나오면 깜빡이는 넘김 표시 */
	UPROPERTY(Config, EditAnywhere, Category = "!Story|Screen")
	TSoftObjectPtr<UTexture2D> NextMarkTexture;

	UPROPERTY(Config, EditAnywhere, Category = "!Story|Input")
	TSoftObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(Config, EditAnywhere, Category = "!Story|Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Config, EditAnywhere, Category = "!Story|Input")
	TSoftObjectPtr<UInputAction> LookAction;
};
