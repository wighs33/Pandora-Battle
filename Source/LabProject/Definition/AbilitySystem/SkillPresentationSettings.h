#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SkillPresentationSettings.generated.h"

class UAnimMontage;
class UMaterialInterface;
class UNiagaraSystem;

/** 시전 액션들이 공유하는 몽타주와 이벤트 설정. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillAnimationConfig
{
	GENERATED_BODY()

	/** 기본 실행 몽타주. 재생 시점과 완료 처리는 각 Ability가 결정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Animation")
	TObjectPtr<UAnimMontage> PrimaryMontage;

	/** 광역 공격 Ability에서 공격 실행 시 재생하는 두 번째 몽타주. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Animation")
	TObjectPtr<UAnimMontage> SecondaryMontage;

	/** 몽타주에서 실제 스킬 실행 시점을 알리는 GameplayEvent 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Animation", meta = (Categories = "Event"))
	FGameplayTag PrimaryEventTag;
};

/** 캐릭터 메시의 오버레이 연출. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillOverlaySettings
{
	GENERATED_BODY()

	/** 캐릭터에 오버레이 머티리얼을 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Overlay")
	bool bUseCharacterOverlay = false;

	/** 캐릭터 메시 위에 표시할 오버레이 머티리얼. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Overlay", meta = (EditCondition = "bUseCharacterOverlay"))
	TObjectPtr<UMaterialInterface> CharacterOverlayMaterial;
};

/** 캐릭터 위치의 데칼 연출. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillDecalSettings
{
	GENERATED_BODY()

	/** 캐릭터 위치 데칼의 머티리얼. 지정된 경우 데칼을 생성한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal")
	TObjectPtr<UMaterialInterface> DecalMaterial;

	/** 캐릭터 위치 데칼의 시작 크기(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	double DecalSize = 512.0;

	/** 스킬 유지 시간에 맞춰 데칼을 최종 크기까지 확대한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal")
	bool bGrowDecalSize = false;

	/** 확대가 끝난 데칼의 크기(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Decal", meta = (EditCondition = "bGrowDecalSize", ClampMin = "0.0", ForceUnits = "cm"))
	double FinalDecalSize = 0.0;
};

/** 공통 Niagara 연출. 피해나 GameplayEffect 설정은 포함하지 않는다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillNiagaraSettings
{
	GENERATED_BODY()

	/** 공통 연출을 재생하는 GameplayCue 태그. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|GameplayCue", meta = (Categories = "GameplayCue"))
	FGameplayTag GameplayCueTag;

	/** 캐릭터 몸체에 부착할 Niagara 시스템. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Aura")
	TObjectPtr<UNiagaraSystem> AuraNiagaraSystem;

	/** 몸체 연출에 재사용할 Niagara 컴포넌트의 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Aura")
	FName AuraNiagaraComponentName = TEXT("AuraNiagara");

	/** 몸체 Niagara의 상대 위치 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Aura")
	FVector AuraLocationOffset = FVector::ZeroVector;

	/** 몸체 Niagara의 크기 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Aura")
	FVector AuraScale = FVector::OneVector;

	/** 지면에 표시할 Niagara 시스템. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground")
	TObjectPtr<UNiagaraSystem> GroundNiagaraSystem;

	/** 지면 연출이 캐릭터의 지정 소켓을 따라가도록 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground", meta = (DisplayName = "Follow Character"))
	bool bGroundNiagaraFollowsCharacter = false;

	/** 지면 연출이 따라갈 캐릭터 소켓 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground", meta = (EditCondition = "bGroundNiagaraFollowsCharacter", DisplayName = "Follow Socket Name"))
	FName GroundFollowSocketName = TEXT("root");

	/** 지면 Niagara의 위치 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground")
	FVector GroundLocationOffset = FVector::ZeroVector;

	/** 지면 Niagara의 회전 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground")
	FRotator GroundRotationOffset = FRotator::ZeroRotator;

	/** 지면 Niagara의 크기 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Ground")
	FVector GroundScale = FVector::OneVector;

	/** 캐릭터 소켓에 부착할 Niagara. 미사일 Ability에서는 미사일 연출로 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	TObjectPtr<UNiagaraSystem> SocketNiagaraSystem;

	/** 소켓 연출에 재사용할 Niagara 컴포넌트의 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FName SocketNiagaraComponentName = NAME_None;

	/** Niagara를 생성하거나 부착할 캐릭터 소켓 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FName SocketName = NAME_None;

	/** 소켓 Niagara의 위치 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FVector SocketLocationOffset = FVector::ZeroVector;

	/** 소켓 Niagara의 회전 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FRotator SocketRotationOffset = FRotator::ZeroRotator;

	/** 소켓 Niagara의 크기 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Default FX|Socket")
	FVector SocketScale = FVector::OneVector;

};

/** 무기 궤적, 검기 연출과 타격 판정. 애니메이션은 공통 Animation을 사용한다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillSwordTrailSettings
{
	GENERATED_BODY()

	/** 검기 타격 트레이스 끝점의 Z 길이에 적용할 배율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail", meta = (ClampMin = "1.0"))
	double TrailEndZLengthMultiplier = 1.0;

	/** 무기의 움직임을 따라 표시할 궤적 Niagara. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail")
	TObjectPtr<UNiagaraSystem> TrailNiagaraSystem;

	/** 검기 생성 AnimNotify에서 재생할 Niagara. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail")
	TObjectPtr<UNiagaraSystem> SlashNiagaraSystem;

	/** 검기 생성 위치, 회전 및 크기 보정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail")
	FTransform SlashTransformOffset = FTransform::Identity;

	/** 검기를 생성할 소켓 이름. 비어 있으면 기본 생성 위치를 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail", meta = (DisplayName = "Slash Spawn Socket Name"))
	FName SlashSpawnSocketName = NAME_None;

	/** 검기 공격 구간에 무기 타격 트레이스를 활성화한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Sword Trail")
	bool bEnableSlashHitTrace = true;
};
