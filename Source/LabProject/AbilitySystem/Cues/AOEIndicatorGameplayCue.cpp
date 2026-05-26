#include "AbilitySystem/Cues/AOEIndicatorGameplayCue.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Common/LabGameplayTags.h"
#include "Components/DecalComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AOEIndicatorGameplayCue)

DEFINE_LOG_CATEGORY_STATIC(LogAOEIndicatorGameplayCue, Log, All);

AAOEIndicatorGameplayCue::AAOEIndicatorGameplayCue()
{
	GameplayCueTag = LabGameplayTags::GameplayCue_AOEIndicator;
	bAutoDestroyOnRemove = true;
}

bool AAOEIndicatorGameplayCue::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return EventType == EGameplayCueEvent::OnActive || EventType == EGameplayCueEvent::Removed;
}

bool AAOEIndicatorGameplayCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(MyTarget);

	if (!DecalMaterial)
	{
		UE_LOG(LogAOEIndicatorGameplayCue, Warning,
			TEXT("AOEIndicator OnActive skipped: DecalMaterial is null. target=%s location=%s rawMagnitude=%.1f"),
			*GetNameSafe(MyTarget),
			*Parameters.Location.ToCompactString(),
			Parameters.RawMagnitude);
		return false;
	}

	DestroySpawnedDecal();

	const float DecalDiameter = Parameters.RawMagnitude > 0.0f ? Parameters.RawMagnitude : FallbackDecalDiameter;
	SpawnedDecalComponent = UGameplayStatics::SpawnDecalAtLocation(
		this,
		DecalMaterial,
		FVector(DecalDepth, DecalDiameter, DecalDiameter),
		Parameters.Location,
		FRotator(-90.0f, 0.0f, 0.0f),
		0.0f);

	if (!SpawnedDecalComponent)
	{
		UE_LOG(LogAOEIndicatorGameplayCue, Warning,
			TEXT("AOEIndicator OnActive failed: spawn decal returned null. target=%s material=%s location=%s diameter=%.1f"),
			*GetNameSafe(MyTarget),
			*GetNameSafe(DecalMaterial.Get()),
			*Parameters.Location.ToCompactString(),
			DecalDiameter);
		return false;
	}

	const bool bLocalInstigator = UAbilitySystemBlueprintLibrary::IsInstigatorLocallyControlledPlayer(Parameters);
	if (bLocalInstigator && bOverrideLocalDecalColor)
	{
		SpawnedDecalComponent->SetDecalColor(LocalDecalColor);
	}
	else if (!bLocalInstigator)
	{
		SpawnedDecalComponent->SetDecalColor(NonLocalDecalColor);
	}

	UE_LOG(LogAOEIndicatorGameplayCue, Log,
		TEXT("AOEIndicator OnActive: target=%s material=%s location=%s diameter=%.1f localInstigator=%s"),
		*GetNameSafe(MyTarget),
		*GetNameSafe(DecalMaterial.Get()),
		*Parameters.Location.ToCompactString(),
		DecalDiameter,
		bLocalInstigator ? TEXT("true") : TEXT("false"));

	return true;
}

bool AAOEIndicatorGameplayCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(MyTarget);
	static_cast<void>(Parameters);

	DestroySpawnedDecal();
	UE_LOG(LogAOEIndicatorGameplayCue, Log,
		TEXT("AOEIndicator OnRemove: target=%s location=%s"),
		*GetNameSafe(MyTarget),
		*Parameters.Location.ToCompactString());
	return true;
}

void AAOEIndicatorGameplayCue::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroySpawnedDecal();
	Super::EndPlay(EndPlayReason);
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
