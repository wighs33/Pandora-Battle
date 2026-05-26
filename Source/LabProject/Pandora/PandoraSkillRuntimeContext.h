#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "UObject/Object.h"
#include "PandoraSkillRuntimeContext.generated.h"

class UPandoraDefinition;
class USkillDataAsset;
struct FSkill;

UCLASS(BlueprintType)
class LABPROJECT_API UPandoraSkillRuntimeContext : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(
		const UPandoraDefinition* InPandoraDefinition,
		const USkillDataAsset* InSkillDataAsset,
		int32 InSkillIndex,
		int32 InPandoraLevel);

	virtual bool IsSupportedForNetworking() const override { return true; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const UPandoraDefinition* GetPandoraDefinition() const { return PandoraDefinition.Get(); }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const USkillDataAsset* GetSkillDataAsset() const { return SkillDataAsset.Get(); }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	int32 GetSkillIndex() const { return SkillIndex; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	int32 GetPandoraLevel() const { return PandoraLevel; }

	const FSkill* GetPandoraSkill() const;
	TArray<FProjectileImpactEffectAreaSpawnConfig> GetProjectileImpactEffectAreasForLevel(int32 Level) const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UPandoraDefinition> PandoraDefinition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const USkillDataAsset> SkillDataAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 SkillIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 PandoraLevel = 1;
};
