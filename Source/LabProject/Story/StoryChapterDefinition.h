#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Story/StoryTypes.h"
#include "StoryChapterDefinition.generated.h"

class UStaticMesh;
class UStoryCharacterDefinition;
class UStoryStep;

/** 장에 나오는 인물 하나와 처음 설 자리. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FStoryCastMember
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Story")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Story")
	TObjectPtr<UStoryCharacterDefinition> Character;

	/** 처음 설 자리(AStoryMark의 이름) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Story")
	FName StartMark;
};

/** 이야기 한 장: 인물과 차례대로 재생할 단계. */
UCLASS(BlueprintType)
class LABPROJECT_API UStoryChapterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story")
	TArray<FStoryCastMember> Cast;

	/** 플레이어가 직접 걷는 인물 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story")
	FName PlayerCharacterId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Story")
	TMap<EStoryEmote, TObjectPtr<UStaticMesh>> EmoteMeshes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category = "!Story", meta = (TitleProperty = "{bWait}"))
	TArray<TObjectPtr<UStoryStep>> Steps;
};
