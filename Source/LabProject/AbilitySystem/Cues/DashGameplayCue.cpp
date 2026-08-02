#include "AbilitySystem/Cues/DashGameplayCue.h"

#include "Common/LabGameplayTags.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DashGameplayCue)

namespace
{
	bool ShouldHideCharacterMeshForDashCue(const FGameplayCueParameters& Parameters)
	{
		return Parameters.RawMagnitude >= 0.0f;
	}
}

UDashGameplayCue::UDashGameplayCue()
{
	GameplayCueTag = LabGameplayTags::GameplayCue_Dash_Active;
}

bool UDashGameplayCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!IsValid(MyTarget))
	{
		return false;
	}

	USceneComponent* RootComponent = MyTarget->GetRootComponent();
	if (StartEmitter && RootComponent)
	{
		UGameplayStatics::SpawnEmitterAttached(
			StartEmitter,
			RootComponent,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			StartEmitterScale,
			EAttachLocation::KeepRelativeOffset,
			true,
			EPSCPoolMethod::None,
			true);
	}

	if (StartSound && RootComponent)
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, StartSound, RootComponent->GetComponentLocation());
	}

	if (ShouldHideCharacterMeshForDashCue(Parameters))
	{
		SetCharacterMeshVisibility(MyTarget, false);
	}
	return true;
}

bool UDashGameplayCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!IsValid(MyTarget))
	{
		return false;
	}

	const FVector RemovedLocation = MyTarget->GetActorLocation();
	if (RemovedEmitter)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			MyTarget,
			RemovedEmitter,
			RemovedLocation,
			FRotator::ZeroRotator,
			RemovedEmitterScale,
			true,
			EPSCPoolMethod::None,
			true);
	}

	if (RemovedSound)
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, RemovedSound, RemovedLocation + RemovedSoundLocationOffset);
	}

	if (ShouldHideCharacterMeshForDashCue(Parameters))
	{
		SetCharacterMeshVisibility(MyTarget, true);
	}
	return true;
}

void UDashGameplayCue::SetCharacterMeshVisibility(AActor* Target, bool bVisible)
{
	ACharacter* Character = Cast<ACharacter>(Target);
	if (!Character || !Character->GetMesh())
	{
		return;
	}

	Character->GetMesh()->SetVisibility(bVisible, true);
}
