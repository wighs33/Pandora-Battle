#pragma once

#include "AttributeSet.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AttributeDefinition.generated.h"

/**
 * <태그-어트리뷰트 매핑 엔트리>
 * - 스탯 태그와 Gameplay Attribute를 1:1로 묶는 데이터입니다.
 * - UAttributeDefinition의 Entries 배열에서 사용합니다.
 * - 태그 기반 로직을 실제 Attribute로 연결할 때 사용합니다.
 */
USTRUCT(BlueprintType)
struct FAttributeDefinitionEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute", meta = (Categories = "Status"))
	FGameplayTag StatTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute")
	FGameplayAttribute Attribute;
};

/**
 * <스탯 태그 해석용 데이터 애셋>
 * - 스탯 태그를 Gameplay Attribute로 변환하는 테이블입니다.
 * - 내부의 Entries를 기준으로 매핑을 찾습니다.
 * - 태그 중심 시스템과 GAS Attribute를 연결할 때 사용합니다.
 * - 에디터의 DA_Attribute로 관리
 */
UCLASS(BlueprintType)
class LABPROJECT_API UAttributeDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 태그를 실제 Attribute로 변환 */
	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Attribute")
	bool ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute")
	TArray<FAttributeDefinitionEntry> Entries;
};
