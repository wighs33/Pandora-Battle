#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PdAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class LABPROJECT_API UPdAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPdAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

protected:
	UFUNCTION()
	void OnRep_Strength(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Intelligence(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Arcane(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Toughness(const FGameplayAttributeData& OldValue);

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
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Offense", ReplicatedUsing = OnRep_Strength)
	FGameplayAttributeData Strength = 20.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Strength)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Offense", ReplicatedUsing = OnRep_Intelligence)
	FGameplayAttributeData Intelligence = 20.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Intelligence)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Offense", ReplicatedUsing = OnRep_Arcane)
	FGameplayAttributeData Arcane = 20.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Arcane)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Defense", ReplicatedUsing = OnRep_Toughness)
	FGameplayAttributeData Toughness = 5.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Toughness)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Defense", ReplicatedUsing = OnRep_Recovery)
	FGameplayAttributeData Recovery = 5.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Recovery)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Defense", ReplicatedUsing = OnRep_MagicResistance)
	FGameplayAttributeData MagicResistance = 5.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, MagicResistance)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Resistance", ReplicatedUsing = OnRep_Immunity)
	FGameplayAttributeData Immunity = 0.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Immunity)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Resistance", ReplicatedUsing = OnRep_Fortitude)
	FGameplayAttributeData Fortitude = 0.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Fortitude)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Resistance", ReplicatedUsing = OnRep_Sanity)
	FGameplayAttributeData Sanity = 0.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Sanity)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!PandoraForce", ReplicatedUsing = OnRep_FirstPandora)
	FGameplayAttributeData FirstPandora = 1.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, FirstPandora)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!PandoraForce", ReplicatedUsing = OnRep_SecondPandora)
	FGameplayAttributeData SecondPandora = 1.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, SecondPandora)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!PandoraForce", ReplicatedUsing = OnRep_ThirdPandora)
	FGameplayAttributeData ThirdPandora = 1.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, ThirdPandora)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Offense", ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed = 0.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, AttackSpeed)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Offense", ReplicatedUsing = OnRep_MovementSpeed)
	FGameplayAttributeData MovementSpeed = 0.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, MovementSpeed)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Offense", ReplicatedUsing = OnRep_CriticalChance)
	FGameplayAttributeData CriticalChance = 0.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, CriticalChance)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Resource", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health = 100.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Health)

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Resource", ReplicatedUsing = OnRep_MaxHealth, meta = (ShowOnlyInnerProperties))
	FGameplayAttributeData MaxHealth = 100.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, MaxHealth)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Resource", ReplicatedUsing = OnRep_Mana)
	FGameplayAttributeData Mana = 100.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Mana)

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Resource", ReplicatedUsing = OnRep_MaxMana, meta = (ShowOnlyInnerProperties))
	FGameplayAttributeData MaxMana = 100.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, MaxMana)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Resource", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina = 100.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Stamina)

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "!Resource", ReplicatedUsing = OnRep_MaxStamina, meta = (ShowOnlyInnerProperties))
	FGameplayAttributeData MaxStamina = 100.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, MaxStamina)

	UPROPERTY(BlueprintReadWrite, Category = "!Damage")
	FGameplayAttributeData Damage = 0.f;
	ATTRIBUTE_ACCESSORS(UPdAttributeSet, Damage)
};
