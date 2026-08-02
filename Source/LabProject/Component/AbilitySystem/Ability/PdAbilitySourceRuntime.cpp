#include "Component/AbilitySystem/Ability/PdAbilitySourceRuntime.h"

#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Character/CharacterBase.h"
#include "Common/Enum_Direction.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Definition/AbilitySystem/StatusEffectDefinition.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerState.h"
#include "NiagaraSystem.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAbilitySourceRuntime)

namespace
{
const APdPlayerState* ResolvePdPlayerStateFromActorInfo(
	const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo)
	{
		return nullptr;
	}

	if (const ACharacterBase* Character =
		Cast<ACharacterBase>(ActorInfo->AvatarActor.Get()))
	{
		if (APdPlayerState* PlayerState =
			Character->GetPlayerState<APdPlayerState>())
		{
			return PlayerState;
		}
	}

	if (const APawn* Pawn = Cast<APawn>(ActorInfo->AvatarActor.Get()))
	{
		if (APdPlayerState* PlayerState = Pawn->GetPlayerState<APdPlayerState>())
		{
			return PlayerState;
		}
	}

	return Cast<APdPlayerState>(ActorInfo->OwnerActor.Get());
}

float GetPandoraLoadoutDamageBonusPercent(
	const UBasicAttributeSet* AttributeSet,
	const EEnum_Direction LoadoutDirection)
{
	if (!AttributeSet)
	{
		return 0.0f;
	}

	switch (LoadoutDirection)
	{
	case EEnum_Direction::Left:
		return FMath::Max(AttributeSet->GetFirstPandora(), 0.0f);
	case EEnum_Direction::Up:
		return FMath::Max(AttributeSet->GetSecondPandora(), 0.0f);
	case EEnum_Direction::Right:
		return FMath::Max(AttributeSet->GetThirdPandora(), 0.0f);
	case EEnum_Direction::Center:
	case EEnum_Direction::Down:
	default:
		return 0.0f;
	}
}
}

const FGameplayAbilitySpec* UPdAbilitySourceRuntime::ResolveCurrentAbilitySpec(
	const UPdGameplayAbility& Ability) const
{
	if (const FGameplayAbilitySpec* AbilitySpec = Ability.GetCurrentAbilitySpec())
	{
		return AbilitySpec;
	}

	UPdAbilitySystemComponent* AbilitySystemComponent =
		Ability.GetPdAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpecHandle SpecHandle =
		Ability.GetCurrentAbilitySpecHandle();
	return AbilitySystemComponent && SpecHandle.IsValid()
		? AbilitySystemComponent->FindAbilitySpecFromHandle(SpecHandle)
		: nullptr;
}

UObject* UPdAbilitySourceRuntime::GetCurrentAbilitySpecSourceObject(
	const UPdGameplayAbility& Ability) const
{
	const FGameplayAbilitySpec* AbilitySpec = ResolveCurrentAbilitySpec(Ability);
	return AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr;
}

const USkillDefinition* UPdAbilitySourceRuntime::ResolveSkillDataAsset(
	const UPdGameplayAbility& Ability,
	const FGameplayAbilitySpec* AbilitySpec,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	const FGameplayAbilityActorInfo* ResolvedActorInfo =
		ActorInfo ? ActorInfo : Ability.GetCurrentActorInfo();

	const UObject* SourceObject =
		AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr;
	if (const UPandoraSkillRuntimeContext* RuntimeContext =
		Cast<UPandoraSkillRuntimeContext>(SourceObject))
	{
		return RuntimeContext->GetSkillDataAsset();
	}

	if (const USkillDefinition* SkillDataAsset =
		Cast<USkillDefinition>(SourceObject))
	{
		return SkillDataAsset;
	}

	const int32 SkillIndex = GetPandoraSkillIndex(AbilitySpec);
	if (SkillIndex == INDEX_NONE)
	{
		return nullptr;
	}

	const APdPlayerState* PlayerState =
		ResolvePdPlayerStateFromActorInfo(ResolvedActorInfo);
	const UPandoraComponent* PandoraComponent =
		PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	const UPandoraDefinition* PandoraDefinition =
		PandoraComponent
			? PandoraComponent->GetCurrentPandoraDefinition()
			: nullptr;
	if (!PandoraDefinition || !PandoraDefinition->Skill.IsValidIndex(SkillIndex))
	{
		return nullptr;
	}

	return PandoraDefinition->Skill[SkillIndex].SkillDefinition.Get();
}

