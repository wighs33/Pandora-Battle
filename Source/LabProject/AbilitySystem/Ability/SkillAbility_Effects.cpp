#include "AbilitySystem/Ability/SkillAbility.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Character/CharacterBase.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Component/Character/CharacterPresentationComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "GameplayEffect.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/MeleeWeapon.h"

FGameplayEffectSpecHandle USkillAbility::MakeConfiguredDamageEffectSpec(
	const FSkillGameplayEffectConfig& DamageConfig, const float DamageMagnitude, UObject* SourceObject) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent || !DamageConfig.GameplayEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	if (SourceObject)
	{
		EffectContext.AddSourceObject(SourceObject);
	}
	else
	{
		EffectContext.AddSourceObject(GetCurrentSourceObject());
	}

	FGameplayEffectSpecHandle DamageSpecHandle =
		SourceAbilitySystemComponent->MakeOutgoingSpec(DamageConfig.GameplayEffectClass, FMath::Max(GetAbilityLevel(), 1), EffectContext);
	if (!DamageSpecHandle.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayTag DamageDataTag = DamageConfig.MagnitudeDataTag;
	if (!DamageDataTag.IsValid())
	{
		DamageDataTag = UProjectTagConfig::GetDefaultConfig()->GetSetByCallerDamageMagnitudeTag();
	}

	if (DamageDataTag.IsValid())
	{
		DamageSpecHandle.Data->SetSetByCallerMagnitude(DamageDataTag, DamageMagnitude);
	}

	return DamageSpecHandle;
}

FGameplayEffectSpecHandle USkillAbility::MakeConfiguredStatusEffectSpec(const USkillDefinition* SkillDataAsset,
	const TSubclassOf<UGameplayEffect> FallbackStatusEffectClass, const float FallbackStatusEffectLevel) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	const UStatusEffectDefinition* StatusEffectDefinition = SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
	if (StatusEffectDefinition)
	{
		StatusEffectDefinition->SynchronizeStackEffectStackLimit();
	}
	const TSubclassOf<UGameplayEffect> DebuffGameplayEffectClass =
		StatusEffectDefinition ? StatusEffectDefinition->StackGameplayEffectClass : FallbackStatusEffectClass;
	if (!SourceAbilitySystemComponent || !DebuffGameplayEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystemComponent->MakeEffectContext();
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	EffectContext.AddInstigator(AvatarActor, AvatarActor);
	EffectContext.AddSourceObject(GetCurrentSourceObject());

	const float StatusEffectLevel = StatusEffectDefinition && SkillDataAsset ? FMath::Max(SkillDataAsset->StatusEffectLevel, 1.0f)
																			 : FMath::Max(FallbackStatusEffectLevel, 1.0f);
	FGameplayEffectSpecHandle StatusEffectSpecHandle =
		SourceAbilitySystemComponent->MakeOutgoingSpec(DebuffGameplayEffectClass, StatusEffectLevel, EffectContext);
	if (!StatusEffectSpecHandle.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}
	if (SkillDataAsset)
	{
		StatusEffectSpecHandle.Data->SetStackCount(FMath::Max(SkillDataAsset->StackCount, 1));
	}

	if (!StatusEffectDefinition)
	{
		return StatusEffectSpecHandle;
	}
	StatusEffectSpecHandle.Data->SetDuration(
		StatusEffectTiming::FullStackLifetimeSeconds, true);

	return StatusEffectSpecHandle;
}

FActiveGameplayEffectHandle USkillAbility::ApplyConfiguredStatusEffectToTarget(const USkillDefinition* SkillDataAsset,
	UAbilitySystemComponent* TargetAbilitySystemComponent, const TSubclassOf<UGameplayEffect> FallbackStatusEffectClass,
	const float FallbackStatusEffectLevel) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent = GetPdAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent || !TargetAbilitySystemComponent)
	{
		return FActiveGameplayEffectHandle();
	}
	const UStatusEffectDefinition* StatusEffectDefinition = SkillDataAsset ? SkillDataAsset->StatusEffectDataAsset.Get() : nullptr;
	if (StatusEffectDefinition && !StatusEffectDefinition->CanStack(TargetAbilitySystemComponent))
	{
		return FActiveGameplayEffectHandle();
	}

	const FGameplayEffectSpecHandle StatusEffectSpecHandle =
		MakeConfiguredStatusEffectSpec(SkillDataAsset, FallbackStatusEffectClass, FallbackStatusEffectLevel);
	if (!StatusEffectSpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}

	const FActiveGameplayEffectHandle AppliedHandle =
		SourceAbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*StatusEffectSpecHandle.Data.Get(), TargetAbilitySystemComponent);
	AActor* TargetActor = TargetAbilitySystemComponent->GetAvatarActor();
	if (AppliedHandle.WasSuccessfullyApplied() && StatusEffectDefinition && TargetActor)
	{
		if (UStatusEffectReplicationComponent* ReplicationComponent =
				TargetActor->FindComponentByClass<UStatusEffectReplicationComponent>())
		{
			ReplicationComponent->TrackAppliedStatusEffect(StatusEffectDefinition, AppliedHandle);
		}
	}

	return AppliedHandle;
}

