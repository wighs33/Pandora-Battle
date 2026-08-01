#include "BasicAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Character/PdCharacterBase.h"
#include "Common/LabGameplayTags.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(BasicAttributeSet)

DEFINE_LOG_CATEGORY_STATIC(LogPdAttributeSet, Log, All);

namespace
{
	bool RollPercentChance(float PercentChance)
	{
		const float ClampedPercentChance = FMath::Clamp(PercentChance, 0.f, 100.f);
		return ClampedPercentChance >= 100.f
			|| (ClampedPercentChance > 0.f && FMath::FRandRange(0.f, 100.f) < ClampedPercentChance);
	}

	float ClampResourceAttribute(float Value, float MaxValue)
	{
		return FMath::Clamp(Value, 0.f, FMath::Max(MaxValue, 0.f));
	}

	float CalculateArmorMitigatedDamage(const float IncomingDamageAmount, const float TargetArmor, const float ArmorMitigationScale)
	{
		const double ArmorDivider = 1.0 + (static_cast<double>(ArmorMitigationScale) * FMath::Max(TargetArmor, 0.f));
		return ArmorDivider > UE_DOUBLE_SMALL_NUMBER
			? FMath::Max(static_cast<float>(static_cast<double>(IncomingDamageAmount) / ArmorDivider), 0.f)
			: 0.f;
	}

	bool EffectSpecHasAssetTag(const FGameplayEffectSpec& EffectSpec, const FGameplayTag& AssetTag)
	{
		return AssetTag.IsValid()
			&& EffectSpec.Def
			&& EffectSpec.Def->GetAssetTags().HasTag(AssetTag);
	}

	bool EffectSpecHasStatusTag(const FGameplayEffectSpec& EffectSpec, const FGameplayTag& StatusTag)
	{
		if (!StatusTag.IsValid())
		{
			return false;
		}

		return EffectSpec.DynamicGrantedTags.HasTag(StatusTag)
			|| EffectSpec.GetDynamicAssetTags().HasTag(StatusTag)
			|| (EffectSpec.Def && EffectSpec.Def->GetGrantedTags().HasTag(StatusTag))
			|| (EffectSpec.Def && EffectSpec.Def->GetAssetTags().HasTag(StatusTag));
	}

	float CalculateStatusResistanceMitigatedDamage(
		const float IncomingDamageAmount,
		const float ResistancePercent)
	{
		const float ClampedResistance = FMath::Clamp(ResistancePercent, 0.f, 100.f);
		return FMath::Max(IncomingDamageAmount, 0.f) * (1.f - ClampedResistance * 0.01f);
	}

	float ResolveStatusResistance(
		const UBasicAttributeSet* AttributeSet,
		const FGameplayEffectSpec& EffectSpec,
		bool& bOutStatusDamage)
	{
		bOutStatusDamage = true;

		if (EffectSpecHasStatusTag(EffectSpec, LabGameplayTags::Status_Burning))
		{
			return AttributeSet ? AttributeSet->GetBurn() : 0.f;
		}

		if (EffectSpecHasStatusTag(EffectSpec, LabGameplayTags::Status_Frostbite))
		{
			return AttributeSet ? AttributeSet->GetFrostbite() : 0.f;
		}

		if (EffectSpecHasStatusTag(EffectSpec, LabGameplayTags::Status_ElectricShock))
		{
			return AttributeSet ? AttributeSet->GetElectricShock() : 0.f;
		}

		bOutStatusDamage = false;
		return 0.f;
	}