USkillDefinition* UPdAbilitySourceRuntime::GetSourceSkillDataAsset(
	const UPdGameplayAbility& Ability) const
{
	if (const UPandoraSkillRuntimeContext* RuntimeContext =
		GetSourceSkillRuntimeContext(Ability))
	{
		return const_cast<USkillDefinition*>(
			RuntimeContext->GetSkillDataAsset());
	}

	if (USkillDefinition* SkillDataAsset =
		Cast<USkillDefinition>(GetCurrentAbilitySpecSourceObject(Ability)))
	{
		return SkillDataAsset;
	}

	const FGameplayAbilitySpec* AbilitySpec =
		ResolveCurrentAbilitySpec(Ability);
	return const_cast<USkillDefinition*>(
		ResolveSkillDataAsset(
			Ability,
			AbilitySpec,
			Ability.GetCurrentActorInfo()));
}

UPandoraSkillRuntimeContext*
UPdAbilitySourceRuntime::GetSourceSkillRuntimeContext(
	const UPdGameplayAbility& Ability) const
{
	if (UPandoraSkillRuntimeContext* RuntimeContext =
		Cast<UPandoraSkillRuntimeContext>(
			GetCurrentAbilitySpecSourceObject(Ability)))
	{
		return RuntimeContext;
	}

	return ResolveSourceSkillRuntimeContextFromSelectedPandora(Ability);
}

UPandoraSkillRuntimeContext*
UPdAbilitySourceRuntime::ResolveSourceSkillRuntimeContextFromSelectedPandora(
	const UPdGameplayAbility& Ability) const
{
	const FGameplayAbilitySpec* AbilitySpec =
		ResolveCurrentAbilitySpec(Ability);
	const int32 SkillIndex = GetPandoraSkillIndex(AbilitySpec);
	if (SkillIndex == INDEX_NONE)
	{
		return nullptr;
	}

	const APdPlayerState* PlayerState =
		Ability.GetPdPlayerStateFromActorInfo();
	const UPandoraComponent* PandoraComponent =
		PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	const UPandoraDefinition* PandoraDefinition =
		PandoraComponent
			? PandoraComponent->GetCurrentPandoraDefinition()
			: nullptr;
	if (!PandoraDefinition || !PandoraDefinition->Skill.IsValidIndex(SkillIndex))
	{
		return nullptr;
	}

	const FSkill& Skill = PandoraDefinition->Skill[SkillIndex];
	const USkillDefinition* SkillDataAsset = Skill.SkillDefinition.Get();
	if (!SkillDataAsset)
	{
		return nullptr;
	}

	const int32 RuntimeLevel = AbilitySpec
		? FMath::Max(AbilitySpec->Level, 1)
		: FMath::Max(Ability.GetAbilityLevel(), 1);
	const EEnum_Direction LoadoutDirection = PandoraComponent
		? PandoraComponent->GetCurrentPandoraLoadoutDirection()
		: EEnum_Direction::Center;
	if (CachedResolvedSourceSkillRuntimeContext
		&& CachedResolvedSourceSkillRuntimeContext->GetPandoraDefinition()
			== PandoraDefinition
		&& CachedResolvedSourceSkillRuntimeContext->GetSkillDataAsset()
			== SkillDataAsset
		&& CachedResolvedSourceSkillRuntimeContext->GetSkillIndex()
			== SkillIndex
		&& CachedResolvedSourceSkillRuntimeContext->GetPandoraLevel()
			== RuntimeLevel
		&& CachedResolvedSourceSkillRuntimeContext->GetLoadoutDirection()
			== LoadoutDirection)
	{
		return CachedResolvedSourceSkillRuntimeContext.Get();
	}

	UPdAbilitySourceRuntime* MutableThis =
		const_cast<UPdAbilitySourceRuntime*>(this);
	UPdGameplayAbility* MutableAbility =
		const_cast<UPdGameplayAbility*>(&Ability);
	MutableThis->CachedResolvedSourceSkillRuntimeContext =
		NewObject<UPandoraSkillRuntimeContext>(MutableAbility);
	MutableThis->CachedResolvedSourceSkillRuntimeContext->Initialize(
		PandoraDefinition,
		SkillDataAsset,
		SkillIndex,
		RuntimeLevel,
		LoadoutDirection);
	return MutableThis->CachedResolvedSourceSkillRuntimeContext.Get();
}

