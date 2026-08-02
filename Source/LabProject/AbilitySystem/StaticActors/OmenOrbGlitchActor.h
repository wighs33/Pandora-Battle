#pragma once

#include "Common/Enum_Direction.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "OmenOrbGlitchActor.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraComponent;
class UCurveFloat;
class USceneComponent;
class FLifetimeProperty;
struct FSkillGameplayEffectConfig;
struct FSkillStaticSettings;

UCLASS(Blueprintable)
class LABPROJECT_API AOmenOrbGlitchActor : public AActor
{
	GENERATED_BODY()

public:
	AOmenOrbGlitchActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!Skill|Darkness|Omen Orb")
	void StartOrbSequence();

	void ConfigureFromStaticSettings(
		const FSkillStaticSettings& StaticSettings,
		const FSkillGameplayEffectConfig& FinishDamageConfig,
		int32 InAbilityLevel,
		EEnum_Direction InSourcePandoraLoadoutDirection = EEnum_Direction::Center);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "!Skill|Darkness|Omen Orb")
	void OnOrbSequenceFinished();

private:
	void ResolveBlueprintComponents();
	void CacheInitialComponentScales();
	void TickGrowth(float DeltaSeconds);
	void TickCollapse(float DeltaSeconds);
	void TickOmenVFX(float DeltaSeconds);
	void FinishGrowth();
	void FinishOmenVFX();
	void RefreshTickEnabledFromSequenceState();
	void ApplyOrbScale(float ScaleAlpha) const;
	void SetOrbAndFloorVisible(bool bVisible) const;
	void ApplyOmenFXValue(float FXValue) const;
	void ActivateFinishNiagara() const;
	void ApplyFinishAreaDamage();
	float CalculateFinishAreaDamageMagnitude(const UAbilitySystemComponent* SourceASC) const;
	void PullEnemyCharacters(float DeltaSeconds, float GrowthAlpha);
	void ClearPullRootMotionSources();
	bool IsPullPhaseActive() const;
	bool ShouldPullCharacter(ACharacterBase* TargetCharacter) const;
	AActor* ResolveSourceActor() const;
	USceneComponent* FindSceneComponentByName(FName ComponentName) const;
	UNiagaraComponent* FindNiagaraComponentByName(FName ComponentName) const;
	float EvaluateCurveOrLinear(const UCurveFloat* Curve, float NormalizedTime) const;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Components")
	FName OrbComponentName = TEXT("Orb");

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Components")
	FName FloorComponentName = TEXT("Floor");

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Components")
	FName OmenOrbNiagaraComponentName = TEXT("NS_Omen_ORB_Glitch");

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Components")
	FName AreaNiagaraComponentName = TEXT("NS_Free_Magic_Area2");

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Components")
	FName SlashNiagaraComponentName = TEXT("NS_Free_Magic_Slash");

	UPROPERTY(Replicated)
	float GrowthDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Growth", meta = (ClampMin = "0.0"))
	float GrowthStartScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Growth", meta = (ClampMin = "0.0"))
	float GrowthEndScaleMultiplier = 7.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Growth")
	TObjectPtr<UCurveFloat> GrowthCurve;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|VFX", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float OmenFXDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|VFX")
	float OmenFXStartValue = 1.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|VFX")
	float OmenFXEndValue = 6.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|VFX")
	TObjectPtr<UCurveFloat> OmenFXCurve;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|VFX")
	FName OmenFXActivateParameterName = TEXT("FX_Activate");

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|VFX")
	bool bActivateFinishNiagara = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|VFX")
	bool bResetFinishNiagaraOnActivate = false;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Finish Damage")
	bool bApplyFinishAreaDamage = false;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Finish Damage", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FinishDamageRadius = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Finish Damage")
	TSubclassOf<UGameplayEffect> FinishDamageEffectClass;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Finish Damage", meta = (Categories = "Data"))
	FGameplayTag FinishDamageDataTag;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Finish Damage", meta = (ClampMin = "0.0"))
	double FinishDamageMagnitude = 0.0;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Collapse")
	bool bCollapseOrbDuringOmenFX = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Collapse", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float CollapseDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Collapse")
	TObjectPtr<UCurveFloat> CollapseCurve;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Pull")
	bool bPullEnemiesDuringGrowth = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Pull", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float PullRadius = 1200.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Pull", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float PullSpeed = 650.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Pull", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float PullStopDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Pull")
	bool bPullOnHorizontalPlane = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Pull")
	bool bScalePullStrengthWithGrowthAlpha = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Pull")
	bool bScalePullRadiusWithGrowthAlpha = false;

	UPROPERTY(EditAnywhere, Category = "!Skill|Darkness|Pull")
	bool bRequireSourceCharacterForTeamFilter = true;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> OrbComponent;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> FloorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> OmenOrbNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> AreaNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SlashNiagaraComponent;

	UPROPERTY(Transient)
	FVector InitialOrbScale = FVector::OneVector;

	UPROPERTY(Transient)
	FVector InitialFloorScale = FVector::OneVector;

	UPROPERTY(Transient)
	bool bInitialComponentScalesCached = false;

	UPROPERTY(Transient)
	int32 ConfiguredAbilityLevel = 1;

	UPROPERTY(Transient)
	EEnum_Direction SourcePandoraLoadoutDirection = EEnum_Direction::Center;

	UPROPERTY(Transient)
	float GrowthElapsed = 0.0f;

	UPROPERTY(Transient)
	float CollapseElapsed = 0.0f;

	UPROPERTY(Transient)
	float OmenFXElapsed = 0.0f;

	UPROPERTY(Transient)
	bool bGrowthActive = false;

	UPROPERTY(Transient)
	bool bCollapseActive = false;

	UPROPERTY(Transient)
	bool bOmenFXActive = false;

	FName PullRootMotionSourceName;
	TArray<TWeakObjectPtr<class UCharacterMovementComponent>> ActivePullMovementComponents;
};