	bool TryActivateAbilityByTag(UAbilitySystemComponent* ASC, const FGameplayTag& AbilityTag)
	{
		if (!ASC || !AbilityTag.IsValid())
		{
			UE_LOG(LogPdAttributeSet, Warning, TEXT("TryActivateAbilityByTag failed: asc=%s tag=%s"),
				*GetNameSafe(ASC),
				*AbilityTag.ToString());
			return false;
		}

		FGameplayTagContainer AbilityTags;
		AbilityTags.AddTag(AbilityTag);
		const bool bActivated = ASC->TryActivateAbilitiesByTag(AbilityTags, true);
		UE_LOG(LogPdAttributeSet, Log, TEXT("TryActivateAbilityByTag: owner=%s tag=%s result=%s abilityCount=%d ownedTags=%s"),
			*GetNameSafe(ASC->GetOwnerActor()),
			*AbilityTag.ToString(),
			bActivated ? TEXT("true") : TEXT("false"),
			ASC->GetActivatableAbilities().Num(),
			*ASC->GetOwnedGameplayTags().ToStringSimple());
		return bActivated;
	}

	void TryActivateHitReactionAbility(UAbilitySystemComponent* ASC)
	{
		if (TryActivateAbilityByTag(ASC, LabGameplayTags::GameplayAbility_HitReaction))
		{
			return;
		}

		TryActivateAbilityByTag(ASC, LabGameplayTags::Action_HitReact);
	}

	void TryActivateDeathAbility(UAbilitySystemComponent* ASC, const TCHAR* Reason, const float OldHealth, const float NewHealth)
	{
		if (!ASC)
		{
			UE_LOG(LogPdAttributeSet, Warning,
				TEXT("Death activation failed: ASC is null reason=%s oldHealth=%.3f newHealth=%.3f"),
				Reason,
				OldHealth,
				NewHealth);
			return;
		}

		if (ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead))
		{
			UE_LOG(LogPdAttributeSet, Log,
				TEXT("Death activation skipped: owner=%s already has State.Dead reason=%s oldHealth=%.3f newHealth=%.3f ownedTags=%s"),
				*GetNameSafe(ASC->GetOwnerActor()),
				Reason,
				OldHealth,
				NewHealth,
				*ASC->GetOwnedGameplayTags().ToStringSimple());
			return;
		}

		const bool bActivated = TryActivateAbilityByTag(ASC, LabGameplayTags::GameplayAbility_Death);
		if (bActivated)
		{
			UE_LOG(LogPdAttributeSet, Log,
				TEXT("Death activation requested: owner=%s reason=%s oldHealth=%.3f newHealth=%.3f result=true abilityCount=%d ownedTags=%s"),
				*GetNameSafe(ASC->GetOwnerActor()),
				Reason,
				OldHealth,
				NewHealth,
				ASC->GetActivatableAbilities().Num(),
				*ASC->GetOwnedGameplayTags().ToStringSimple());
		}
		else
		{
			UE_LOG(LogPdAttributeSet, Warning,
				TEXT("Death activation requested: owner=%s reason=%s oldHealth=%.3f newHealth=%.3f result=false abilityCount=%d ownedTags=%s"),
				*GetNameSafe(ASC->GetOwnerActor()),
				Reason,
				OldHealth,
				NewHealth,
				ASC->GetActivatableAbilities().Num(),
				*ASC->GetOwnedGameplayTags().ToStringSimple());
		}
	}
}

UBasicAttributeSet::UBasicAttributeSet()
{
	Health = 100.0f;
	MaxHealth = 100.0f;
	Shield = 0.0f;
	MaxShield = 100.0f;
	Stamina = 100.0f;
	MaxStamina = 100.0f;
}

/** 복제할 Attribute 프로퍼티를 등록합니다. */
void UBasicAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// =================================================================================================================
	// === Push Model 복제 파라미터 설정

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	// =================================================================================================================
	// === 기본 공격 스탯 복제 등록
	
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Strength, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Intelligence, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Arcane, Params);
	
	// =================================================================================================================
	// === 기본 방어 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Armor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Recovery, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MagicResistance, Params);
	
	// =================================================================================================================
	// === 상태 이상 저항 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Immunity, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Fortitude, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Sanity, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Burn, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Frostbite, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ElectricShock, Params);
	
	// =================================================================================================================
	// === 판도라 관련 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, FirstPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, SecondPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, ThirdPandora, Params);
	
	// =================================================================================================================
	// === 전투 보조 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, AttackSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MovementSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, CriticalChance, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, CriticalDamageMultiplier, Params);
	
	// =================================================================================================================
	// === 자원 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Health, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxHealth, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Shield, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxShield, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Mana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxMana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, Stamina, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UBasicAttributeSet, MaxStamina, Params);
}