TArray<FProjectileImpactEffectAreaSpawnConfig>
UPdAbilitySourceRuntime::GetSourceProjectileImpactEffectAreas(
	const UPdGameplayAbility& Ability) const
{
	if (const UPandoraSkillRuntimeContext* RuntimeContext =
		GetSourceSkillRuntimeContext(Ability))
	{
		return RuntimeContext->GetProjectileImpactEffectAreas();
	}

	return TArray<FProjectileImpactEffectAreaSpawnConfig>();
}

int32 UPdAbilitySourceRuntime::GetPandoraSkillIndex(
	const FGameplayAbilitySpec* AbilitySpec) const
{
	if (!AbilitySpec)
	{
		return INDEX_NONE;
	}

	const FGameplayTagContainer& SourceTags =
		AbilitySpec->GetDynamicSpecSourceTags();
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill1))
	{
		return 0;
	}
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill2))
	{
		return 1;
	}
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill3))
	{
		return 2;
	}
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill4))
	{
		return 3;
	}

	return INDEX_NONE;
}

bool UPdAbilitySourceRuntime::IsPandoraSkillSpec(
	const FGameplayAbilitySpec* AbilitySpec) const
{
	return AbilitySpec
		&& (Cast<UPandoraSkillRuntimeContext>(
				AbilitySpec->SourceObject.Get())
			|| GetPandoraSkillIndex(AbilitySpec) != INDEX_NONE);
}

AWeaponBase* UPdAbilitySourceRuntime::GetCurrentWeaponActorFromAvatar(
	const UPdGameplayAbility& Ability) const
{
	const ACharacterBase* Character = Ability.GetPdCharacterFromActorInfo();
	const UEquipmentComponent* EquipmentComponent =
		Character ? Character->GetEquipmentComponent() : nullptr;
	return EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponActor()
		: nullptr;
}

bool UPdAbilitySourceRuntime::HasCurrentWeaponSkillTrail(
	const UPdGameplayAbility& Ability) const
{
	const AWeaponBase* CurrentWeapon =
		GetCurrentWeaponActorFromAvatar(Ability);
	return CurrentWeapon && CurrentWeapon->HasSkillWeaponTrailComponent();
}

bool UPdAbilitySourceRuntime::StartCurrentWeaponSkillTrail(
	const UPdGameplayAbility& Ability,
	UNiagaraSystem* TrailSystem) const
{
	AWeaponBase* CurrentWeapon = GetCurrentWeaponActorFromAvatar(Ability);
	return CurrentWeapon
		? CurrentWeapon->StartSkillWeaponTrail(TrailSystem)
		: false;
}

void UPdAbilitySourceRuntime::StopCurrentWeaponSkillTrail(
	const UPdGameplayAbility& Ability) const
{
	if (AWeaponBase* CurrentWeapon =
		GetCurrentWeaponActorFromAvatar(Ability))
	{
		CurrentWeapon->StopSkillWeaponTrail();
	}
}

float UPdAbilitySourceRuntime::GetPandoraAttackDamageBonus(
	const UPdGameplayAbility& Ability) const
{
	const UPdAbilitySystemComponent* AbilitySystemComponent =
		Ability.GetPdAbilitySystemComponentFromActorInfo();
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;

	const float IntelligenceDamagePercent = AttributeSet
		? FMath::Max(AttributeSet->GetIntelligence(), 0.0f)
		: 0.0f;
	return IntelligenceDamagePercent
		+ GetPandoraLoadoutAttackDamageBonus(Ability);
}

