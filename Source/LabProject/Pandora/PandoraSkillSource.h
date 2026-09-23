#pragma once

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Common/Enum_Direction.h"
#include "UObject/Object.h"
#include "PandoraSkillSource.generated.h"

class UPandoraDefinition;
class USkillDefinition;

UCLASS(BlueprintType)
class LABPROJECT_API UPandoraSkillSource : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(
		const UPandoraDefinition* InPandoraDefinition,
		int32 InSkillIndex,
		EEnum_Direction InLoadoutDirection = EEnum_Direction::Center);

	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreDestroyFromReplication() override;

	bool IsSourceReady() const { return GetSkillDataAsset() != nullptr; }

	// ASC에서 이 출처의 활성 쿨다운 효과를 조회한다. 시간이나 효과 핸들은 별도로 보관하지 않는다.
	void GetCooldownTimeRemainingAndDuration(float& OutRemaining, float& OutDuration) const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const UPandoraDefinition* GetPandoraDefinition() const { return PandoraDefinition.Get(); }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const USkillDefinition* GetSkillDataAsset() const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	int32 GetSkillIndex() const { return SkillIndex; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	EEnum_Direction GetLoadoutDirection() const { return LoadoutDirection; }

private:
	UFUNCTION()
	void OnRep_Source();

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UPandoraDefinition> PandoraDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 SkillIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	EEnum_Direction LoadoutDirection = EEnum_Direction::Center;
};
