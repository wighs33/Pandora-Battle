#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class LABPROJECT_API IInteractableInterface
{
	GENERATED_BODY()

public:
	// 상호작용 보상으로 지급할 아이템 정의 목록을 반환합니다.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction|Reward")
	void GetRewardItems(TArray<FPrimaryAssetId>& OutItemDefinitionList);

	// 상호작용 보상으로 지급할 스킨 정의 목록을 반환합니다.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction|Reward")
	void GetRewardSkins(TArray<FPrimaryAssetId>& OutSkinDefinitionList);

	// 상호작용 보상으로 지급할 판도라 정의 목록을 반환합니다.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction|Reward")
	void GetRewardPandoras(TArray<FPrimaryAssetId>& OutPandoraDefinitionList);
};
