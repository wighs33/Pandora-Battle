#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/PrimaryAssetId.h"
#include "InteractableInterface.generated.h"

class AActor;

UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class LABPROJECT_API IInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction")
	bool CanInteract(AActor* InteractingActor);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction")
	bool Interact(AActor* InteractingActor);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction")
	FText GetInteractText(AActor* InteractingActor);

	// ?�호?�용 보상?�로 지급할 ?�이???�의 목록??반환?�니??
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction|Reward")
	void GetRewardItems(TArray<FPrimaryAssetId>& OutItemDefinitionList);

	// ?�호?�용 보상?�로 지급할 ?�킨 ?�의 목록??반환?�니??
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction|Reward")
	void GetRewardSkins(TArray<FPrimaryAssetId>& OutSkinDefinitionList);

	// ?�호?�용 보상?�로 지급할 ?�도???�의 목록??반환?�니??
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction|Reward")
	void GetRewardPandoras(TArray<FPrimaryAssetId>& OutPandoraDefinitionList);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "!Interaction|Reward")
	void OnRewardsClaimed(AActor* RewardReceiver);
};