float UPdAbilitySourceRuntime::GetPandoraLoadoutAttackDamageBonus(
	const UPdGameplayAbility& Ability) const
{
	const UPdAbilitySystemComponent* AbilitySystemComponent =
		Ability.GetPdAbilitySystemComponentFromActorInfo();
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;

	const UPandoraSkillRuntimeContext* RuntimeContext =
		GetSourceSkillRuntimeContext(Ability);
	const EEnum_Direction LoadoutDirection = RuntimeContext
		? RuntimeContext->GetLoadoutDirection()
		: EEnum_Direction::Center;
	return GetPandoraLoadoutDamageBonusPercent(
		AttributeSet,
		LoadoutDirection);
}

float UPdAbilitySourceRuntime::CalculateBaseSkillDamageMagnitude(
	const FSkillGameplayEffectConfig& DamageConfig) const
{
	const double BaseMagnitude = FMath::Max(DamageConfig.Magnitude, 0.0);
	return static_cast<float>(FMath::Max(BaseMagnitude, 0.0));
}

float UPdAbilitySourceRuntime::ApplyIntelligenceToSkillDamage(
	const UPdGameplayAbility& Ability,
	const float DamageMagnitude) const
{
	const float AttackDamageBonusPercent =
		GetPandoraAttackDamageBonus(Ability);
	const double Multiplier =
		1.0
		+ static_cast<double>(
			FMath::Max(AttackDamageBonusPercent, 0.0f))
			* 0.01;
	return FMath::Max(
		static_cast<float>(
			static_cast<double>(FMath::Max(DamageMagnitude, 0.0f))
			* Multiplier),
		0.0f);
}

float UPdAbilitySourceRuntime::CalculateSkillDamageMagnitude(
	const UPdGameplayAbility& Ability,
	const FSkillGameplayEffectConfig& DamageConfig) const
{
	return ApplyIntelligenceToSkillDamage(
		Ability,
		CalculateBaseSkillDamageMagnitude(DamageConfig));
}

FGameplayEffectSpecHandle
UPdAbilitySourceRuntime::MakeConfiguredDamageEffectSpec(
	const UPdGameplayAbility& Ability,
	const FSkillGameplayEffectConfig& DamageConfig,
	const float DamageMagnitude,
	UObject* SourceObject) const
{
	UPdAbilitySystemComponent* SourceAbilitySystemComponent =
		Ability.GetPdAbilitySystemComponentFromActorInfo();
	if (!SourceAbilitySystemComponent || !DamageConfig.GameplayEffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle EffectContext =
		SourceAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddInstigator(
		Ability.GetAvatarActorFromActorInfo(),
		Ability.GetAvatarActorFromActorInfo());
	if (SourceObject)
	{
		EffectContext.AddSourceObject(SourceObject);
	}
	else if (const FGameplayAbilitySpec* AbilitySpec =
		ResolveCurrentAbilitySpec(Ability))
	{
		if (UObject* AbilitySourceObject = AbilitySpec->SourceObject.Get())
		{
			EffectContext.AddSourceObject(AbilitySourceObject);
		}
	}

	FGameplayEffectSpecHandle DamageSpecHandle =
		SourceAbilitySystemComponent->MakeOutgoingSpec(
			DamageConfig.GameplayEffectClass,
			FMath::Max(Ability.GetAbilityLevel(), 1),
			EffectContext);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayTag DamageDataTag = DamageConfig.MagnitudeDataTag;
	if (!DamageDataTag.IsValid())
	{
		SourceAbilitySystemComponent->ResolveDamageMagnitudeSetByCallerTag(
			DamageDataTag);
	}

	if (DamageDataTag.IsValid())
	{
		DamageSpecHandle.Data->SetSetByCallerMagnitude(
			DamageDataTag,
			DamageMagnitude);
	}

	if (const USkillDefinition* SkillDefinition =
		GetSourceSkillDataAsset(Ability))
	{
		if (const UStatusEffectDefinition* StatusEffectDefinition =
			SkillDefinition->StatusEffectDataAsset.Get())
		{
			StatusEffectDefinition->AppendRemovalPolicyTags(DamageSpecHandle);
		}
	}

	return DamageSpecHandle;
}
