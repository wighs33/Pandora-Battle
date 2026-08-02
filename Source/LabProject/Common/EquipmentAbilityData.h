#pragma once

#include "CoreMinimal.h"

class UAnimInstance;
class UAnimMontage;
class UItemDefinition;

struct FEquipData
{
	const UItemDefinition* ItemDefinition = nullptr;

	UAnimMontage* EquipMontage = nullptr;

	TSubclassOf<UAnimInstance> EquipAnimLayer;

	bool IsValid() const
	{
		return ItemDefinition != nullptr && EquipMontage != nullptr;
	}
};

struct FUnequipData
{
	const UItemDefinition* ItemDefinition = nullptr;

	UAnimMontage* UnequipMontage = nullptr;

	bool IsValid() const
	{
		return ItemDefinition != nullptr && UnequipMontage != nullptr;
	}
};

struct FAttackData
{
	const UItemDefinition* ItemDefinition = nullptr;

	UAnimMontage* AttackMontage = nullptr;

	bool IsValid() const
	{
		return ItemDefinition != nullptr && AttackMontage != nullptr;
	}
};

struct FHitReactData
{
	const UItemDefinition* ItemDefinition = nullptr;

	UAnimMontage* HitReactMontage = nullptr;

	bool IsValid() const
	{
		return ItemDefinition != nullptr && HitReactMontage != nullptr;
	}
};
