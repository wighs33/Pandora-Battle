#pragma once

#include "CoreMinimal.h"
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
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreDestroyFromReplication() override;

	// Public API ------------------------------------------------------------------------------------------------------
	// 새 Source 생성 시 한 번만 호출한다. 이후 방향 변경은 SetLoadoutDirection으로 처리한다.
	void Initialize(
		const UPandoraDefinition* InPandoraDefinition,
		int32 InSkillIndex,
		EEnum_Direction InLoadoutDirection = EEnum_Direction::Center);

	void SetLoadoutDirection(EEnum_Direction Direction);

	const UPandoraDefinition* GetPandoraDefinition() const { return PandoraDefinition.Get(); }

	const USkillDefinition* GetSkillDataAsset() const;

	int32 GetSkillIndex() const { return SkillIndex; }

	EEnum_Direction GetLoadoutDirection() const { return LoadoutDirection; }

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void OnRep_Source();

private:
	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UPandoraDefinition> PandoraDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 SkillIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	EEnum_Direction LoadoutDirection = EEnum_Direction::Center;
};
