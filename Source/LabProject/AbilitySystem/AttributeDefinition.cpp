#include "AbilitySystem/AttributeDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AttributeDefinition)

/**
 *태그를 실제 Attribute로 변환 
 *  @param StatTag 인풋
 *  @param OutAttribute 레퍼런스 반환값
 */
bool UAttributeDefinition::ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const
{
	// =================================================================================================================
	// === 초기화 & 안전 가드
	
	OutAttribute = FGameplayAttribute();

	if (!StatTag.IsValid())
	{
		return false;
	}
	
	// =================================================================================================================
	// === 등록된 매핑 목록 순회해서 OutAttribute 얻기
	for (const FAttributeDefinitionEntry& Entry : Entries)
	{
		if (!Entry.StatTag.MatchesTagExact(StatTag) || !Entry.Attribute.IsValid())
		{
			continue;
		}

		OutAttribute = Entry.Attribute;
		return true;
	}

	return false;
}
