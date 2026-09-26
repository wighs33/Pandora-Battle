#include "AbilitySystem/Cues/StatusEffectGameplayCue.h"

#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StatusEffectGameplayCue)

AStatusEffectGameplayCue::AStatusEffectGameplayCue()
{
	bAutoDestroyOnRemove = true;
}

bool AStatusEffectGameplayCue::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return EventType == EGameplayCueEvent::OnActive
		|| EventType == EGameplayCueEvent::WhileActive
		|| EventType == EGameplayCueEvent::Removed;
}

bool AStatusEffectGameplayCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return ApplyEffect(MyTarget, true);
}

bool AStatusEffectGameplayCue::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	return ApplyEffect(MyTarget, false);
}

bool AStatusEffectGameplayCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	RemoveEffect();
	return true;
}

bool AStatusEffectGameplayCue::Recycle()
{
	RemoveEffect();
	return Super::Recycle();
}

void AStatusEffectGameplayCue::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveEffect();
	Super::EndPlay(EndPlayReason);
}

bool AStatusEffectGameplayCue::ApplyEffect(AActor* Target, bool bPlayStartSound)
{
	if (!IsValid(Target)) return false;

	if (!IsValid(EffectComponent) && EffectSystem)
	{
		USceneComponent* AttachComponent = Target->FindComponentByClass<USkeletalMeshComponent>();
		if (!AttachComponent) AttachComponent = Target->GetRootComponent();
		if (!AttachComponent) return false;

		EffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(EffectSystem, AttachComponent, AttachSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false, true, ENCPoolMethod::None, true);
		if (EffectComponent) EffectComponent->SetWorldScale3D(EffectScale);
	}

	if (bPlayStartSound && !bHasPlayedStartSound && StartSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, StartSound, Target->GetActorLocation());
		bHasPlayedStartSound = true;
	}

	return IsValid(EffectComponent);
}

void AStatusEffectGameplayCue::RemoveEffect()
{
	if (IsValid(EffectComponent)) EffectComponent->DestroyComponent();
	EffectComponent = nullptr;
	bHasPlayedStartSound = false;
}
