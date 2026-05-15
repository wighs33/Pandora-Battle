#include "PdAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Character/PdCharacterBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAttributeSet)

DEFINE_LOG_CATEGORY_STATIC(LogPdAttributeSet, Log, All);

namespace
{
	bool RollPercentChance(float PercentChance)
	{
		const float ClampedPercentChance = FMath::Clamp(PercentChance, 0.f, 100.f);
		return ClampedPercentChance >= 100.f
			|| (ClampedPercentChance > 0.f && FMath::FRandRange(0.f, 100.f) < ClampedPercentChance);
	}
}

UPdAttributeSet::UPdAttributeSet()
{
}

/** 복제할 Attribute 프로퍼티를 등록합니다. */
void UPdAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// =================================================================================================================
	// === Push Model 복제 파라미터 설정

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	// =================================================================================================================
	// === 기본 공격 스탯 복제 등록
	
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Strength, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Intelligence, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Arcane, Params);
	
	// =================================================================================================================
	// === 기본 방어 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Toughness, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Recovery, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MagicResistance, Params);
	
	// =================================================================================================================
	// === 상태 이상 저항 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Immunity, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Fortitude, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Sanity, Params);
	
	// =================================================================================================================
	// === 판도라 관련 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, FirstPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, SecondPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, ThirdPandora, Params);
	
	// =================================================================================================================
	// === 전투 보조 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, AttackSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MovementSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, CriticalChance, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, CriticalDamageMultiplier, Params);
	
	// =================================================================================================================
	// === 자원 스탯 복제 등록

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Health, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MaxHealth, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Mana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MaxMana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Stamina, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MaxStamina, Params);
}

/** GameplayEffect 적용 후 최종 결과를 처리합니다. */
void UPdAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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
		bPendingIncomingDamageCriticalHit = false;
		SetIncomingDamage(0.f);
		ApplyIncomingDamage(AppliedIncomingDamage, bCriticalHit);
		return;
	}

}

/** Attribute 값이 변경되기 전에 보정합니다. */
void UPdAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	// =================================================================================================================
	// === 체력 값 사전 보정

	// Health는 항상 0 이상 MaxHealth 이하로 유지합니다.
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

/** Attribute 값이 변경된 직후 후처리를 수행합니다. */
void UPdAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
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

	if (Attribute == GetHealthAttribute() && NewValue <= 0.f && OldValue > 0.f)
	{
		if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
		{
			if (APdCharacterBase* Character = Cast<APdCharacterBase>(ASC->GetAvatarActor()))
			{
				// Character->HandleDeathAuth();
			}
		}
	}
}

float UPdAttributeSet::ConsumeOutgoingDamage()
{
	const float ConsumedOutgoingDamage = GetOutgoingDamage();
	SetOutgoingDamage(0.f);
	return ConsumedOutgoingDamage;
}

bool UPdAttributeSet::ConsumeOutgoingDamageCriticalHit()
{
	const bool bConsumedCriticalHit = bLastOutgoingDamageCriticalHit;
	bLastOutgoingDamageCriticalHit = false;
	return bConsumedCriticalHit;
}

void UPdAttributeSet::SetPendingIncomingDamageCriticalHit(bool bCriticalHit)
{
	bPendingIncomingDamageCriticalHit = bCriticalHit;
}

void UPdAttributeSet::ApplyIncomingDamage(float IncomingDamageAmount, bool bCriticalHit)
{
	const float FinalDamage = FMath::Max(IncomingDamageAmount - GetToughness(), 0.f);
	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		if (APdCharacterBase* Character = Cast<APdCharacterBase>(ASC->GetAvatarActor()))
		{
			Character->HandleDamageTaken(FinalDamage, bCriticalHit);
		}
	}

	if (FinalDamage <= 0.f)
	{
		return;
	}

	const float NewHealth = FMath::Max(GetHealth() - FinalDamage, 0.f);
	SetHealth(NewHealth);
}

void UPdAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Strength, OldValue);
}

void UPdAttributeSet::OnRep_Intelligence(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Intelligence, OldValue);
}

void UPdAttributeSet::OnRep_Arcane(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Arcane, OldValue);
}

void UPdAttributeSet::OnRep_Toughness(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Toughness, OldValue);
}

void UPdAttributeSet::OnRep_Recovery(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Recovery, OldValue);
}

void UPdAttributeSet::OnRep_MagicResistance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, MagicResistance, OldValue);
}

void UPdAttributeSet::OnRep_Immunity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Immunity, OldValue);
}

void UPdAttributeSet::OnRep_Fortitude(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Fortitude, OldValue);
}

void UPdAttributeSet::OnRep_Sanity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Sanity, OldValue);
}

void UPdAttributeSet::OnRep_FirstPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, FirstPandora, OldValue);
}

void UPdAttributeSet::OnRep_SecondPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, SecondPandora, OldValue);
}

void UPdAttributeSet::OnRep_ThirdPandora(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, ThirdPandora, OldValue);
}

void UPdAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, AttackSpeed, OldValue);
}

void UPdAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, MovementSpeed, OldValue);
}

void UPdAttributeSet::OnRep_CriticalChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, CriticalChance, OldValue);
}

void UPdAttributeSet::OnRep_CriticalDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, CriticalDamageMultiplier, OldValue);
}

void UPdAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Health, OldValue);
}

void UPdAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, MaxHealth, OldValue);
}

void UPdAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Mana, OldValue);
}

void UPdAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, MaxMana, OldValue);
}

void UPdAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, Stamina, OldValue);
}

void UPdAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UPdAttributeSet, MaxStamina, OldValue);
}
