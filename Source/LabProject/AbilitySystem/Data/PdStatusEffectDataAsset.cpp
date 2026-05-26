#include "AbilitySystem/Data/PdStatusEffectDataAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdStatusEffectDataAsset)

FPrimaryAssetId UPdStatusEffectDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("StatusEffect"), GetFName());
}
