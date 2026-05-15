#pragma once

#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Common/Enum_Operation.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PdAbilitySystemComponent.generated.h"

class UGameplayEffect;

USTRUCT(BlueprintType)
struct FPdAttributeTagMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute", meta = (Categories = "Status"))
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute")
	float DefaultValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute")
	int32 DefaultValuePriority = 0;

	bool IsValid() const
	{
		return StatTag.IsValid() && Attribute.IsValid();
	}
};

USTRUCT(BlueprintType)
struct FPdAttributeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!AbilitySystem|Attribute", meta = (TitleProperty = "StatTag"))
	TArray<FPdAttributeTagMapping> AttributeMappings;

	bool HasAnyData() const
	{
		return !AttributeMappings.IsEmpty();
	}
};

/**
 * Project AbilitySystemComponent.
 * - GameFeature AttributeConfig binds stat tags to actual attributes.
 * - Native gameplay tags resolve shared SetByCaller tags.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UPdAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UPdAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnRegister() override;
	bool HasAbilityActorInfoAllocated() const { return AbilityActorInfo.IsValid(); }

	int32 AddAttributeConfig(const FPdAttributeConfig& AttributeConfig);
	void RemoveAttributeConfig(int32 AttributeConfigHandle);
	bool ApplyAttributeDefaultValues(const FPdAttributeConfig& AttributeConfig);

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	bool ApplyStatUpEffectByTag(TSubclassOf<UGameplayEffect> GameplayEffectClass, FGameplayTag StatTag, float Magnitude,
		EEnum_Operation Operation = EEnum_Operation::Add, float Level = 1.f);

	bool ApplyStatUpEffectByTags(TSubclassOf<UGameplayEffect> GameplayEffectClass, const TMap<FGameplayTag, float>& StatMagnitudes,
		EEnum_Operation Operation = EEnum_Operation::Add, float Level = 1.f);

	bool ResolveAttributeFromTag(const FGameplayTag& StatTag, FGameplayAttribute& OutAttribute) const;
	bool ResolveDamageMagnitudeSetByCallerTag(FGameplayTag& OutTag) const;
	bool ResolveStatUpOperationSetByCallerTag(FGameplayTag& OutTag) const;

protected:
	TMap<int32, FPdAttributeConfig> ActiveAttributeConfigs;
	TArray<int32> AttributeConfigOrder;
	int32 NextAttributeConfigHandle = 1;
};