/** GameplayEffect 적용 후 최종 결과를 처리합니다. */
void UBasicAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	// =================================================================================================================
	// === 데미지 Attribute 후처리

	if (Data.EvaluatedData.Attribute == GetOutgoingDamageAttribute())
	{
		bLastOutgoingDamageCriticalHit = false;

		float FinalOutgoingDamage = GetOutgoingDamage();
		if (FinalOutgoingDamage <= 0.f)
		{
			SetOutgoingDamage(0.f);
			return;
		}

		if (GetCriticalDamageMultiplier() > 0.f && RollPercentChance(GetCriticalChance()))
		{
			FinalOutgoingDamage *= GetCriticalDamageMultiplier();
			bLastOutgoingDamageCriticalHit = true;
		}

		SetOutgoingDamage(FinalOutgoingDamage);
		return;
	}

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float AppliedIncomingDamage = GetIncomingDamage();
		const bool bCriticalHit = bPendingIncomingDamageCriticalHit;
		bool bStatusDamage = false;
		const float StatusResistance = ResolveStatusResistance(this, Data.EffectSpec, bStatusDamage);
		const float StatusMitigatedIncomingDamage = bStatusDamage
			? CalculateStatusResistanceMitigatedDamage(AppliedIncomingDamage, StatusResistance)
			: AppliedIncomingDamage;
		constexpr float ArmorMitigationScale = 0.05f;
		const float TargetArmor = GetArmor();
		const float MitigatedIncomingDamage = CalculateArmorMitigatedDamage(
			StatusMitigatedIncomingDamage,
			TargetArmor,
			ArmorMitigationScale);
		UE_LOG(LogPdAttributeSet, Log, TEXT("IncomingDamage executed: owner=%s effect=%s incoming=%.3f armor=%.3f scale=%.3f mitigated=%.3f hasHitReactTag=%s healthBefore=%.3f"),
			*GetNameSafe(GetOwningActor()),
			*GetNameSafe(Data.EffectSpec.Def),
			AppliedIncomingDamage,
			TargetArmor,
			ArmorMitigationScale,
			MitigatedIncomingDamage,
			EffectSpecHasAssetTag(Data.EffectSpec, LabGameplayTags::Effect_HitReaction) ? TEXT("true") : TEXT("false"),
			GetHealth());
		bPendingIncomingDamageCriticalHit = false;
		SetIncomingDamage(0.f);
		const float HealthDamage = ApplyIncomingDamage(MitigatedIncomingDamage, bCriticalHit);
		const bool bShouldHitReact =
			!bStatusDamage
			&& !FMath::IsNearlyZero(HealthDamage)
			&& EffectSpecHasAssetTag(Data.EffectSpec, LabGameplayTags::Effect_HitReaction);
		UE_LOG(LogPdAttributeSet, Log, TEXT("IncomingDamage applied: owner=%s healthAfter=%.3f shouldHitReact=%s"),
			*GetNameSafe(GetOwningActor()),
			GetHealth(),
			bShouldHitReact ? TEXT("true") : TEXT("false"));
		if (bShouldHitReact && GetHealth() > 0.f)
		{
			TryActivateHitReactionAbility(GetOwningAbilitySystemComponent());
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		bool bStatusDamage = false;
		ResolveStatusResistance(this, Data.EffectSpec, bStatusDamage);
		const bool bShouldHitReact =
			Data.EvaluatedData.Magnitude < 0.f
			&& !bStatusDamage
			&& EffectSpecHasAssetTag(Data.EffectSpec, LabGameplayTags::Effect_HitReaction);
		UE_LOG(LogPdAttributeSet, Log, TEXT("Health effect executed: owner=%s effect=%s magnitude=%.3f hasHitReactTag=%s health=%.3f"),
			*GetNameSafe(GetOwningActor()),
			*GetNameSafe(Data.EffectSpec.Def),
			Data.EvaluatedData.Magnitude,
			bShouldHitReact ? TEXT("true") : TEXT("false"),
			GetHealth());
		SetHealth(GetHealth());
		if (bShouldHitReact && GetHealth() > 0.f)
		{
			TryActivateHitReactionAbility(GetOwningAbilitySystemComponent());
		}
		return;
	}

	if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(GetStamina());
		return;
	}

	if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		SetMana(GetMana());
		return;
	}
}

