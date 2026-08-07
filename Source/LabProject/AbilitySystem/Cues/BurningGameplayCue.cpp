#include "AbilitySystem/Cues/BurningGameplayCue.h"

#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentPoolMethodEnum.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BurningGameplayCue)

ABurningGameplayCue::ABurningGameplayCue()
{
	GameplayCueTag = LabGameplayTags::GameplayCue_Burning;
	bAutoDestroyOnRemove = true;
}

bool ABurningGameplayCue::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return EventType == EGameplayCueEvent::OnActive
		|| EventType == EGameplayCueEvent::WhileActive
		|| EventType == EGameplayCueEvent::Removed;
}

bool ABurningGameplayCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(Parameters);
	return ApplyBurningEffect(MyTarget, true);
}

bool ABurningGameplayCue::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(Parameters);
	return ApplyBurningEffect(MyTarget, false);
}

bool ABurningGameplayCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(MyTarget);
	static_cast<void>(Parameters);
	RemoveBurningEffect();
	return true;
}

bool ABurningGameplayCue::ApplyBurningEffect(AActor* MyTarget, bool bPlaySound)
{
	if (!IsValid(MyTarget))
	{
		return false;
	}

	if (!BurningEffectComponent && BurningSystem)
	{
		USceneComponent* AttachComponent = ResolveAttachComponent(MyTarget);
		if (!AttachComponent)
		{
			return false;
		}

		BurningEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			BurningSystem,
			AttachComponent,
			AttachSocketName,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false,
			true,
			ENCPoolMethod::None,
			true);

		if (BurningEffectComponent)
		{
			BurningEffectComponent->SetWorldScale3D(EffectScale);
		}
	}

	if (bPlaySound && BurningStartSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BurningStartSound, MyTarget->GetActorLocation());
	}

	return BurningEffectComponent != nullptr;
}

void ABurningGameplayCue::RemoveBurningEffect()
{
	if (!IsValid(BurningEffectComponent))
	{
		BurningEffectComponent = nullptr;
		return;
	}

	BurningEffectComponent->DestroyComponent();
	BurningEffectComponent = nullptr;
}

USceneComponent* ABurningGameplayCue::ResolveAttachComponent(AActor* MyTarget) const
{
	if (!IsValid(MyTarget))
	{
		return nullptr;
	}

	if (USceneComponent* SkeletalMeshComponent = Cast<USceneComponent>(MyTarget->GetComponentByClass(USkeletalMeshComponent::StaticClass())))
	{
		return SkeletalMeshComponent;
	}

	return MyTarget->GetRootComponent();
}
