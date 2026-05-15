#include "AbilitySystem/Ability/HitReactAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/PdCharacterBase.h"
#include "PlayerComponent/EquipmentComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(HitReactAbility)

UHitReactAbility::UHitReactAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRetriggerInstancedAbility = true;
}

// State helpers
void UHitReactAbility::ClearActiveHitReactEffect()
{
	if (HitReactEffectClass && HasAuthority(&CurrentActivationInfo))
	{
		RemoveGameplayEffect(HitReactEffectClass);
	}
}

// Timing callbacks
void UHitReactAbility::OnHitReactMontageCompleted()
{
	ClearActiveHitReactEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UHitReactAbility::OnHitReactMontageInterrupted()
{
	ClearActiveHitReactEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UHitReactAbility::OnHitReactMontageCancelled()
{
	ClearActiveHitReactEffect();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

// Ability flow
void UHitReactAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	APdCharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!ensure(Character))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UAnimMontage* Montage = nullptr;
	FName StartSectionName = HitReactStartSectionName;
	if (const UEquipmentComponent* EquipmentComponent = Character->FindComponentByClass<UEquipmentComponent>())
	{
		FHitReactData HitReactData;
		if (EquipmentComponent->GetHitReactData(HitReactData))
		{
			Montage = HitReactData.HitReactMontage;
		}
	}

	if (!Montage)
	{
		Montage = HitReactMontage.LoadSynchronous();
	}

	if (!ensure(Montage))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!StartSectionName.IsNone() && Montage->GetSectionIndex(StartSectionName) == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("HitReactAbility: start section '%s' was not found in montage '%s'. Falling back to the default start."),
			*StartSectionName.ToString(),
			*GetNameSafe(Montage));
		StartSectionName = NAME_None;
	}

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		Montage,
		1.f,
		StartSectionName,
		false,
		1.f,
		0.f,
		false);
	if (!ensure(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UHitReactAbility::OnHitReactMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UHitReactAbility::OnHitReactMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UHitReactAbility::OnHitReactMontageCancelled);
	MontageTask->ReadyForActivation();

	if (HitReactEffectClass)
	{
		ApplyGameplayEffect(HitReactEffectClass, 1.f, 1);
	}

	if (HitReactCueTag.IsValid())
	{
		FGameplayCueParameters CueParameters;
		CueParameters.Location = Character->GetActorLocation();
		CueParameters.Instigator = Character;
		CueParameters.EffectCauser = Character;
		K2_ExecuteGameplayCueWithParams(HitReactCueTag, CueParameters);
	}

	static_cast<void>(TriggerEventData);
}
