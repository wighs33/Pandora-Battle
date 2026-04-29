#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ItemInstance.generated.h"

class UItemDefinition;
DECLARE_LOG_CATEGORY_EXTERN(ItemInstanceLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UItemInstance : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!Item")
	FGuid GetOrCreateItemId();

	UFUNCTION(BlueprintPure, Category = "!Item")
	FGuid GetItemId() const { return ItemId; }

	void EnsureItemId();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Item")
	TObjectPtr<const UItemDefinition> ItemDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Item|Stat")
	TMap<FGameplayTag, float> Map_EnhancedStat_Magnitude;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Item")
	int32 Quantity = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Item")
	FGuid ItemId;
};
