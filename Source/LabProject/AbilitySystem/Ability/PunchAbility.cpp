#include "AbilitySystem/Ability/PunchAbility.h"

#include "Common/LabGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PunchAbility)

UPunchAbility::UPunchAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::Action_Punch);
	SetAssetTags(AbilityAssetTags);
}
