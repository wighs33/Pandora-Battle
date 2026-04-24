#include "PdAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Mode/PdCharacterBase.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdAttributeSet)

UPdAttributeSet::UPdAttributeSet()
{
}

void UPdAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Strength, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Intelligence, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Arcane, Params);

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Toughness, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Recovery, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MagicResistance, Params);

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Immunity, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Fortitude, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Sanity, Params);

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, FirstPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, SecondPandora, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, ThirdPandora, Params);

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, AttackSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MovementSpeed, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, CriticalChance, Params);

	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Health, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MaxHealth, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Mana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MaxMana, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, Stamina, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UPdAttributeSet, MaxStamina, Params);
}

void UPdAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetDamageAttribute())
	{
		const float IncomingDamage = GetDamage();
		SetDamage(0.f);

		if (IncomingDamage > 0.f)
		{
			float RemainingDamage = IncomingDamage;
			const float CurrentToughness = GetToughness();

			if (CurrentToughness > 0.f)
			{
				// constexpr float ArmorAbsorptionRate = 0.5f;
				// const float DamageToArmor = FMath::Min(IncomingDamage * ArmorAbsorptionRate, CurrentToughness);
				//
				// SetToughness(CurrentToughness - DamageToArmor);
				// RemainingDamage = IncomingDamage - DamageToArmor;
			}

			if (RemainingDamage > 0.f)
			{
				// const float NewHealth = FMath::Max(GetHealth() - RemainingDamage, 0.f);
				// SetHealth(NewHealth);
			}
		}
	}
}

void UPdAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void UPdAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (OldValue != NewValue)
	{
		if (FProperty* Property = Attribute.GetUProperty())
		{
			MARK_PROPERTY_DIRTY(this, Property);
		}
	}

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
