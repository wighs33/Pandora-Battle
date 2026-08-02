#include "AbilitySystem/StaticActors/AnimeAuraActor.h"

#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Component/Player/EquipmentComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimeAuraActor)

AAnimeAuraActor::AAnimeAuraActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bNetUseOwnerRelevancy = true;

	static ConstructorHelpers::FObjectFinder<UAnimMontage> DefaultPowerUpMontage(
		TEXT("/Game/Anime_Aura/Animations/AM_PowerUp.AM_PowerUp"));
	if (DefaultPowerUpMontage.Succeeded())
	{
		PowerUpMontage = DefaultPowerUpMontage.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultOverlayMaterial(
		TEXT("/Game/Anime_Aura/Materials/M_Black_Body.M_Black_Body"));
	if (DefaultOverlayMaterial.Succeeded())
	{
		OverlayMaterial = DefaultOverlayMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultAttachedNiagaraSystem(
		TEXT("/Game/Anime_Aura/Niagara/NS_Anime_Aura.NS_Anime_Aura"));
	if (DefaultAttachedNiagaraSystem.Succeeded())
	{
		AttachedNiagaraSystem = DefaultAttachedNiagaraSystem.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> DefaultMaterialParameterCollection(
		TEXT("/Game/Anime_Aura/Materials/MPC_Ice_Body.MPC_Ice_Body"));
	if (DefaultMaterialParameterCollection.Succeeded())
	{
		MaterialParameterCollection = DefaultMaterialParameterCollection.Object;
	}
}

void AAnimeAuraActor::BeginPlay()
{
	Super::BeginPlay();

	StartSourcePlayerEffect();
}

void AAnimeAuraActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bSourceEffectActive || EffectAlpha >= 1.0f)
	{
		SetActorTickEnabled(false);
		return;
	}

	if (RampDuration <= KINDA_SMALL_NUMBER)
	{
		EffectAlpha = 1.0f;
	}
	else
	{
		EffectAlpha = FMath::Clamp(EffectAlpha + (DeltaSeconds / RampDuration), 0.0f, 1.0f);
	}

	ApplyEffectAlpha(EffectAlpha);
	if (EffectAlpha >= 1.0f)
	{
		SetActorTickEnabled(false);
	}
}

void AAnimeAuraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SourceResolveRetryTimerHandle);
	StopSourcePlayerEffect();

	Super::EndPlay(EndPlayReason);
}

void AAnimeAuraActor::OnRep_Owner()
{
	Super::OnRep_Owner();
	if (HasActorBegunPlay())
	{
		StartSourcePlayerEffect();
	}
}

void AAnimeAuraActor::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	if (HasActorBegunPlay())
	{
		StartSourcePlayerEffect();
	}
}

void AAnimeAuraActor::StartSourcePlayerEffect()
{
	if (bSourceEffectActive)
	{
		return;
	}

	ACharacterBase* SourceCharacter = ResolveSourceCharacter();
	USkeletalMeshComponent* SourceMesh = SourceCharacter ? SourceCharacter->GetMesh() : nullptr;
	if (!SourceCharacter || !SourceMesh)
	{
		ScheduleSourcePlayerEffectRetry();
		return;
	}

	GetWorldTimerManager().ClearTimer(SourceResolveRetryTimerHandle);
	SourceResolveRetryTimerHandle.Invalidate();
	SourceResolveRetryCount = 0;
	ActiveSourceCharacter = SourceCharacter;
	ActiveSourceMesh = SourceMesh;
	CachedSourceMeshScale = SourceMesh->GetComponentScale();
	StarterNiagaraComponent = FindNiagaraComponentByName(StarterNiagaraComponentName);

	UAnimMontage* MontageToPlay = ResolvePowerUpMontage();
	UMaterialInterface* OverlayToApply = ResolveOverlayMaterial();
	UNiagaraSystem* AttachedSystemToSpawn = ResolveAttachedNiagaraSystem();

	if (MontageToPlay)
	{
		SourceCharacter->PlayAnimMontage(MontageToPlay);
	}

	if (OverlayToApply)
	{
		SourceCharacter->ApplySkillPresentationOverlay(this, OverlayToApply);
		bOverlayApplied = true;
	}

	if (StarterNiagaraComponent)
	{
		StarterNiagaraComponent->Activate(false);
	}

	if (AttachedSystemToSpawn)
	{
		AttachedNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			AttachedSystemToSpawn,
			SourceMesh,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false,
			true);
	}

	if (bScaleWeaponTraceEndZ && WeaponTraceEndZMultiplier > 1.0f)
	{
		if (AWeaponBase* CurrentWeapon = ResolveCurrentWeapon(SourceCharacter))
		{
			CurrentWeapon->SetTemporaryAttackTraceEndZMultiplier(this, WeaponTraceEndZMultiplier);
			ActiveTraceWeapon = CurrentWeapon;
			bTraceEndZApplied = true;
		}
	}

	bSourceEffectActive = true;
	EffectAlpha = RampDuration <= KINDA_SMALL_NUMBER ? 1.0f : 0.0f;
	ApplyEffectAlpha(EffectAlpha);
	SetActorTickEnabled(EffectAlpha < 1.0f);

}

