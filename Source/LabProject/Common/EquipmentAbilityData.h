#pragma once

#include "CoreMinimal.h"

class UAnimInstance;
class UAnimMontage;
class UItemDefinition;

/**
 * <장착 데이터>
 * - 장착 연출에 필요한 데이터입니다.
 * - 장착 어빌리티가 사용합니다.
 */
struct FEquipData
{
	/** 장착 대상 아이템 정의입니다. */
	const UItemDefinition* ItemDefinition = nullptr;

	/** 장착 몽타주입니다. */
	UAnimMontage* EquipMontage = nullptr;

	/** 장착 애님 레이어입니다. */
	TSubclassOf<UAnimInstance> EquipAnimLayer;

	/** 데이터 유효성을 반환합니다. */
	bool IsValid() const
	{
		return ItemDefinition != nullptr && EquipMontage != nullptr;
	}
};

/**
 * <장착 해제 데이터>
 * - 장착 해제 연출에 필요한 데이터입니다.
 * - 장착 해제 어빌리티가 사용합니다.
 */
struct FUnequipData
{
	/** 장착 해제 대상 아이템 정의입니다. */
	const UItemDefinition* ItemDefinition = nullptr;

	/** 장착 해제 몽타주입니다. */
	UAnimMontage* UnequipMontage = nullptr;

	/** 데이터 유효성을 반환합니다. */
	bool IsValid() const
	{
		return ItemDefinition != nullptr && UnequipMontage != nullptr;
	}
};

/**
 * <공격 데이터>
 * - 공격 연출에 필요한 데이터입니다.
 * - 공격 어빌리티가 사용합니다.
 */
struct FAttackData
{
	/** 현재 공격 아이템 정의입니다. */
	const UItemDefinition* ItemDefinition = nullptr;

	/** 공격 몽타주입니다. */
	UAnimMontage* AttackMontage = nullptr;

	/** 데이터 유효성을 반환합니다. */
	bool IsValid() const
	{
		return ItemDefinition != nullptr && AttackMontage != nullptr;
	}
};

/**
 * <피격 리액션 데이터>
 * - 피격 리액션 연출에 필요한 장착 무기 데이터를 저장합니다.
 * - 피격 리액션 어빌리티가 사용합니다.
 */
struct FHitReactData
{
	/** 현재 피격 리액션 대상 아이템 정의입니다. */
	const UItemDefinition* ItemDefinition = nullptr;

	/** 피격 리액션 몽타주입니다. */
	UAnimMontage* HitReactMontage = nullptr;

	/** 데이터 유효성을 반환합니다. */
	bool IsValid() const
	{
		return ItemDefinition != nullptr && HitReactMontage != nullptr;
	}
};
