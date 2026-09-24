#pragma once

#include "CoreMinimal.h"
#include "Common/WeaponDefinitionData.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "SkillAreaSettings.generated.h"

class AGameplayAbilityTargetActor;
class UMaterialInterface;

/** 광역 공격의 조준, 카메라, 피해 반경과 타격 연출. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FSkillAreaSettings
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	FSkillAreaSettings();

public:
	/** 조준 위치와 표적 검증에 사용하는 최대 거리(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Targeting Max Range"))
	double TargetingMaxRange = 0.0;

	/** 플레이어의 공격 위치 선택을 처리하는 GAS TargetActor 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Target Actor Class"))
	TSubclassOf<AGameplayAbilityTargetActor> TargetActorClass;

	/** 광역 공격 조준 위치에 표시할 데칼 머티리얼. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Decal"))
	TObjectPtr<UMaterialInterface> TargetingDecal;

	/** 광역 공격 조준 데칼에 전달할 색상. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Decal Color"))
	FLinearColor TargetingDecalColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	/** 조준 트레이스에 사용할 충돌 프로필 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Trace Profile Name"))
	FName TargetingTraceProfileName = TEXT("BlockAll");

	/** 지면 TargetActor의 충돌 검사 반경(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Targeting Collision Radius"))
	double TargetingCollisionRadius = 0.0;

	/** 지면 TargetActor의 충돌 검사 높이(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Targeting Collision Height"))
	double TargetingCollisionHeight = 0.0;

	/** 조준 트레이스 결과를 발사 방향의 상하 각도에 반영한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Trace Affects Aim Pitch"))
	bool bTargetingTraceAffectsAimPitch = true;

	/** 개발용 스킬 디버그 CVar가 켜진 경우 조준 정보를 그린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Debug Targeting"))
	bool bDebugTargeting = false;

	/** 광역 공격 조준의 시작 위치로 사용할 소켓. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Targeting", meta = (DisplayName = "Targeting Socket Name"))
	FName TargetingSocketName = NAME_None;

	/** 광역 공격 조준 중 전용 카메라 설정을 적용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Camera", meta = (DisplayName = "Use Camera Settings"))
	bool bUseCameraSettings = false;

	/** 광역 공격 조준 카메라의 시야각, 위치, 회전 및 보간 속도. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Camera", meta = (EditCondition = "bUseCameraSettings", EditConditionHides, DisplayName = "Camera Settings"))
	FWeaponAimCameraSettings CameraSettings;

	/** AI 광역 공격의 목표 위치를 지면에 투영할 트레이스 채널. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|AI Targeting", meta = (DisplayName = "Target Ground Trace Channel"))
	TEnumAsByte<ETraceTypeQuery> TargetGroundTraceChannel;

	/** 충돌 연출의 지면 검사 깊이(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|AI Targeting", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Target Ground Trace Depth"))
	double TargetGroundTraceDepth = 10000.0;

	/** 광역 공격의 피해 반경(cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Radius", meta = (ClampMin = "0.0", ForceUnits = "cm", DisplayName = "Radius"))
	double Radius = 0.0;

	/** 개발용 스킬 디버그 CVar가 켜진 경우 피해 반경을 그린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Debug", meta = (DisplayName = "Draw Debug Damage Radius"))
	bool bDrawDebugDamageRadius = false;

	/** 광역 공격 디버그 반경을 표시할 시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Debug", meta = (EditCondition = "bDrawDebugDamageRadius", EditConditionHides, ClampMin = "0.0", ForceUnits = "s", DisplayName = "Debug Damage Radius Draw Time"))
	double DebugDamageRadiusDrawTime = 5.0;

	/** 광역 공격의 범위 예고를 표시하는 GameplayCue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|GameplayCue", meta = (Categories = "GameplayCue", DisplayName = "Indicator Cue Tag"))
	FGameplayTag IndicatorCueTag;

	/** 광역 공격의 타격 연출을 재생하는 GameplayCue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|GameplayCue", meta = (Categories = "GameplayCue", DisplayName = "Impact Cue Tag"))
	FGameplayTag ImpactCueTag;

	/** 광역 공격 실행 후 실제 피해를 적용하기까지의 대기시간(초). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skill|Area|Timing", meta = (ClampMin = "0.0", ForceUnits = "s", DisplayName = "Damage Delay"))
	double DamageDelay = 0.2;
};
