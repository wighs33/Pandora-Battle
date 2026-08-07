#include "AbilitySystem/Cues/AOEIndicatorGameplayCue.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Common/LabGameplayTags.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AOEIndicatorGameplayCue)

namespace
{
	float DecodeCueSeconds(const int32 EncodedMilliseconds)
	{
		return EncodedMilliseconds > 1
			? static_cast<float>(EncodedMilliseconds) / 1000.0f
			: 0.0f;
	}

	float ResolveCueGrowthDuration(const FGameplayCueParameters& Parameters, const bool bTrackAsPersistent)
	{
		return bTrackAsPersistent
			? DecodeCueSeconds(Parameters.GameplayEffectLevel)
			: DecodeCueSeconds(Parameters.AbilityLevel);
	}

	float ResolveCueLifeSpan(const FGameplayCueParameters& Parameters, const bool bTrackAsPersistent)
	{
		return bTrackAsPersistent
			? 0.0f
			: DecodeCueSeconds(Parameters.GameplayEffectLevel);
	}

	float ResolveFloorOnlyDecalDepth(const float ConfiguredDepth)
	{
		return FMath::Clamp(ConfiguredDepth, 1.0f, 8.0f);
	}
}

AAOEIndicatorGameplayCue::AAOEIndicatorGameplayCue()
{
	GameplayCueTag = LabGameplayTags::GameplayCue_AOEIndicator;
	bAutoDestroyOnRemove = true;
}

bool AAOEIndicatorGameplayCue::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return EventType == EGameplayCueEvent::Executed
		|| EventType == EGameplayCueEvent::OnActive
		|| EventType == EGameplayCueEvent::Removed;
}

bool AAOEIndicatorGameplayCue::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return SpawnDecalFromParameters(MyTarget, Parameters, false) != nullptr;
}

bool AAOEIndicatorGameplayCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return SpawnDecalFromParameters(MyTarget, Parameters, true) != nullptr;
}

bool AAOEIndicatorGameplayCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(MyTarget);
	static_cast<void>(Parameters);

	DestroySpawnedDecal();

	return true;
}

void AAOEIndicatorGameplayCue::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroySpawnedDecal();
	Super::EndPlay(EndPlayReason);
}

UDecalComponent* AAOEIndicatorGameplayCue::SpawnDecalFromParameters(
	AActor* MyTarget,
	const FGameplayCueParameters& Parameters,
	const bool bTrackAsPersistent)
{
	UMaterialInterface* ResolvedDecalMaterial = ResolveDecalMaterial(Parameters);

	if (!ResolvedDecalMaterial)
	{

		return nullptr;
	}

	if (bTrackAsPersistent)
	{
		DestroySpawnedDecal();
	}

	const float StartDiameter = Parameters.RawMagnitude > 0.0f ? Parameters.RawMagnitude : FallbackDecalDiameter;
	const float TargetDiameter = Parameters.NormalizedMagnitude > 0.0f ? Parameters.NormalizedMagnitude : StartDiameter;
	const float GrowthDuration = ResolveCueGrowthDuration(Parameters, bTrackAsPersistent);
	const float LifeSpan = ResolveCueLifeSpan(Parameters, bTrackAsPersistent);
	const float EffectiveDecalDepth = ResolveFloorOnlyDecalDepth(DecalDepth);
	UDecalComponent* NewDecalComponent = UGameplayStatics::SpawnDecalAtLocation(
		this,
		ResolvedDecalMaterial,
		FVector(EffectiveDecalDepth, StartDiameter, StartDiameter),
		Parameters.Location,
		FRotator(-90.0f, 0.0f, 0.0f),
		LifeSpan);

	if (!NewDecalComponent)
	{

		return nullptr;
	}

	if (bTrackAsPersistent)
	{
		SpawnedDecalComponent = NewDecalComponent;
	}

	const bool bLocalInstigator = UAbilitySystemBlueprintLibrary::IsInstigatorLocallyControlledPlayer(Parameters);
	if (bLocalInstigator && bOverrideLocalDecalColor)
	{
		NewDecalComponent->SetDecalColor(LocalDecalColor);
	}
	else if (!bLocalInstigator)
	{
		NewDecalComponent->SetDecalColor(NonLocalDecalColor);
	}

	ApplyDecalGrowth(NewDecalComponent, StartDiameter, TargetDiameter, GrowthDuration);

	return NewDecalComponent;
}

void AAOEIndicatorGameplayCue::ApplyDecalGrowth(
	UDecalComponent* DecalComponent,
	const float StartDiameter,
	const float TargetDiameter,
	const float GrowthDuration)
{
	if (!IsValid(DecalComponent) || GrowthDuration <= UE_SMALL_NUMBER || FMath::IsNearlyEqual(StartDiameter, TargetDiameter))
	{
		return;
	}

	TWeakObjectPtr<UDecalComponent> WeakDecal = DecalComponent;
	const double GrowthStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	TSharedRef<FTimerHandle> GrowthTimerHandle = MakeShared<FTimerHandle>();
	GetWorldTimerManager().SetTimer(
		*GrowthTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakDecal, StartDiameter, TargetDiameter, GrowthDuration, GrowthStartTime, GrowthTimerHandle]()
		{
			UWorld* TimerWorld = GetWorld();
			UDecalComponent* ActiveDecalComponent = WeakDecal.Get();
			if (!TimerWorld || !ActiveDecalComponent)
			{
				if (TimerWorld)
				{
					TimerWorld->GetTimerManager().ClearTimer(*GrowthTimerHandle);
				}
				return;
			}

			const double Elapsed = TimerWorld->GetTimeSeconds() - GrowthStartTime;
			const float Alpha = FMath::Clamp(static_cast<float>(Elapsed) / GrowthDuration, 0.0f, 1.0f);
			const float CurrentDiameter = FMath::Lerp(StartDiameter, TargetDiameter, Alpha);
			ActiveDecalComponent->DecalSize = FVector(ResolveFloorOnlyDecalDepth(DecalDepth), CurrentDiameter, CurrentDiameter);
			ActiveDecalComponent->MarkRenderStateDirty();

			if (Alpha >= 1.0f)
			{
				TimerWorld->GetTimerManager().ClearTimer(*GrowthTimerHandle);
			}
		}),
		0.02f,
		true);
}

UMaterialInterface* AAOEIndicatorGameplayCue::ResolveDecalMaterial(const FGameplayCueParameters& Parameters) const
{
	const UObject* SourceObject = Parameters.SourceObject.Get();
	if (const UMaterialInterface* SourceMaterial = Cast<UMaterialInterface>(SourceObject))
	{
		return const_cast<UMaterialInterface*>(SourceMaterial);
	}

	return DecalMaterial.Get();
}

void AAOEIndicatorGameplayCue::DestroySpawnedDecal()
{
	if (!IsValid(SpawnedDecalComponent))
	{
		SpawnedDecalComponent = nullptr;
		return;
	}

	SpawnedDecalComponent->DestroyComponent();
	SpawnedDecalComponent = nullptr;
}
