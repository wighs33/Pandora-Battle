#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BasicAttributeSet.generated.h"

// Attribute 프로퍼티에 대한 Getter / Setter / Init 함수를 한 번에 생성합니다.
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * <프로젝트 전용 AttributeSet>
 * - 캐릭터의 전투, 저항, 자원, 판도라 관련 스탯을 보관합니다.
 * - 각 Attribute는 GAS 방식으로 접근할 수 있도록 접근자 매크로를 함께 선언합니다.
 * - 복제, 값 변경 전후 처리, GameplayEffect 적용 후처리를 담당합니다.
 */
UCLASS()
class LABPROJECT_API UBasicAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UBasicAttributeSet();

	// Timing hooks
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	// Public API
	float ConsumeOutgoingDamage();
	bool ConsumeOutgoingDamageCriticalHit();
	void SetPendingIncomingDamageCriticalHit(bool bCriticalHit);

protected:
	// Replication callbacks
	UFUNCTION()
	void OnRep_Strength(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Intelligence(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Arcane(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Recovery(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MagicResistance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Immunity(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Fortitude(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Sanity(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Burn(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Frostbite(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ElectricShock(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_FirstPandora(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_SecondPandora(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ThirdPandora(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_AttackSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MovementSpeed(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_CriticalChance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_CriticalDamageMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Shield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxShield(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	float ApplyIncomingDamage(float IncomingDamageAmount, bool bCriticalHit);

private:
	bool bLastOutgoingDamageCriticalHit = false;
	bool bPendingIncomingDamageCriticalHit = false;

public:
	UPROPERTY(BlueprintReadOnly, Category = "!Offense", ReplicatedUsing = OnRep_Strength)
	FGameplayAttributeData Strength = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Strength)

	UPROPERTY(BlueprintReadOnly, Category = "!Offense", ReplicatedUsing = OnRep_Intelligence)
	FGameplayAttributeData Intelligence = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Intelligence)

	UPROPERTY(BlueprintReadOnly, Category = "!Offense", ReplicatedUsing = OnRep_Arcane)
	FGameplayAttributeData Arcane = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Arcane)

	UPROPERTY(BlueprintReadOnly, Category = "!Defense", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Armor)

	UPROPERTY(BlueprintReadOnly, Category = "!Defense", ReplicatedUsing = OnRep_Recovery)
	FGameplayAttributeData Recovery = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Recovery)

	UPROPERTY(BlueprintReadOnly, Category = "!Defense", ReplicatedUsing = OnRep_MagicResistance)
	FGameplayAttributeData MagicResistance = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, MagicResistance)

	UPROPERTY(BlueprintReadOnly, Category = "!Resistance", ReplicatedUsing = OnRep_Immunity)
	FGameplayAttributeData Immunity = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Immunity)

	UPROPERTY(BlueprintReadOnly, Category = "!Resistance", ReplicatedUsing = OnRep_Fortitude)
	FGameplayAttributeData Fortitude = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Fortitude)

	UPROPERTY(BlueprintReadOnly, Category = "!Resistance", ReplicatedUsing = OnRep_Sanity)
	FGameplayAttributeData Sanity = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Sanity)

	UPROPERTY(BlueprintReadOnly, Category = "!Resistance", ReplicatedUsing = OnRep_Burn)
	FGameplayAttributeData Burn = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Burn)

	UPROPERTY(BlueprintReadOnly, Category = "!Resistance", ReplicatedUsing = OnRep_Frostbite)
	FGameplayAttributeData Frostbite = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Frostbite)

	UPROPERTY(BlueprintReadOnly, Category = "!Resistance", ReplicatedUsing = OnRep_ElectricShock)
	FGameplayAttributeData ElectricShock = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, ElectricShock)

	UPROPERTY(BlueprintReadOnly, Category = "!PandoraForce", ReplicatedUsing = OnRep_FirstPandora)
	FGameplayAttributeData FirstPandora = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, FirstPandora)

	UPROPERTY(BlueprintReadOnly, Category = "!PandoraForce", ReplicatedUsing = OnRep_SecondPandora)
	FGameplayAttributeData SecondPandora = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, SecondPandora)

	UPROPERTY(BlueprintReadOnly, Category = "!PandoraForce", ReplicatedUsing = OnRep_ThirdPandora)
	FGameplayAttributeData ThirdPandora = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, ThirdPandora)

	UPROPERTY(BlueprintReadOnly, Category = "!Offense", ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, AttackSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "!Offense", ReplicatedUsing = OnRep_MovementSpeed)
	FGameplayAttributeData MovementSpeed = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, MovementSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "!Offense", ReplicatedUsing = OnRep_CriticalChance)
	FGameplayAttributeData CriticalChance = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, CriticalChance)

	UPROPERTY(BlueprintReadOnly, Category = "!Offense", ReplicatedUsing = OnRep_CriticalDamageMultiplier)
	FGameplayAttributeData CriticalDamageMultiplier = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, CriticalDamageMultiplier)

	UPROPERTY(BlueprintReadOnly, Category = "!Resource", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "!Resource", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "!Resource", ReplicatedUsing = OnRep_Shield)
	FGameplayAttributeData Shield = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Shield)

	UPROPERTY(BlueprintReadOnly, Category = "!Resource", ReplicatedUsing = OnRep_MaxShield)
	FGameplayAttributeData MaxShield = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, MaxShield)

	UPROPERTY(BlueprintReadOnly, Category = "!Resource", ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Mana)

	UPROPERTY(BlueprintReadOnly, Category = "!Resource", ReplicatedUsing = OnRep_MaxMana)
	FGameplayAttributeData MaxMana = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, MaxMana)

	UPROPERTY(BlueprintReadOnly, Category = "!Resource", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, Category = "!Resource", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, MaxStamina)

	UPROPERTY(BlueprintReadWrite, Category = "!Damage")
	FGameplayAttributeData OutgoingDamage = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, OutgoingDamage)

	UPROPERTY(BlueprintReadWrite, Category = "!Damage")
	FGameplayAttributeData IncomingDamage = 0.f;
	ATTRIBUTE_ACCESSORS(UBasicAttributeSet, IncomingDamage)
};
