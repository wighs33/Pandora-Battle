#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "EquipmentComponent.generated.h"

class ACharacter;
class AWeaponBase;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;
class UInventoryComponent;
class UItemDefinition;
class UItemInstance;

DECLARE_LOG_CATEGORY_EXTERN(EquipmentComponentLog, Log, All);

struct FEquipData
{
	const UItemDefinition* ItemDefinition = nullptr;
	UAnimMontage* EquipMontage = nullptr;
	TSubclassOf<UAnimInstance> EquipAnimLayer;
	TSubclassOf<UGameplayEffect> EquipEffectClass;
	FGameplayTag EquipCueTag;

	bool IsValid() const
	{
		return ItemDefinition != nullptr && EquipMontage != nullptr;
	}
};

struct FUnequipData
{
	const UItemDefinition* ItemDefinition = nullptr;
	UAnimMontage* UnequipMontage = nullptr;
	TSubclassOf<UGameplayEffect> UnequipEffectClass;

	bool IsValid() const
	{
		return ItemDefinition != nullptr && UnequipMontage != nullptr;
	}
};

struct FAttackData
{
	const UItemDefinition* ItemDefinition = nullptr;
	UAnimMontage* AttackMontage = nullptr;
	TSubclassOf<UGameplayEffect> AttackEffectClass;

	bool IsValid() const
	{
		return ItemDefinition != nullptr && AttackMontage != nullptr;
	}
};

/**
 * 캐릭터의 장비 장착 상태를 관리하는 컴포넌트입니다.
 *
 * - 인벤토리의 아이템 요청을 실제 장착/해제 동작으로 확정합니다.
 * - 장착된 무기 액터를 생성, 부착, 제거하고 현재 장착 아이템 ID를 관리합니다.
 * - 장착 완료 상태 GE는 어빌리티가 노티파이 이벤트를 받아 적용/제거하며, 이 컴포넌트는 실제 장비 상태의 최종 소유자가 됩니다.
 *
 * 주요 사용 흐름:
 * - 장착 시작: SetRequestedItemInstance()로 장착할 아이템을 요청 상태로 저장합니다.
 * - 장착 데이터 조회: EquipAbility가 GetEquipData()로 몽타주, 애님 레이어, 임시 GE, Cue 태그를 가져옵니다.
 * - 장착 확정: 장착 몽타주의 AnimNotify에서 EquipRequestedItem()을 호출해 실제 무기 액터를 생성하고 부착합니다.
 * - 직접 장착: 몽타주 없이 즉시 장착해야 할 때는 EquipItem()을 호출합니다.
 * - 해제 데이터 조회: UnequipAbility가 GetUnequipData()로 해제 몽타주와 임시 GE를 가져옵니다.
 * - 해제 확정: 해제 몽타주의 AnimNotify에서 UnequipCurrentItem()을 호출해 실제 무기 제거와 상태 초기화를 수행합니다.
 * - 장착 완료 GE: EquipRequestedItem()/EquipItem() 성공 후 노티파이 이벤트를 받은 EquipAbility가 적용하고, UnequipAbility가 제거합니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEquipmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const UItemDefinition* GetRequestedItemDefinition() const;
	bool GetEquipData(FEquipData& OutEquipData) const;
	bool GetUnequipData(FUnequipData& OutUnequipData) const;
	bool GetAttackData(FAttackData& OutAttackData) const;

	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool SetRequestedItemInstance(UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool EquipRequestedItem();
	
	// 아이템 인스턴스를 즉시 장착 요청으로 처리합니다.
	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool EquipItem(UItemInstance* ItemInstance);

	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool UnequipCurrentItem();

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetRequestedItem(FGuid ItemId);

	UFUNCTION(Server, Reliable)
	void ServerEquipItem(FGuid ItemId);

	bool EquipItemById(FGuid ItemId);
	bool EquipItemInternal(UItemInstance* ItemInstance);
	bool UnequipCurrentItemInternal();
	void ClearRequestedItem();
	const UItemDefinition* GetCurrentItemDefinition() const;

	UItemInstance* FindOwnedItemInstanceById(FGuid ItemId) const;
	UInventoryComponent* FindInventoryComponent() const;
	ACharacter* GetCharacterOwner() const;
	void AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const;

protected:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	TObjectPtr<AWeaponBase> CurrentWeaponActor;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid CurrentItemId;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid RequestedItemId;
};
