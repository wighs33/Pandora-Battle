#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "CombatSettings.generated.h"

class UAnimMontage;
class UGameplayEffect;
class UNiagaraSystem;

// 무기와 맨손이 함께 사용하는 피해 GE 설정이다.
USTRUCT(BlueprintType)
struct LABPROJECT_API FCombatDamageSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Combat|Damage")
	TSubclassOf<UGameplayEffect> OutgoingDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Combat|Damage")
	TSubclassOf<UGameplayEffect> IncomingDamageEffectClass;
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FUnarmedAttackTraceDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|Trace")
	FName StartSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|Trace")
	FName EndSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|Trace")
	FVector HalfSize = FVector(22.0f);
};

USTRUCT(BlueprintType)
struct LABPROJECT_API FUnarmedCombatSettings
{
	GENERATED_BODY()

	// 이전 자산을 읽기 위한 필드다. PostLoad에서 공통 피해 설정으로 이전한다.
	UPROPERTY(BlueprintReadOnly, Category = "!Combat|Legacy", meta = (DeprecatedProperty))
	TSubclassOf<UGameplayEffect> OutgoingDamageEffectClass;

	UPROPERTY(BlueprintReadOnly, Category = "!Combat|Legacy", meta = (DeprecatedProperty))
	TSubclassOf<UGameplayEffect> IncomingDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|Animation")
	TSoftObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|VFX")
	TObjectPtr<UNiagaraSystem> ComboWindowStartEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|Damage",
		meta = (ClampMin = "0.0"))
	float DamageMagnitude = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|Trace")
	TArray<FUnarmedAttackTraceDefinition> AttackTraces;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|Trace",
		meta = (ClampMin = "0.001", ForceUnits = "s"))
	float TraceInterval = 0.033333f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|Trace",
		meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float TraceInterpolationDistance = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Player|Combat|Unarmed|Trace")
	TArray<TEnumAsByte<EObjectTypeQuery>> TraceObjectTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Combat|Unarmed|Trace", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxTraceInterpolationSteps = 16;

	// 이 거리보다 크게 이동한 프레임은 순간이동으로 보고 중간 경로를 공격하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Combat|Unarmed|Trace", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float MaxTraceTravelDistance = 200.0f;
};
