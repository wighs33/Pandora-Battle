#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Definition/AbilitySystem/SkillActivationSettings.h"
#include "Definition/AbilitySystem/SkillGameplayEffectConfig.h"
#include "Definition/AbilitySystem/SkillPresentationSettings.h"
#include "Definition/AbilitySystem/SkillEffectSettings.h"
#include "Definition/AbilitySystem/SkillMovementSettings.h"
#include "SkillDefinition.generated.h"

class USkillAction;
class UStatusEffectDefinition;

/** 여러 기능의 설정과 실행 액션을 소유하는 스킬 데이터 에셋. */
UCLASS(BlueprintType, Blueprintable, meta = (PrioritizeCategories = "!Skill|Properties !Skill|Execution !Skill|UI !Skill|Cost !Skill|Time Damage !Skill|Animation"))
class LABPROJECT_API USkillDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USkillDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// 실행 정책과 기능 조합

	/** 스킬의 종료 정책. 즉시 실행, 입력 유지, 지정 시간 유지 중 하나를 선택한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Properties")
	ESkillType SkillType = ESkillType::Instant;

	/** 피격 시 실행 중인 스킬을 취소할지 여부. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Properties", meta = (DisplayName = "Cancel On Hit"))
	bool bCancelOnHit = false;

	/** 스킬별 입력과 GAS 태그·비용 정책. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Execution", meta = (ShowOnlyInnerProperties))
	FSkillActivationSettings Activation;

	/** SkillAbility가 실행할 액션 트리. 순차·병렬 액션으로 기능을 조합한다. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "!Skill|Execution")
	TObjectPtr<USkillAction> Action;

	// 화면 표시

	/** UI에 표시할 스킬 이름. 비어 있으면 데이터 에셋 이름을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI", AssetRegistrySearchable)
	FName Name;

	/** 스킬 툴팁에 표시할 설명. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI", meta = (MultiLine = "true"))
	FText Description;

	/** 스킬 슬롯에 표시할 텍스처 또는 머티리얼. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI", meta = (AssetBundles = "Client", AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TObjectPtr<UObject> Icon;

	/** 스킬 설명에 화상 효과 아이콘을 표시한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI|Effect Icon", meta = (DisplayName = "Show Burn Effect Icon"))
	bool bShowBurnEffectIcon = false;

	/** 스킬 설명에 동상 효과 아이콘을 표시한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI|Effect Icon", meta = (DisplayName = "Show Frostbite Effect Icon"))
	bool bShowFrostbiteEffectIcon = false;

	/** 스킬 설명에 감전 효과 아이콘을 표시한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI|Effect Icon", meta = (DisplayName = "Show Electric Shock Effect Icon"))
	bool bShowElectricShockEffectIcon = false;

	/** 스킬 설명에 보호막 효과 아이콘을 표시한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|UI|Effect Icon", meta = (DisplayName = "Show Shield Effect Icon"))
	bool bShowShieldEffectIcon = false;

	// 비용과 시간

	/** 스킬 사용 시 소모하는 마나. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Cost", meta = (ClampMin = "0.0", DisplayName = "Mana Cost"))
	double ManaCost = 0.0;

	/** 재사용 대기시간과 스킬 유지 시간. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Time", meta = (ShowOnlyInnerProperties))
	FSkillTimeSettings Time;

	// 공통 피해와 상태 효과

	/** 스킬 적중 피해와 트리거 반복 피해의 공통 설정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage", meta = (ShowOnlyInnerProperties))
	FSkillTopLevelDamageConfig Damage;

	/** 보호막 액션 등 피해 이외의 용도로 적용하는 효과. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect", meta = (ShowOnlyInnerProperties))
	FSkillGameplayEffectConfig GameplayEffect;

	/** 스킬 사용자가 자신에게 적용하는 버프와 능력치 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Self Buff", meta = (ShowOnlyInnerProperties))
	FSkillSelfBuffSettings SelfBuff;

	/** 적중 시 스택을 적용할 상태 효과 정의. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status")
	TObjectPtr<UStatusEffectDefinition> StatusEffectDataAsset;

	/** 상태 효과 GameplayEffect Spec을 생성할 때 사용하는 레벨. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status", meta = (ClampMin = "1.0"))
	float StatusEffectLevel = 1.0f;

	/** 한 번 적중했을 때 적용할 상태 효과 스택 수. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Gameplay Effect|Status",
		meta = (ClampMin = "1", UIMin = "1",
			ToolTip = "Number of status-effect stacks applied by one successful skill hit."))
	int32 StackCount = 1;

	// 공통 연출

	/** 시전 액션들이 공유하는 몽타주와 실행 이벤트. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Animation", meta = (ShowOnlyInnerProperties))
	FSkillAnimationConfig Animation;

	/** 스킬 실행 중 캐릭터에 적용할 오버레이 머티리얼. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Overlay", meta = (ShowOnlyInnerProperties))
	FSkillOverlaySettings Overlay;

	/** 캐릭터 위치에 표시하는 공통 바닥 데칼. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal", meta = (ShowOnlyInnerProperties))
	FSkillDecalSettings CharacterDecal;

	/** 몸체, 지면, 소켓에 재생하는 공통 Niagara 연출. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX", meta = (ShowOnlyInnerProperties))
	FSkillNiagaraSettings Niagara;

	// 공통 이동

	/** 이동 속도 증가, 이동 제한 및 접촉 피해 설정. 대시는 해당 액션에서 설정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Movement", meta = (ShowOnlyInnerProperties))
	FSkillMovementSettings Movement;

	FText GetDisplayName() const;
	UObject* GetIconResource() const;
	FSkillGameplayEffectConfig GetResolvedDamageConfig() const;
};
