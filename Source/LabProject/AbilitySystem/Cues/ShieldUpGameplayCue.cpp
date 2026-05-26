#include "AbilitySystem/Cues/ShieldUpGameplayCue.h"

#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShieldUpGameplayCue)

DEFINE_LOG_CATEGORY_STATIC(LogShieldUpGameplayCue, Log, All);

AShieldUpGameplayCue::AShieldUpGameplayCue()
{
	GameplayCueTag = LabGameplayTags::GameplayCue_ShieldUp;
	bAutoDestroyOnRemove = true;
}

bool AShieldUpGameplayCue::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return EventType == EGameplayCueEvent::OnActive
		|| EventType == EGameplayCueEvent::WhileActive
		|| EventType == EGameplayCueEvent::Removed;
}

bool AShieldUpGameplayCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(Parameters);
	return ApplyShieldOverlay(MyTarget, ShieldOverlayMaterial, true);
}

bool AShieldUpGameplayCue::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(Parameters);
	return ApplyShieldOverlay(MyTarget, ShieldOverlayMaterial, false);
}

bool AShieldUpGameplayCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(Parameters);
	return ApplyShieldOverlay(MyTarget, nullptr, false);
}

USkeletalMeshComponent* AShieldUpGameplayCue::ResolveSkeletalMesh(AActor* MyTarget) const
{
	if (!IsValid(MyTarget))
	{
		return nullptr;
	}

	return Cast<USkeletalMeshComponent>(MyTarget->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
}

bool AShieldUpGameplayCue::ApplyShieldOverlay(AActor* MyTarget, UMaterialInterface* OverlayMaterial, bool bPlaySound) const
{
	USkeletalMeshComponent* SkeletalMeshComponent = ResolveSkeletalMesh(MyTarget);
	if (!SkeletalMeshComponent)
	{
		UE_LOG(LogShieldUpGameplayCue, Warning,
			TEXT("ShieldUp cue skipped: skeletal mesh missing. cue=%s target=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MyTarget));
		return false;
	}

	SkeletalMeshComponent->SetOverlayMaterial(OverlayMaterial);

	if (bPlaySound && ShieldUpSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ShieldUpSound,
			SkeletalMeshComponent->GetComponentLocation());
	}

	UE_LOG(LogShieldUpGameplayCue, Log,
		TEXT("ShieldUp cue applied: target=%s overlay=%s playSound=%s"),
		*GetNameSafe(MyTarget),
		*GetNameSafe(OverlayMaterial),
		bPlaySound ? TEXT("true") : TEXT("false"));
	return true;
}
