#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Common/Enum_Direction.h"
#include "UObject/Object.h"
#include "PandoraSkillRuntimeContext.generated.h"

class UPandoraDefinition;
class USkillDefinition;
struct FSkill;
struct FGameplayEffectQuery;

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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
#if UE_WITH_IRIS
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context,
		UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;
#endif

	bool IsSourceReady() const { return PandoraDefinition && SkillDataAsset && SkillIndex != INDEX_NONE; }
	FGameplayEffectQuery MakeCooldownQuery() const;

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
	UFUNCTION()
	void OnRep_Source();

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UPandoraDefinition> PandoraDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const USkillDefinition> SkillDataAsset;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 SkillIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 PandoraLevel = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	EEnum_Direction LoadoutDirection = EEnum_Direction::Center;
};
