#pragma once

#include "Common/Enum_Direction.h"
#include "CoreMinimal.h"
#include "PandoraLoadoutTypes.generated.h"

class UPandoraDefinition;

USTRUCT(BlueprintType)
struct LABPROJECT_API FPandoraLoadoutSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora|Loadout")
	EEnum_Direction Direction = EEnum_Direction::Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Pandora|Loadout")
	TObjectPtr<UPandoraDefinition> PandoraDefinition = nullptr;
};

namespace PandoraLoadout
{
	bool IsLoadoutDirection(EEnum_Direction Direction);
	EEnum_Direction GetDirectionFromLoadoutNumber(int32 LoadoutNumber);
	int32 GetLoadoutNumberFromDirection(EEnum_Direction Direction);
	int32 NormalizeLoadoutNumber(int32 LoadoutNumber);
	FPandoraLoadoutSlot* FindSlot(TArray<FPandoraLoadoutSlot>& Slots, EEnum_Direction Direction);
	const FPandoraLoadoutSlot* FindSlot(const TArray<FPandoraLoadoutSlot>& Slots, EEnum_Direction Direction);
}
