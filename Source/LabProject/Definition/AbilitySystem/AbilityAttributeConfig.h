#pragma once

#include "AttributeSet.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilityAttributeConfig.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdAttributeTagMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute", meta = (Categories = "Status"))
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute")
	FGameplayAttribute Attribute;

	bool IsValid() const { return StatTag.IsValid() && Attribute.IsValid(); }
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FPdAttributeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute", meta = (TitleProperty = "StatTag"))
	TArray<FPdAttributeTagMapping> AttributeMappings;

	bool HasAnyData() const { return !AttributeMappings.IsEmpty(); }
};