void AAnimeAuraActor::StopSourcePlayerEffect()
{
	SetActorTickEnabled(false);

	if (!bSourceEffectActive
		&& !bCharacterScaleApplied
		&& !bTraceEndZApplied
		&& !bOverlayApplied)
	{
		return;
	}

	ApplyEffectAlpha(0.0f);

	if (StarterNiagaraComponent)
	{
		StarterNiagaraComponent->Deactivate();
	}

	if (AttachedNiagaraComponent)
	{
		AttachedNiagaraComponent->Deactivate();
		AttachedNiagaraComponent->DestroyComponent();
		AttachedNiagaraComponent = nullptr;
	}

	RestoreSourcePlayerState();



	ActiveSourceCharacter = nullptr;
	ActiveSourceMesh = nullptr;
	StarterNiagaraComponent = nullptr;
	bSourceEffectActive = false;
	EffectAlpha = 0.0f;
}

void AAnimeAuraActor::ScheduleSourcePlayerEffectRetry()
{
	if (bSourceEffectActive
		|| SourceResolveRetryTimerHandle.IsValid()
		|| SourceResolveRetryCount >= FMath::Max(SourceResolveRetryAttempts, 1))
	{
		return;
	}

	++SourceResolveRetryCount;
	GetWorldTimerManager().SetTimer(
		SourceResolveRetryTimerHandle,
		this,
		&ThisClass::HandleSourcePlayerEffectRetry,
		FMath::Max(SourceResolveRetryInterval, 0.01f),
		false);
}

void AAnimeAuraActor::HandleSourcePlayerEffectRetry()
{
	SourceResolveRetryTimerHandle.Invalidate();
	StartSourcePlayerEffect();
}

ACharacterBase* AAnimeAuraActor::ResolveSourceCharacter() const
{
	if (ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOwner()))
	{
		return OwnerCharacter;
	}

	if (ACharacterBase* InstigatorCharacter = Cast<ACharacterBase>(GetInstigator()))
	{
		return InstigatorCharacter;
	}

	return nullptr;
}

UNiagaraComponent* AAnimeAuraActor::FindNiagaraComponentByName(const FName ComponentName) const
{
	if (ComponentName.IsNone())
	{
		return nullptr;
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents<UNiagaraComponent>(NiagaraComponents);
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (!NiagaraComponent)
		{
			continue;
		}

		if (NiagaraComponent->GetFName() == ComponentName || NiagaraComponent->ComponentHasTag(ComponentName))
		{
			return NiagaraComponent;
		}
	}

	return nullptr;
}

AWeaponBase* AAnimeAuraActor::ResolveCurrentWeapon(const ACharacterBase* Character) const
{
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

UAnimMontage* AAnimeAuraActor::ResolvePowerUpMontage() const
{
	return PowerUpMontage;
}

UMaterialInterface* AAnimeAuraActor::ResolveOverlayMaterial() const
{
	return OverlayMaterial;
}

UNiagaraSystem* AAnimeAuraActor::ResolveAttachedNiagaraSystem() const
{
	return AttachedNiagaraSystem;
}

UMaterialParameterCollection* AAnimeAuraActor::ResolveMaterialParameterCollection() const
{
	return MaterialParameterCollection;
}

void AAnimeAuraActor::ApplyEffectAlpha(const float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const float NiagaraValue = FMath::Lerp(NiagaraActivateStartValue, NiagaraActivateEndValue, ClampedAlpha);

	if (!NiagaraActivateParameterName.IsNone())
	{
		if (StarterNiagaraComponent)
		{
			StarterNiagaraComponent->SetVariableFloat(NiagaraActivateParameterName, NiagaraValue);
		}

		if (AttachedNiagaraComponent)
		{
			AttachedNiagaraComponent->SetVariableFloat(NiagaraActivateParameterName, NiagaraValue);
		}
	}

	if (UMaterialParameterCollection* ParameterCollection = ResolveMaterialParameterCollection();
		ParameterCollection && !MaterialScalarParameterName.IsNone())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(
			this,
			ParameterCollection,
			MaterialScalarParameterName,
			ClampedAlpha);
	}

	if (bScaleSourceCharacter && CharacterScaleMultiplier > 1.0f && ActiveSourceMesh)
	{
		const float CurrentScaleMultiplier = FMath::Lerp(1.0f, CharacterScaleMultiplier, ClampedAlpha);
		ActiveSourceMesh->SetWorldScale3D(CachedSourceMeshScale * CurrentScaleMultiplier);
		bCharacterScaleApplied = true;
	}
}

void AAnimeAuraActor::RestoreSourcePlayerState()
{
	if (bCharacterScaleApplied && ActiveSourceMesh)
	{
		ActiveSourceMesh->SetWorldScale3D(CachedSourceMeshScale);
		bCharacterScaleApplied = false;
	}

	if (bTraceEndZApplied && ActiveTraceWeapon)
	{
		ActiveTraceWeapon->ClearTemporaryAttackTraceEndZMultiplier(this);
	}
	ActiveTraceWeapon = nullptr;
	bTraceEndZApplied = false;

	if (bOverlayApplied)
	{
		if (ActiveSourceCharacter)
		{
			ActiveSourceCharacter->ClearSkillPresentationOverlay(this);
		}
		bOverlayApplied = false;
	}

	CachedSourceMeshScale = FVector::OneVector;
}
