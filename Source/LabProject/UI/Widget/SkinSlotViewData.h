#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkinSlotViewData.generated.h"

class USkinInstance;

UCLASS(BlueprintType)
class LABPROJECT_API USkinSlotViewData : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(
		int32 InSlotIndex,
		USkinInstance* InSkinInstance,
		bool bInAssigned = false);

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	USkinInstance* GetSkinInstance() const { return SkinInstance; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	bool IsEmpty() const { return SkinInstance == nullptr; }

	UFUNCTION(BlueprintPure, Category = "!UI|Skin")
	bool IsAssigned() const { return bAssigned; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Skin", meta = (AllowPrivateAccess = "true"))
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinInstance> SkinInstance;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!UI|Skin", meta = (AllowPrivateAccess = "true"))
	bool bAssigned = false;
};
