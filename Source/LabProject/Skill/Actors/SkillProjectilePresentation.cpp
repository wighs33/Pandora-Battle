#include "Skill/Actors/SkillProjectilePresentation.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillProjectilePresentation)

namespace
{
	constexpr float ImpactEffectGroundTraceStartHeight = 150.0f;
	constexpr float ImpactEffectGroundTraceDepth = 5000.0f;

	FTransform ResolveImpactEffectTransform(const AActor& Projectile, const FVector& Location, const bool bOnGround)
	{
		const FTransform DefaultTransform(Projectile.GetActorRotation(), Location);
		UWorld* World = Projectile.GetWorld();
		if (!bOnGround || !World)
		{
			return DefaultTransform;
		}

		const FVector TraceStart = Location + FVector(0.0, 0.0, ImpactEffectGroundTraceStartHeight);
		const FVector TraceEnd = Location - FVector(0.0, 0.0, ImpactEffectGroundTraceDepth);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectileHitNiagaraGroundTrace), false);
		QueryParams.AddIgnoredActor(&Projectile);
		if (AActor* OwningActor = Projectile.GetOwner())
		{
			QueryParams.AddIgnoredActor(OwningActor);
		}
		if (APawn* InstigatorPawn = Projectile.GetInstigator())
		{
			QueryParams.AddIgnoredActor(InstigatorPawn);
		}

		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		FHitResult GroundHit;
		if (World->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, ObjectParams, QueryParams) && GroundHit.bBlockingHit)
		{
			const FRotator GroundRotation = FRotationMatrix::MakeFromZ(GroundHit.Normal.GetSafeNormal()).Rotator();
			return FTransform(GroundRotation, GroundHit.Location);
		}

		return DefaultTransform;
	}
}

float FSkillProjectileGrowth::GetAlpha(const float ServerTimeSeconds) const
{
	if (Duration <= 0.0f)
	{
		return 1.0f;
	}

	const float ElapsedTime = FMath::Max(ServerTimeSeconds - ServerStartTime, 0.0f);
	return FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
}

FVector FSkillProjectileGrowth::GetScale(const float Alpha) const
{
	return FMath::Lerp(StartScale, TargetScale, FMath::Clamp(Alpha, 0.0f, 1.0f));
}

FVector2D FSkillProjectileGrowth::GetNiagaraSize(const float Alpha) const
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	return FVector2D(FMath::Lerp(NiagaraStartSize.X, NiagaraTargetSize.X, ClampedAlpha),
		FMath::Lerp(NiagaraStartSize.Y, NiagaraTargetSize.Y, ClampedAlpha));
}

FName FSkillProjectileGrowth::GetNiagaraParameterName() const
{
	if (NiagaraVector2DParameterName.IsNone())
	{
		return NAME_None;
	}

	const FString RawName = NiagaraVector2DParameterName.ToString();
	return RawName.StartsWith(TEXT("User."))
		? NiagaraVector2DParameterName
		: FName(*FString::Printf(TEXT("User.%s"), *RawName));
}

void PdSkillProjectilePresentation::ApplyLoopEffect(UNiagaraComponent* Effect, UNiagaraSystem* DesiredSystem)
{
	if (!Effect)
	{
		return;
	}

	if (!DesiredSystem)
	{
		Effect->Deactivate();
		return;
	}

	if (Effect->GetAsset() != DesiredSystem)
	{
		Effect->DeactivateImmediate();
		Effect->SetAsset(DesiredSystem);
		Effect->ResetSystem();
	}
	else if (!Effect->IsActive())
	{
		Effect->ResetSystem();
	}

	Effect->Activate(true);
}

void PdSkillProjectilePresentation::ExecuteCue(AActor& Projectile, const FGameplayTag CueTag, const FVector& Location)
{
	if (!CueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = Location;
	Parameters.Instigator = Projectile.GetInstigator();
	Parameters.EffectCauser = &Projectile;
	UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor(&Projectile, CueTag, Parameters);
}

void PdSkillProjectilePresentation::SpawnImpactEffect(AActor& Projectile, UNiagaraSystem* HitEffect,
	const FVector& Location, const bool bOnGround)
{
	if (!HitEffect)
	{
		return;
	}

	const FTransform SpawnTransform = ResolveImpactEffectTransform(Projectile, Location, bOnGround);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(&Projectile, HitEffect, SpawnTransform.GetLocation(),
		SpawnTransform.GetRotation().Rotator());
}

void PdSkillProjectilePresentation::SetNiagaraVector2D(const AActor& Projectile, const FName ParameterName, const FVector2D Value)
{
	if (ParameterName.IsNone())
	{
		return;
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	Projectile.GetComponents(NiagaraComponents);
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (NiagaraComponent)
		{
			NiagaraComponent->SetVariableVec2(ParameterName, Value);
		}
	}
}
