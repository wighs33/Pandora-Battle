#include "PlayerComponent//CombatComponent.h"

#include "AbilitySystem/Ability/HitReactAbility.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "GameplayEffect.h"
#include "Item/ItemDefinition.h"
#include "Item/ItemInstance.h"
#include "Character/PdCharacterBase.h"
#include "Weapon/WeaponBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(CombatComponent)

/** 전투 컴포넌트 기본 상태를 초기화합니다. */
UCombatComponent::UCombatComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================
	// === 기본 설정

	PrimaryComponentTick.bCanEverTick = false;
}

/** 현재 무기 데미지 수치를 반환합니다. */
float UCombatComponent::GetCurrentWeaponDamageMagnitude()
{
	// =================================================================================================================
	// === 반환값 초기화

	CurrentWeaponDamageMagnitude = 0.f;

	// =================================================================================================================
	// === 소유 ASC 확인

	UPdAbilitySystemComponent* OwnerASC = GetOwnerPdAbilitySystemComponent();
	if (!OwnerASC)
	{
		return 0.f;
	}

	// =================================================================================================================
	// === 데미지 Attribute 해석

	FGameplayAttribute DamageAttribute;
	if (!ResolveWeaponDamageAttribute(DamageAttribute))
	{
		return 0.f;
	}

	// =================================================================================================================
	// === 현재 수치 조회

	CurrentWeaponDamageMagnitude = OwnerASC->GetNumericAttribute(DamageAttribute);
	return CurrentWeaponDamageMagnitude;
}

/** 아이템 정보로 무기 데미지를 계산합니다. */
bool UCombatComponent::ApplyResolvedDamageToWeapon(AWeaponBase* InWeaponActor, UItemInstance* InItemInstance)
{
	// =================================================================================================================
	// === 입력값 검사

	if (!InWeaponActor || !InItemInstance || !InItemInstance->ItemDefinition || !WeaponDamageStatTag.IsValid())
	{
		CurrentWeaponDamageMagnitude = 0.f;
		return false;
	}

	// =================================================================================================================
	// === 기본값과 강화값 조회

	const UItemDefinition* ItemDefinition = InItemInstance->ItemDefinition.Get();
	const float BaseDamage = ItemDefinition->Map_Stat_Magnitude.FindRef(WeaponDamageStatTag);
	const float EnhancedDamage = InItemInstance->Map_EnhancedStat_Magnitude.FindRef(WeaponDamageStatTag);

	// =================================================================================================================
	// === 최종 데미지 계산

	CurrentWeaponDamageMagnitude = BaseDamage + EnhancedDamage;
	return true;
}

/** 현재 무기 데미지를 대상에게 적용합니다. */
bool UCombatComponent::ApplyWeaponDamageToTarget(AActor* TargetActor)
{
	// =================================================================================================================
	// === 전투 주체 조회

	APdCharacterBase* SourceCharacter = GetCharacterOwner();
	APdCharacterBase* TargetCharacter = Cast<APdCharacterBase>(TargetActor);
	UPdAbilitySystemComponent* SourceASC = GetOwnerPdAbilitySystemComponent();
	UPdAbilitySystemComponent* TargetASC = TargetCharacter ? TargetCharacter->GetPdAbilitySystemComponent() : nullptr;
	FGameplayTag DamageMagnitudeSetByCallerTag;

	// =================================================================================================================
	// === 적용 조건 검사

	if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter || !SourceASC || !TargetASC
		|| !WeaponDamageEffectClass || !SourceASC->ResolveDamageMagnitudeSetByCallerTag(DamageMagnitudeSetByCallerTag))
	{
		return false;
	}

	// =================================================================================================================
	// === 데미지 값 계산

	const float DamageAmount = GetCurrentWeaponDamageMagnitude();
	if (DamageAmount <= 0.f)
	{
		return false;
	}

	// =================================================================================================================
	// === 이펙트 스펙 생성

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(WeaponDamageEffectClass, 1.f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	// =================================================================================================================
	// === SetByCaller 설정 및 적용

	SpecHandle.Data->SetSetByCallerMagnitude(DamageMagnitudeSetByCallerTag, DamageAmount);
	const FActiveGameplayEffectHandle AppliedDamageEffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	if (!AppliedDamageEffectHandle.WasSuccessfullyApplied())
	{
		return false;
	}

	TryActivateHitReactAbilityOnTarget(TargetCharacter);
	return true;
}

/** 무기 데미지용 Attribute를 해석합니다. */
bool UCombatComponent::ResolveWeaponDamageAttribute(FGameplayAttribute& OutAttribute) const
{
	// =================================================================================================================
	// === 반환값 초기화

	OutAttribute = FGameplayAttribute();

	// =================================================================================================================
	// === ASC 및 태그 검사

	const UPdAbilitySystemComponent* OwnerASC = GetOwnerPdAbilitySystemComponent();
	if (!OwnerASC || !WeaponDamageStatTag.IsValid())
	{
		return false;
	}

	return OwnerASC->ResolveAttributeFromTag(WeaponDamageStatTag, OutAttribute);
}

/** 소유 캐릭터를 반환합니다. */
APdCharacterBase* UCombatComponent::GetCharacterOwner() const
{
	return Cast<APdCharacterBase>(GetOwner());
}

/** 소유자의 프로젝트 ASC를 반환합니다. */
UPdAbilitySystemComponent* UCombatComponent::GetOwnerPdAbilitySystemComponent() const
{
	const APdCharacterBase* CharacterOwner = GetCharacterOwner();
	return CharacterOwner ? CharacterOwner->GetPdAbilitySystemComponent() : nullptr;
}

bool UCombatComponent::TryActivateHitReactAbilityOnTarget(APdCharacterBase* TargetCharacter) const
{
	UPdAbilitySystemComponent* TargetASC = TargetCharacter ? TargetCharacter->GetPdAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : TargetASC->GetActivatableAbilities())
	{
		if (!AbilitySpec.Ability || !AbilitySpec.Ability->GetClass()->IsChildOf(UHitReactAbility::StaticClass()))
		{
			continue;
		}

		return TargetASC->TryActivateAbility(AbilitySpec.Handle, false);
	}

	return false;
}
