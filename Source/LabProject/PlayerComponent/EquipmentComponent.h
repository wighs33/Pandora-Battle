#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "EquipmentComponent.generated.h"

class ACharacter;
class AWeaponBase;
class UAnimInstance;
class UAnimMontage;
class UInventoryComponent;
class UItemDefinition;
class UItemInstance;
class UPdAbilitySystemComponent;

/** 장비 컴포넌트 로그 카테고리입니다. */
DECLARE_LOG_CATEGORY_EXTERN(EquipmentComponentLog, Log, All);

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

	/** 장착 큐 태그입니다. */

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
 * <장착 스탯 스냅샷>
 * - 장착 중인 아이템 스탯을 저장합니다.
 * - 장착 시 적용하고 해제 시 되돌립니다.
 */
USTRUCT(BlueprintType)
struct FEquippedItemStatSnapshot
{
	GENERATED_BODY()

	/** 기본 스탯 값 맵입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	TMap<FGameplayTag, float> BaseStatMagnitudes;

	/** 강화 스탯 값 맵입니다. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	TMap<FGameplayTag, float> EnhancedStatMagnitudes;

	/** 스냅샷을 초기화합니다. */
	void Reset()
	{
		BaseStatMagnitudes.Reset();
		EnhancedStatMagnitudes.Reset();
	}

	/** 값 존재 여부를 반환합니다. */
	bool HasAnyMagnitude() const
	{
		return !BaseStatMagnitudes.IsEmpty() || !EnhancedStatMagnitudes.IsEmpty();
	}
};

/**
 * <장비 상태 관리 컴포넌트>
 * - 장착 요청과 해제를 처리합니다.
 * - 무기 액터를 생성, 부착, 제거합니다.
 * - 장착 스탯 적용과 해제를 처리합니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 장비 컴포넌트 기본 상태를 초기화합니다. */
	UEquipmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 복제 프로퍼티를 등록합니다. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 요청된 아이템 정의를 반환합니다. */
	const UItemDefinition* GetRequestedItemDefinition() const;

	/** 장착 데이터를 반환합니다. */
	bool GetEquipData(FEquipData& OutEquipData) const;

	/** 장착 해제 데이터를 반환합니다. */
	bool GetUnequipData(FUnequipData& OutUnequipData) const;

	/** 공격 데이터를 반환합니다. */
	bool GetAttackData(FAttackData& OutAttackData) const;
	UAnimMontage* GetCurrentHitReactMontage() const;


	/** 현재 무기 액터를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!Equipment")
	AWeaponBase* GetCurrentWeaponActor() const { return CurrentWeaponActor; }

	/** 요청 아이템을 저장합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool SetRequestedItemInstance(UItemInstance* ItemInstance);

	/** 요청된 아이템을 장착합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool EquipRequestedItem();
	
	/** 아이템을 즉시 장착합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool EquipItem(UItemInstance* ItemInstance);

	/** 현재 아이템을 해제합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool UnequipCurrentItem();

protected:
	/** 서버에 요청 아이템 ID를 전달합니다. */
	UFUNCTION(Server, Reliable)
	void ServerSetRequestedItem(FGuid ItemId);

	/** 서버에 장착 아이템 ID를 전달합니다. */
	UFUNCTION(Server, Reliable)
	void ServerEquipItem(FGuid ItemId);

	/** ID 기준으로 장착을 처리합니다. */
	bool EquipItemById(FGuid ItemId);

	/** 실제 장착 로직을 처리합니다. */
	bool EquipItemInternal(UItemInstance* ItemInstance);

	/** 실제 장착 해제 로직을 처리합니다. */
	bool UnequipCurrentItemInternal();

	/** 요청 아이템을 초기화합니다. */
	void ClearRequestedItem();

	/** 현재 아이템 정의를 반환합니다. */
	const UItemDefinition* GetCurrentItemDefinition() const;

	/** 아이템 스탯 스냅샷을 구성합니다. */
	bool BuildItemStatSnapshot(const UItemInstance* ItemInstance, FEquippedItemStatSnapshot& OutSnapshot) const;

	/** 스탯 스냅샷을 적용합니다. */
	bool ApplyItemStatSnapshot(const FEquippedItemStatSnapshot& StatSnapshot, float MagnitudeScale) const;

	/** 소유 인벤토리에서 아이템 인스턴스를 찾습니다. */
	UItemInstance* FindOwnedItemInstanceById(FGuid ItemId) const;

	/** 소유 인벤토리 컴포넌트를 반환합니다. */
	UInventoryComponent* FindInventoryComponent() const;

	/** 소유 ASC를 반환합니다. */
	UPdAbilitySystemComponent* FindOwnerPdAbilitySystemComponent() const;

	/** 소유 캐릭터를 반환합니다. */
	ACharacter* GetCharacterOwner() const;

	/** 무기를 소유자 메시에 부착합니다. */
	void AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const;

protected:
	/** 현재 무기 액터입니다. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	TObjectPtr<AWeaponBase> CurrentWeaponActor;


	/** 현재 장착 아이템 ID입니다. */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid CurrentItemId;

	/** 요청된 아이템 ID입니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid RequestedItemId;

	/** 현재 장착 스탯 스냅샷입니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	FEquippedItemStatSnapshot CurrentEquippedItemStatSnapshot;

	/** 현재 스탯 스냅샷 적용 여부입니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	bool bHasAppliedCurrentEquippedItemStatSnapshot = false;
};