/** Attribute 값이 변경되기 전에 보정합니다. */
void UBasicAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	// =================================================================================================================
	// === 체력 값 사전 보정

	// Health는 항상 0 이상 MaxHealth 이하로 유지합니다.
	if (Attribute == GetHealthAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxHealth());
	}
	else if (Attribute == GetShieldAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxShield());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxStamina());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = ClampResourceAttribute(NewValue, GetMaxMana());
	}
	else if (Attribute == GetBurnAttribute()
		|| Attribute == GetFrostbiteAttribute()
		|| Attribute == GetElectricShockAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxShieldAttribute()
		|| Attribute == GetMaxStaminaAttribute() || Attribute == GetMaxManaAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}

/** Attribute 값이 변경된 직후 후처리를 수행합니다. */
void UBasicAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	
	// =================================================================================================================
	// === Push Model 복제 dirty 마킹

	if (OldValue != NewValue)
	{
		UE_LOG(LogPdAttributeSet, Log, TEXT("[StatUpgrade] Attribute changed: owner=%s attribute=%s old=%.3f new=%.3f"),
			*GetNameSafe(GetOwningActor()),
			*Attribute.GetName(),
			OldValue,
			NewValue);

		if (FProperty* Property = Attribute.GetUProperty())
		{
			MARK_PROPERTY_DIRTY(this, Property);
		}
	}
	
	// =================================================================================================================
	// === 체력 0 이하 도달 시 사망 처리 지점

	if (Attribute == GetMaxHealthAttribute())
	{
		SetHealth(GetHealth());
	}
	else if (Attribute == GetMaxShieldAttribute())
	{
		SetShield(GetShield());
	}
	else if (Attribute == GetMaxStaminaAttribute())
	{
		SetStamina(GetStamina());
	}
	else if (Attribute == GetMaxManaAttribute())
	{
		SetMana(GetMana());
	}

	if (Attribute == GetHealthAttribute() && NewValue <= 0.f)
	{
		TryActivateDeathAbility(GetOwningAbilitySystemComponent(), TEXT("PostAttributeChange.Health"), OldValue, NewValue);
	}

	if (Attribute == GetShieldAttribute())
	{
		if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
		{
			if (AActor* OwnerActor = ASC->GetOwnerActor(); OwnerActor && OwnerActor->HasAuthority())
			{
				if (OldValue <= 0.f && NewValue > 0.f)
				{
					ASC->AddGameplayCue(LabGameplayTags::GameplayCue_ShieldUp);
				}
				else if (OldValue > 0.f && NewValue <= 0.f)
				{
					ASC->RemoveGameplayCue(LabGameplayTags::GameplayCue_ShieldUp);
					ASC->ExecuteGameplayCue(LabGameplayTags::GameplayCue_ShieldDown);
				}
			}
		}
	}
}

float UBasicAttributeSet::ConsumeOutgoingDamage()
{
	const float ConsumedOutgoingDamage = GetOutgoingDamage();
	SetOutgoingDamage(0.f);
	return ConsumedOutgoingDamage;
}

bool UBasicAttributeSet::ConsumeOutgoingDamageCriticalHit()
{
	const bool bConsumedCriticalHit = bLastOutgoingDamageCriticalHit;
	bLastOutgoingDamageCriticalHit = false;
	return bConsumedCriticalHit;
}

void UBasicAttributeSet::SetPendingIncomingDamageCriticalHit(bool bCriticalHit)
{
	bPendingIncomingDamageCriticalHit = bCriticalHit;
}