AWeaponBase* USkillAbility::GetCurrentWeaponActorFromAvatar() const
{
	const ACharacterBase* Character = GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent = Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

bool USkillAbility::HasCurrentWeaponSkillTrail() const
{
	const AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar();
	return CurrentWeapon && CurrentWeapon->HasSkillWeaponTrailComponent();
}

bool USkillAbility::StartCurrentWeaponSkillTrail(UNiagaraSystem* TrailSystem) const
{
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar();
	return CurrentWeapon ? CurrentWeapon->StartSkillWeaponTrail(TrailSystem) : false;
}

void USkillAbility::StopCurrentWeaponSkillTrail() const
{
	if (AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar())
	{
		CurrentWeapon->StopSkillWeaponTrail();
	}
}

void USkillAbility::StartConfiguredSelfBuff(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	StopConfiguredSelfBuff();
	const USkillDefinition* SkillDefinition = GetSourceSkillDataAsset();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	ACharacterBase* Character = GetPdCharacterFromActorInfo();
	if (!SkillDefinition || !SkillDefinition->SelfBuff.bEnabled || !ASC || !GetAvatarActorFromActorInfo())
	{
		return;
	}
	const FSkillSelfBuffSettings& Settings = SkillDefinition->SelfBuff;
	UCharacterPresentationComponent* Presentation = Character ? Character->GetCharacterPresentationComponent() : nullptr;
	if (Presentation && Settings.CharacterScaleMultiplier > 1.0)
	{
		Presentation->SetTemporaryMeshScaleMultiplier(this, static_cast<float>(Settings.CharacterScaleMultiplier));
		SelfBuffScaleOwner = Presentation;
	}
	if (Settings.WeaponTraceEndZMultiplier > 1.0)
	{
		if (AMeleeWeapon* Weapon = Cast<AMeleeWeapon>(GetCurrentWeaponActorFromAvatar()))
		{
			Weapon->SetTemporaryAttackTraceEndZMultiplier(this, static_cast<float>(Settings.WeaponTraceEndZMultiplier));
			SelfBuffTraceEndZWeapon = Weapon;
		}
	}
	// 버프 수치는 서버가 확정한다. 외형과 무기 검사 범위의 로컬 처리는 기존 방식대로 유지한다.
	if (!ASC->IsOwnerActorAuthoritative())
	{
		return;
	}
	if (UCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr; Combat && Settings.WeaponDamageBonus > 0.0)
	{
		Combat->SetTemporaryWeaponDamageBonus(this, static_cast<float>(Settings.WeaponDamageBonus));
		SelfBuffCombatComponent = Combat;
	}
	if (!Settings.GameplayEffectClass)
	{
		return;
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(GetAvatarActorFromActorInfo(), GetAvatarActorFromActorInfo());
	Context.AddSourceObject(SkillDefinition);
	FGameplayEffectSpecHandle Spec =
		ASC->MakeOutgoingSpec(Settings.GameplayEffectClass, FMath::Max(GetAbilityLevel(Handle, ActorInfo), 1), Context);
	if (!Spec.IsValid())
	{
		return;
	}
	if (Settings.MagnitudeDataTag.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(Settings.MagnitudeDataTag, static_cast<float>(Settings.Magnitude));
	}
	const FActiveGameplayEffectHandle AppliedHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	if (AppliedHandle.IsValid() && Settings.bRemoveOnAbilityEnd)
	{
		ActiveSelfBuffEffectHandle = AppliedHandle;
		SelfBuffAbilitySystemComponent = ASC;
	}
}

void USkillAbility::StopConfiguredSelfBuff()
{
	if (UCharacterPresentationComponent* Presentation = SelfBuffScaleOwner.Get())
	{
		Presentation->ClearTemporaryMeshScaleMultiplier(this);
	}
	SelfBuffScaleOwner.Reset();
	if (AMeleeWeapon* Weapon = SelfBuffTraceEndZWeapon.Get())
	{
		Weapon->ClearTemporaryAttackTraceEndZMultiplier(this);
	}
	SelfBuffTraceEndZWeapon.Reset();
	if (UCombatComponent* Combat = SelfBuffCombatComponent.Get())
	{
		Combat->ClearTemporaryWeaponDamageBonus(this);
	}
	SelfBuffCombatComponent.Reset();
	if (UAbilitySystemComponent* ASC = SelfBuffAbilitySystemComponent.Get(); ASC && ASC->IsOwnerActorAuthoritative())
	{
		ASC->RemoveActiveGameplayEffect(ActiveSelfBuffEffectHandle);
	}
	ActiveSelfBuffEffectHandle.Invalidate();
	SelfBuffAbilitySystemComponent.Reset();
}
