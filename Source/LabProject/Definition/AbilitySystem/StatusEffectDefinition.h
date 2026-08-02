#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StatusEffectDefinition.generated.h"

class UGameplayEffect;
class UTexture2D;
class UAbilitySystemComponent;
struct FGameplayEffectSpecHandle;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UStatusEffectDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UStatusEffectDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	double ResolveDamageMagnitude() const;
	float GetAttackDamageBonusPercent(const UAbilitySystemComponent* SourceAbilitySystemComponent) const;
	float CalculateDamageMagnitude(
		const UAbilitySystemComponent* SourceAbilitySystemComponent,
		float SkillScaledDamageMagnitude,
		float DamageScale = 1.0f) const;
	bool SetDamageMagnitude(
		FGameplayEffectSpecHandle& SpecHandle,
		const UAbilitySystemComponent* SourceAbilitySystemComponent,
		float SkillScaledDamageMagnitude,
		float DamageScale = 1.0f) const;
	void AppendRemovalPolicyTags(FGameplayEffectSpecHandle& SpecHandle) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|Debuff", meta = (Categories = "Debuff"))
	FGameplayTag DebuffTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|Debuff", meta = (ClampMin = "1"))
	int32 MaxStackCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|Debuff", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DebuffStackDuration = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect", meta = (Categories = "Status"))
	FGameplayTag StatusEffectTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect")
	TSubclassOf<UGameplayEffect> StatusEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|Policy",
		meta = (Categories = "Effect.Policy"))
	FGameplayTagContainer RemovalPolicyTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float StatusDuration = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|Damage", meta = (ClampMin = "0.0"))
	float StatusDamageMagnitude = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|UI", meta = (AssetBundles = "Client"))
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!StatusEffect|UI")
	FLinearColor IconBackgroundColor = FLinearColor::White;
};
