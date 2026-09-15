#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
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
		int32 InPandoraLevel,
		EEnum_Direction InLoadoutDirection = EEnum_Direction::Center);

	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsSourceReady() const { return GetSkillDataAsset() != nullptr; }

	// 이 스킬의 활성 쿨다운 효과에서 남은 시간과 적용 당시의 전체 지속시간을 조회한다.
	void GetCooldownTimeRemainingAndDuration(float& OutRemaining, float& OutDuration) const;

	// ASC의 효과 추가·제거 알림으로 핸들을 설정·해제하며, 판도라 선택이 바뀌어도 유지한다.
	void SetCooldownEffectHandle(FActiveGameplayEffectHandle EffectHandle);
	void ClearCooldownEffectHandle(FActiveGameplayEffectHandle EffectHandle);

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const UPandoraDefinition* GetPandoraDefinition() const { return PandoraDefinition.Get(); }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	const USkillDefinition* GetSkillDataAsset() const;

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	int32 GetSkillIndex() const { return SkillIndex; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	int32 GetPandoraLevel() const { return PandoraLevel; }

	UFUNCTION(BlueprintPure, Category = "!Pandora|Skill")
	EEnum_Direction GetLoadoutDirection() const { return LoadoutDirection; }

private:
	// 서버가 적용한 이 스킬의 쿨다운 효과를 가리킨다. 클라이언트는 복제된 효과의 로컬 핸들을 보관한다.
	// 시간 정보는 ASC의 GameplayEffect에만 보관하며 이 핸들 자체는 복제하지 않는다.
	FActiveGameplayEffectHandle CooldownEffectHandle;

	UFUNCTION()
	void OnRep_Source();

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UPandoraDefinition> PandoraDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 SkillIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	int32 PandoraLevel = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Source, VisibleAnywhere, BlueprintReadOnly, Category = "!Pandora|Skill", meta = (AllowPrivateAccess = "true"))
	EEnum_Direction LoadoutDirection = EEnum_Direction::Center;
};
