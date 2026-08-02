#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Common/Enum_Direction.h"
#include "UObject/Object.h"
#include "PandoraSkillRuntimeContext.generated.h"

class UPandoraDefinition;
class USkillDefinition;
struct FSkill;

UCLASS(BlueprintType)
class LABPROJECT_API UPandoraSkillRuntimeContext : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(
		const UPandoraDefinition* InPandoraDefinition,
		const USkillDefinition* InSkillDataAsset,
		int32 InSkillIndex,
		int32 InPandoraLevel,
		EEnum_Direction InLoadoutDirection = EEnum_Direction::Center);

	virtual bool IsSupportedForNetworking() const override { return true; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const UPandoraDefinition* GetPandoraDefinition() const { return PandoraDefinition.Get(); }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const USkillDefinition* GetSkillDataAsset() const { return SkillDataAsset.Get(); }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	int32 GetSkillIndex() const { return SkillIndex; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	int32 GetPandoraLevel() const { return PandoraLevel; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	EEnum_Direction GetLoadoutDirection() const { return LoadoutDirection; }

	const FSkill* GetPandoraSkill() const;
	TArray<FProjectileImpactEffectAreaSpawnConfig> GetProjectileImpactEffectAreas() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UPandoraDefinition> PandoraDefinition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const USkillDefinition> SkillDataAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 SkillIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 PandoraLevel = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	EEnum_Direction LoadoutDirection = EEnum_Direction::Center;
};
