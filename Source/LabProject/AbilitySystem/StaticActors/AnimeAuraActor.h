#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnimeAuraActor.generated.h"

class ACharacterBase;
class AWeaponBase;
class UAnimMontage;
class UMaterialInterface;
class UMaterialParameterCollection;
class UNiagaraComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;

UCLASS(Blueprintable)
class LABPROJECT_API AAnimeAuraActor : public AActor
{
	GENERATED_BODY()

public:
	AAnimeAuraActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRep_Owner() override;
	virtual void OnRep_Instigator() override;

	UFUNCTION(BlueprintCallable, Category = "!Skill|Anime Aura")
	void StartSourcePlayerEffect();

	UFUNCTION(BlueprintCallable, Category = "!Skill|Anime Aura")
	void StopSourcePlayerEffect();

private:
	ACharacterBase* ResolveSourceCharacter() const;
	UNiagaraComponent* FindNiagaraComponentByName(FName ComponentName) const;
	AWeaponBase* ResolveCurrentWeapon(const ACharacterBase* Character) const;
	UAnimMontage* ResolvePowerUpMontage() const;
	UMaterialInterface* ResolveOverlayMaterial() const;
	UNiagaraSystem* ResolveAttachedNiagaraSystem() const;
	UMaterialParameterCollection* ResolveMaterialParameterCollection() const;
	void ApplyEffectAlpha(float Alpha);
	void RestoreSourcePlayerState();
	void ScheduleSourcePlayerEffectRetry();
	void HandleSourcePlayerEffectRetry();

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Components")
	FName StarterNiagaraComponentName = TEXT("NS_Anime_Aura_Starter");

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Animation")
	TObjectPtr<UAnimMontage> PowerUpMontage;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Overlay")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	UPROPERTY(
		EditAnywhere,
		Category = "!Skill|Anime Aura|Overlay",
		meta = (
			DeprecatedProperty,
			DeprecationMessage = "Skill overlays now always restore the character's team outline when they end."))
	bool bRestoreTeamOverlayOnEnd = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Niagara")
	TObjectPtr<UNiagaraSystem> AttachedNiagaraSystem;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Niagara")
	FName NiagaraActivateParameterName = TEXT("FX_Activate");

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Niagara")
	float NiagaraActivateStartValue = 0.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Niagara")
	float NiagaraActivateEndValue = 1.0f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Material")
	TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Material")
	FName MaterialScalarParameterName = TEXT("Erode");

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Scale")
	bool bScaleSourceCharacter = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Scale", meta = (EditCondition = "bScaleSourceCharacter", ClampMin = "1.0"))
	float CharacterScaleMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Scale", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RampDuration = 0.35f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Weapon Trace")
	bool bScaleWeaponTraceEndZ = true;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Weapon Trace", meta = (EditCondition = "bScaleWeaponTraceEndZ", ClampMin = "1.0"))
	float WeaponTraceEndZMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Network", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float SourceResolveRetryInterval = 0.1f;

	UPROPERTY(EditAnywhere, Category = "!Skill|Anime Aura|Network", meta = (ClampMin = "1"))
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
	TObjectPtr<AWeaponBase> ActiveTraceWeapon;

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