float UBasicAttributeSet::ApplyIncomingDamage(float IncomingDamageAmount, bool bCriticalHit)
{
	const float FinalDamage = FMath::Max(IncomingDamageAmount, 0.f);

	UE_LOG(LogPdAttributeSet, Log, TEXT("IncomingDamage received: owner=%s final=%.3f shieldBefore=%.3f healthBefore=%.3f"),
		*GetNameSafe(GetOwningActor()),
		FinalDamage,
		GetShield(),
		GetHealth());

	if (FinalDamage <= 0.f)
	{
		return 0.f;
	}

	float RemainingHealthDamage = FinalDamage;
	const float CurrentShield = GetShield();
	if (CurrentShield > 0.f)
	{
		SetShield(CurrentShield - FinalDamage);
		RemainingHealthDamage = FMath::Max(FinalDamage - CurrentShield, 0.f);
	}

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (APdCharacterBase* Character = Cast<APdCharacterBase>(ASC->GetAvatarActor()))
		{
			Character->HandleDamageTaken(RemainingHealthDamage, bCriticalHit);
		}
	}

	if (RemainingHealthDamage <= 0.f)
	{
		UE_LOG(LogPdAttributeSet, Log, TEXT("IncomingDamage absorbed by shield: owner=%s final=%.3f shieldBefore=%.3f shieldAfter=%.3f"),
			*GetNameSafe(GetOwningActor()),
			FinalDamage,
			CurrentShield,
			GetShield());
		return 0.f;
	}

	const float OldHealth = GetHealth();
	const float NewHealth = FMath::Max(OldHealth - RemainingHealthDamage, 0.f);
	SetHealth(NewHealth);
	if (NewHealth <= 0.f)
	{
		TryActivateDeathAbility(GetOwningAbilitySystemComponent(), TEXT("ApplyIncomingDamage"), OldHealth, NewHealth);
	}

	return RemainingHealthDamage;
}

void UBasicAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Strength, OldValue);
}

void UBasicAttributeSet::OnRep_Intelligence(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Intelligence, OldValue);
}

void UBasicAttributeSet::OnRep_Arcane(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Arcane, OldValue);
}

void UBasicAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Armor, OldValue);
}

void UBasicAttributeSet::OnRep_Recovery(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Recovery, OldValue);
}

void UBasicAttributeSet::OnRep_MagicResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MagicResistance, OldValue);
}

void UBasicAttributeSet::OnRep_Immunity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Immunity, OldValue);
}

void UBasicAttributeSet::OnRep_Fortitude(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Fortitude, OldValue);
}

void UBasicAttributeSet::OnRep_Sanity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Sanity, OldValue);
}

void UBasicAttributeSet::OnRep_Burn(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Burn, OldValue);
}

void UBasicAttributeSet::OnRep_Frostbite(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Frostbite, OldValue);
}

void UBasicAttributeSet::OnRep_ElectricShock(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ElectricShock, OldValue);
}

void UBasicAttributeSet::OnRep_FirstPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, FirstPandora, OldValue);
}

void UBasicAttributeSet::OnRep_SecondPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, SecondPandora, OldValue);
}

void UBasicAttributeSet::OnRep_ThirdPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, ThirdPandora, OldValue);
}

void UBasicAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, AttackSpeed, OldValue);
}

void UBasicAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MovementSpeed, OldValue);
}

void UBasicAttributeSet::OnRep_CriticalChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, CriticalChance, OldValue);
}

void UBasicAttributeSet::OnRep_CriticalDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, CriticalDamageMultiplier, OldValue);
}

void UBasicAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Health, OldValue);
}

void UBasicAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxHealth, OldValue);
}

void UBasicAttributeSet::OnRep_Shield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Shield, OldValue);
}

void UBasicAttributeSet::OnRep_MaxShield(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxShield, OldValue);
}

void UBasicAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Mana, OldValue);
}

void UBasicAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxMana, OldValue);
}

void UBasicAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, Stamina, OldValue);
}

void UBasicAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBasicAttributeSet, MaxStamina, OldValue);
}
