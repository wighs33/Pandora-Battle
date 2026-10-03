#pragma once

#include "Common/Enum_Direction.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SkillBlackHoleActor.generated.h"

class ACharacterBase;
class UAbilitySystemComponent;
class UGameplayEffect;
class UNiagaraComponent;
class UCurveFloat;
class USceneComponent;
class FLifetimeProperty;
struct FSkillGameplayEffectConfig;
struct FSkillActorFieldSettings;

UCLASS(Blueprintable)
class LABPROJECT_API ASkillBlackHoleActor : public AActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	ASkillBlackHoleActor();

	UFUNCTION(BlueprintCallable, Category = "!Skill|Black Hole")
	void StartBlackHoleSequence();

	void ConfigureFromFieldSettings(
		const FSkillActorFieldSettings& FieldSettings,
		const FSkillGameplayEffectConfig& FinishDamageConfig,
		int32 InAbilityLevel,
		EEnum_Direction InSourcePandoraLoadoutDirection = EEnum_Direction::Center);

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintImplementableEvent, Category = "!Skill|Black Hole")
	void OnOrbSequenceFinished();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ResolveBlueprintComponents();
	void CacheInitialComponentScales();
	void TickGrowth(float DeltaSeconds);
	void TickCollapse(float DeltaSeconds);
	void TickBlackHoleVFX(float DeltaSeconds);
	void FinishGrowth();
	void FinishBlackHoleVFX();
	void RefreshTickEnabledFromSequenceState();
	void ApplyOrbScale(float ScaleAlpha) const;
	void SetOrbAndFloorVisible(bool bVisible) const;
	void ApplyBlackHoleFXValue(float FXValue) const;
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

private:
	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Components")
	FName OrbComponentName = TEXT("Orb");

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Components")
	FName FloorComponentName = TEXT("Floor");

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Components")
	FName BlackHoleNiagaraComponentName = TEXT("NS_Omen_ORB_Glitch");

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Components")
	FName AreaNiagaraComponentName = TEXT("NS_Free_Magic_Area2");

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Components")
	FName SlashNiagaraComponentName = TEXT("NS_Free_Magic_Slash");

	// 성장 시간, 끌어당김, 종료 피해는 스킬 정의가 정한다. ConfigureFromFieldSettings가 생성 직전에 채운다.
	UPROPERTY(Replicated)
	float GrowthDuration = 3.0f;

	UPROPERTY(Transient)
	bool bPullEnemiesDuringGrowth = true;

	UPROPERTY(Transient)
	float PullRadius = 1200.0f;

	UPROPERTY(Transient)
	float PullSpeed = 650.0f;

	UPROPERTY(Transient)
	bool bApplyFinishAreaDamage = false;

	UPROPERTY(Transient)
	float FinishDamageRadius = 1200.0f;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> FinishDamageEffectClass;

	UPROPERTY(Transient)
	FGameplayTag FinishDamageDataTag;

	UPROPERTY(Transient)
	double FinishDamageMagnitude = 0.0;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Growth", meta = (ClampMin = "0.0"))
	float GrowthStartScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Growth", meta = (ClampMin = "0.0"))
	float GrowthEndScaleMultiplier = 7.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Growth")
	TObjectPtr<UCurveFloat> GrowthCurve;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|VFX", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float BlackHoleFXDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|VFX")
	float BlackHoleFXStartValue = 1.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|VFX")
	float BlackHoleFXEndValue = 6.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|VFX")
	TObjectPtr<UCurveFloat> BlackHoleFXCurve;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|VFX")
	FName BlackHoleFXActivateParameterName = TEXT("FX_Activate");

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|VFX")
	bool bActivateFinishNiagara = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|VFX")
	bool bResetFinishNiagaraOnActivate = false;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Collapse")
	bool bCollapseOrbDuringBlackHoleFX = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Collapse", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float CollapseDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Collapse")
	TObjectPtr<UCurveFloat> CollapseCurve;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Pull", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float PullStopDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Pull")
	bool bPullOnHorizontalPlane = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Pull")
	bool bScalePullStrengthWithGrowthAlpha = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Pull")
	bool bScalePullRadiusWithGrowthAlpha = false;

	UPROPERTY(EditAnywhere, Category = "!Skill|Black Hole|Pull")
	bool bRequireSourceCharacterForTeamFilter = true;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> OrbComponent;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> FloorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BlackHoleNiagaraComponent;

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
	float BlackHoleFXElapsed = 0.0f;

	UPROPERTY(Transient)
	bool bGrowthActive = false;

	UPROPERTY(Transient)
	bool bCollapseActive = false;

	UPROPERTY(Transient)
	bool bBlackHoleFXActive = false;

	FName PullRootMotionSourceName;
	TArray<TWeakObjectPtr<class UCharacterMovementComponent>> ActivePullMovementComponents;
};
