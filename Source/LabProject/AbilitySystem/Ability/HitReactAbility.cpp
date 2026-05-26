#include "AbilitySystem/Ability/HitReactAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "Character/PdCharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Item/ItemDefinition.h"
#include "PlayerComponent/EquipmentComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(HitReactAbility)

UHitReactAbility::UHitReactAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	bRetriggerInstancedAbility = true;

	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(LabGameplayTags::GameplayAbility_HitReaction);
	AbilityAssetTags.AddTag(LabGameplayTags::Action_HitReact);
	SetAssetTags(AbilityAssetTags);

	CancelAbilitiesWithTag.AddTag(LabGameplayTags::GameplayAbility);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_Attack);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_RangedAttack);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_Equip);
	CancelAbilitiesWithTag.AddTag(LabGameplayTags::Action_Unequip);
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
		UE_LOG(LogTemp, Warning, TEXT("HitReactAbility failed: character is null. ability=%s"), *GetNameSafe(this));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("HitReactAbility activated: character=%s authority=%s avatar=%s owner=%s"),
		*GetNameSafe(Character),
		HasAuthority(&ActivationInfo) ? TEXT("true") : TEXT("false"),
		ActorInfo ? *GetNameSafe(ActorInfo->AvatarActor.Get()) : TEXT("None"),
		ActorInfo ? *GetNameSafe(ActorInfo->OwnerActor.Get()) : TEXT("None"));

	UAnimMontage* Montage = nullptr;
	FName StartSectionName = HitReactStartSectionName;
	if (const UEquipmentComponent* EquipmentComponent = Character->FindComponentByClass<UEquipmentComponent>())
	{
		FHitReactData HitReactData;
		if (EquipmentComponent->GetHitReactData(HitReactData))
		{
			Montage = HitReactData.HitReactMontage;
			UE_LOG(LogTemp, Log, TEXT("HitReactAbility montage from equipment: character=%s item=%s montage=%s"),
				*GetNameSafe(Character),
				*GetNameSafe(HitReactData.ItemDefinition),
				*GetNameSafe(Montage));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("HitReactAbility equipment has no hit react data: character=%s"), *GetNameSafe(Character));
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("HitReactAbility no equipment component: character=%s"), *GetNameSafe(Character));
	}

	if (!Montage)
	{
		Montage = HitReactMontage.LoadSynchronous();
		UE_LOG(LogTemp, Log, TEXT("HitReactAbility fallback montage: character=%s montage=%s softPath=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(Montage),
			*HitReactMontage.ToSoftObjectPath().ToString());
	}

	if (!Montage)
	{
		Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Animation/Stickman/Unarmed/React/AM_HitReact.AM_HitReact"));
		UE_LOG(LogTemp, Log, TEXT("HitReactAbility project default montage: character=%s montage=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(Montage));
	}

	if (!ensure(Montage))
	{
		UE_LOG(LogTemp, Warning, TEXT("HitReactAbility ended: no montage configured. character=%s ability=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(GetClass()));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!ensure(CommitAbility(Handle, ActorInfo, ActivationInfo)))
	{
		UE_LOG(LogTemp, Warning, TEXT("HitReactAbility commit failed: character=%s ability=%s"),
			*GetNameSafe(Character),
			*GetNameSafe(GetClass()));
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
		true);
	if (!ensure(MontageTask))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UHitReactAbility::OnHitReactMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(this, &UHitReactAbility::OnHitReactMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UHitReactAbility::OnHitReactMontageCancelled);
	UE_LOG(LogTemp, Log, TEXT("HitReactAbility playing montage: character=%s montage=%s section=%s"),
		*GetNameSafe(Character),
		*GetNameSafe(Montage),
		*StartSectionName.ToString());
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
