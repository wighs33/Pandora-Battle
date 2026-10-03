#pragma once
#include "Definition/AbilitySystem/SkillActorFieldSettings.h"

#include "CoreMinimal.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "GameFramework/Actor.h"
#include "SkillPowerUpActor.generated.h"

class ACharacterBase;
class AWeaponBase;
class AMeleeWeapon;
class UNiagaraComponent;
class USkeletalMeshComponent;

UCLASS(Blueprintable)
class LABPROJECT_API ASkillPowerUpActor : public AActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_Owner() override;
	virtual void OnRep_Instigator() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	ASkillPowerUpActor();

	void ConfigurePresentationSettings(const FSkillPowerUpPresentationSettings& InSettings);

	UFUNCTION(BlueprintCallable, Category = "!Skill|Power Up")
	void StartSourcePlayerEffect();

	UFUNCTION(BlueprintCallable, Category = "!Skill|Power Up")
	void StopSourcePlayerEffect();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleSourcePlayerEffectRetry();

	UFUNCTION()
	void OnRep_PresentationSettings();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	ACharacterBase* ResolveSourceCharacter() const;
	UNiagaraComponent* FindNiagaraComponentByName(FName ComponentName) const;
	AWeaponBase* ResolveCurrentWeapon(const ACharacterBase* Character) const;
	void ApplyEffectAlpha(float Alpha);
	void RestoreSourcePlayerState();
	void ScheduleSourcePlayerEffectRetry();

private:
	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Components")
	FName StarterNiagaraComponentName = TEXT("NS_Anime_Aura_Starter");

	UPROPERTY(ReplicatedUsing = OnRep_PresentationSettings)
	FSkillPowerUpPresentationSettings PresentationSettings;

	UPROPERTY(
		EditAnywhere,
		Category = "!Skill|Power Up|Overlay",
		meta = (
			DeprecatedProperty,
			DeprecationMessage = "Skill overlays now always restore the character's team outline when they end."))
	bool bRestoreTeamOverlayOnEnd = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Niagara")
	FName NiagaraActivateParameterName = TEXT("FX_Activate");

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Niagara")
	float NiagaraActivateStartValue = 0.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Niagara")
	float NiagaraActivateEndValue = 1.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Material")
	FName MaterialScalarParameterName = TEXT("Erode");

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Scale")
	bool bScaleSourceCharacter = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Scale", meta = (EditCondition = "bScaleSourceCharacter", ClampMin = "1.0"))
	float CharacterScaleMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Scale", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RampDuration = 0.35f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Weapon Trace")
	bool bScaleWeaponTraceEndZ = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Weapon Trace", meta = (EditCondition = "bScaleWeaponTraceEndZ", ClampMin = "1.0"))
	float WeaponTraceEndZMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Network", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float SourceResolveRetryInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Power Up|Network", meta = (ClampMin = "1"))
	int32 SourceResolveRetryAttempts = 20;

	FTimerHandle SourceResolveRetryTimerHandle;
	int32 SourceResolveRetryCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<ACharacterBase> ActiveSourceCharacter;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> ActiveSourceMesh;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> StarterNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> AttachedNiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<AMeleeWeapon> ActiveTraceWeapon;

	UPROPERTY(Transient)
	FVector CachedSourceMeshScale = FVector::OneVector;

	UPROPERTY(Transient)
	float EffectAlpha = 0.0f;

	UPROPERTY(Transient)
	bool bSourceEffectActive = false;

	UPROPERTY(Transient)
	bool bOverlayApplied = false;

	UPROPERTY(Transient)
	bool bCharacterScaleApplied = false;

	UPROPERTY(Transient)
	bool bTraceEndZApplied = false;
};
