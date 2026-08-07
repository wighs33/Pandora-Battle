#include "AbilitySystem/Cues/ShieldUpGameplayCue.h"

#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShieldUpGameplayCue)

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

	if (const ACharacterBase* Character = Cast<ACharacterBase>(MyTarget))
	{
		return Character->GetMesh();
	}

	return Cast<USkeletalMeshComponent>(MyTarget->GetComponentByClass(USkeletalMeshComponent::StaticClass()));
}

bool AShieldUpGameplayCue::ApplyShieldOverlay(
	AActor* MyTarget,
	UMaterialInterface* OverlayMaterial,
	const bool bPlaySound)
{
	USkeletalMeshComponent* SkeletalMeshComponent = ResolveSkeletalMesh(MyTarget);
	if (!SkeletalMeshComponent)
	{

		return false;
	}

	if (ACharacterBase* Character = Cast<ACharacterBase>(MyTarget))
	{
		if (IsValid(OverlayMaterial))
		{
			Character->ApplySkillPresentationOverlay(this, OverlayMaterial);
		}
		else
		{
			Character->ClearSkillPresentationOverlay(this);
		}
	}
	else
	{
		if (IsValid(OverlayMaterial))
		{
			if (FallbackOverlayMesh.Get() != SkeletalMeshComponent)
			{
				FallbackOverlayMesh = SkeletalMeshComponent;
				FallbackPreviousOverlayMaterial =
					SkeletalMeshComponent->GetOverlayMaterial();
			}
			SkeletalMeshComponent->SetOverlayMaterial(OverlayMaterial);
		}
		else if (FallbackOverlayMesh.Get() == SkeletalMeshComponent)
		{
			SkeletalMeshComponent->SetOverlayMaterial(
				FallbackPreviousOverlayMaterial);
			FallbackOverlayMesh.Reset();
			FallbackPreviousOverlayMaterial = nullptr;
		}
	}

	if (bPlaySound && ShieldUpSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ShieldUpSound,
			SkeletalMeshComponent->GetComponentLocation());
	}

	return true;
}
