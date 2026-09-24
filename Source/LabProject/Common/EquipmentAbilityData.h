#pragma once

#include "CoreMinimal.h"

class UAnimInstance;
class UAnimMontage;
class UItemDefinition;

struct FEquipData
{

public:
	const UItemDefinition* ItemDefinition = nullptr;

	UAnimMontage* EquipMontage = nullptr;

	TSubclassOf<UAnimInstance> EquipAnimLayer;

	// Public API ------------------------------------------------------------------------------------------------------
	bool IsValid() const
	{
		return ItemDefinition != nullptr && EquipMontage != nullptr;
	}
};

struct FUnequipData
{

public:
	const UItemDefinition* ItemDefinition = nullptr;

	UAnimMontage* UnequipMontage = nullptr;

	// Public API ------------------------------------------------------------------------------------------------------
	bool IsValid() const
	{
		return ItemDefinition != nullptr && UnequipMontage != nullptr;
	}
};

struct FAttackData
{

public:
	const UItemDefinition* ItemDefinition = nullptr;

	UAnimMontage* AttackMontage = nullptr;

	// Public API ------------------------------------------------------------------------------------------------------
	bool IsValid() const
	{
		return ItemDefinition != nullptr && AttackMontage != nullptr;
	}
};

struct FHitReactData
{

public:
	const UItemDefinition* ItemDefinition = nullptr;

	UAnimMontage* HitReactMontage = nullptr;

	// Public API ------------------------------------------------------------------------------------------------------
	bool IsValid() const
	{
		return ItemDefinition != nullptr && HitReactMontage != nullptr;
	}
};
