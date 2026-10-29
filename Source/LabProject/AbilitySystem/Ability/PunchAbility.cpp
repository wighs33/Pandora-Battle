#include "AbilitySystem/Ability/PunchAbility.h"

#include "Common/PdGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PunchAbility)

UPunchAbility::UPunchAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(PdGameplayTags::Action_Punch);
	SetAssetTags(AbilityAssetTags);
}
