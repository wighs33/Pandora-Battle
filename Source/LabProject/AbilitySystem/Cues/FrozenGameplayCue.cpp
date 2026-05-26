#include "AbilitySystem/Cues/FrozenGameplayCue.h"

#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentPoolMethodEnum.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FrozenGameplayCue)

AFrozenGameplayCue::AFrozenGameplayCue()
{
	GameplayCueTag = LabGameplayTags::GameplayCue_Frozen;
	bAutoDestroyOnRemove = true;
}

bool AFrozenGameplayCue::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
	return EventType == EGameplayCueEvent::OnActive
		|| EventType == EGameplayCueEvent::WhileActive
		|| EventType == EGameplayCueEvent::Removed;
}

bool AFrozenGameplayCue::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(Parameters);
	return ApplyFrozenEffect(MyTarget, true);
}

bool AFrozenGameplayCue::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(Parameters);
	return ApplyFrozenEffect(MyTarget, false);
}

bool AFrozenGameplayCue::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	static_cast<void>(Parameters);
	RemoveFrozenEffect(MyTarget);
	return true;
}

bool AFrozenGameplayCue::ApplyFrozenEffect(AActor* MyTarget, bool bPlaySound)
{
	if (!IsValid(MyTarget))
	{
		return false;
	}

	if (bApplyTimeDilation)
	{
		MyTarget->CustomTimeDilation = FrozenTimeDilation;
	}

	if (!FrozenEffectComponent && FrozenSystem)
	{
		USceneComponent* AttachComponent = ResolveAttachComponent(MyTarget);
		if (!AttachComponent)
		{
			return false;
		}

		FrozenEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			FrozenSystem,
			AttachComponent,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false,
			true,
			ENCPoolMethod::None,
			true);

		if (FrozenEffectComponent)
		{
			FrozenEffectComponent->SetWorldScale3D(EffectScale);
		}
	}

	if (bPlaySound && FrozenStartSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FrozenStartSound, MyTarget->GetActorLocation());
	}

	return FrozenEffectComponent != nullptr || bApplyTimeDilation;
}

void AFrozenGameplayCue::RemoveFrozenEffect(AActor* MyTarget)
{
	if (bApplyTimeDilation && IsValid(MyTarget))
	{
		MyTarget->CustomTimeDilation = RestoredTimeDilation;
	}

	if (!IsValid(FrozenEffectComponent))
	{
		FrozenEffectComponent = nullptr;
		return;
	}

	FrozenEffectComponent->DestroyComponent();
	FrozenEffectComponent = nullptr;
}

USceneComponent* AFrozenGameplayCue::ResolveAttachComponent(AActor* MyTarget) const
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
