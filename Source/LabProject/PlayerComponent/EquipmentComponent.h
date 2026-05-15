#pragma once

#include "CoreMinimal.h"
#include "Common/EquipmentAbilityData.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "EquipmentComponent.generated.h"

class AWeaponBase;
class APdCharacterBase;
class UInventoryComponent;
class UItemDefinition;
class UItemInstance;
class UGameplayEffect;
class UPdAbilitySystemComponent;

/** 장비 컴포넌트 로그 카테고리입니다. */
DECLARE_LOG_CATEGORY_EXTERN(EquipmentComponentLog, Log, All);

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
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 장비 컴포넌트 기본 상태를 초기화합니다. */
	UEquipmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 복제 프로퍼티를 등록합니다. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 소유 캐릭터, ASC, Inventory 캐시를 갱신합니다. */
	void RefreshCachedReferences();

	/** 요청된 아이템 정의를 반환합니다. */
	const UItemDefinition* GetRequestedWeaponDefinition() const;

	/** 장착 데이터를 반환합니다. */
	bool GetEquipData(FEquipData& OutEquipData) const;

	/** 장착 해제 데이터를 반환합니다. */
	bool GetUnequipData(FUnequipData& OutUnequipData) const;

	/** 공격 데이터를 반환합니다. */
	bool GetAttackData(FAttackData& OutAttackData) const;

	bool AllowsMovementDuringAttack() const;

	/** 피격 리액션 데이터를 반환합니다. */
	bool GetHitReactData(FHitReactData& OutHitReactData) const;


	/** 현재 무기 액터를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!Equipment")
	AWeaponBase* GetCurrentWeaponActor() const { return CurrentWeaponActor; }

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	FGuid GetCurrentWeaponId() const { return CurrentWeaponId; }

	/** 요청 아이템을 저장합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool SetRequestedWeaponInstance(UItemInstance* WeaponInstance);

	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	void ClearRequestedWeaponInstance();

	bool RequestWeaponSelection(UItemInstance* WeaponInstance);

	bool RequestWeaponUnequip();

	/** 요청된 무기를 장착합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool EquipWeapon();

	/** 현재 아이템을 해제합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool UnequipCurrentWeapon();

protected:
	// Timing hooks
	virtual void BeginPlay() override;

	// Network timing callbacks
	/** 서버에 요청 아이템 ID를 전달합니다. */
	UFUNCTION(Server, Reliable)
	void ServerSetRequestedWeapon(FGuid WeaponId);

	/** 서버에 요청된 무기 장착을 요청합니다. */
	UFUNCTION(Server, Reliable)
	void ServerEquipWeapon();

	/** 실제 장착 로직을 처리합니다. */
	bool EquipWeaponInternal(UItemInstance* WeaponInstance);

	/** 장착에 필요한 아이템, ID, 액터 클래스, 스탯 정보를 구성합니다. */
	bool ResolveWeaponEquipRequest(UItemInstance* WeaponInstance, const UItemDefinition*& OutItemDefinition, FGuid& OutWeaponId) const;

	/** 이미 현재 장착된 무기인지 반환합니다. */
	bool IsCurrentWeapon(FGuid WeaponId) const;

	bool TryActivateSingleAbilityTag(const FGameplayTag& AbilityTag) const;

	bool HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const;
	FGameplayTag GetEquipAbilityTag() const;
	FGameplayTag GetUnequipAbilityTag() const;

	TSubclassOf<AWeaponBase> LoadWeaponActorClass(const UItemDefinition* ItemDefinition) const;

	/** 무기 액터를 생성하고 소유자 메시에 부착합니다. */
	AWeaponBase* SpawnAndAttachWeaponActor(TSubclassOf<AWeaponBase> WeaponClass, const UItemDefinition* ItemDefinition) const;

	/** 장착 스탯 GameplayEffect를 적용하고 현재 스냅샷으로 저장합니다. */
	void ApplyAndStoreWeaponStats(const UItemDefinition* ItemDefinition, FEquippedItemStatSnapshot& PendingStatSnapshot);

	/** 현재 장착 스탯 GameplayEffect를 역적용합니다. */
	void RemoveCurrentWeaponStats();

	/** 현재 무기 상태를 확정하고 복제 dirty 플래그를 표시합니다. */
	void CommitCurrentWeaponState(FGuid NewCurrentWeaponId, AWeaponBase* NewWeaponActor);

	/** 실제 장착 해제 로직을 처리합니다. */
	bool UnequipCurrentWeaponInternal();

	/** 요청 아이템을 초기화합니다. */
	void ClearRequestedWeapon();

	/** 현재 아이템 정의를 반환합니다. */
	const UItemDefinition* GetCurrentWeaponDefinition() const;

	/** 아이템 스탯 스냅샷을 구성합니다. */
	bool BuildItemStatSnapshot(const UItemInstance* ItemInstance, FEquippedItemStatSnapshot& OutSnapshot) const;

	/** 스탯 스냅샷을 GameplayEffect로 적용합니다. */
	bool ApplyItemStatSnapshot(const FEquippedItemStatSnapshot& StatSnapshot, float MagnitudeScale) const;

	/** 소유 인벤토리에서 아이템 인스턴스를 찾습니다. */
	UItemInstance* FindOwnedItemInstanceById(FGuid ItemId) const;

	/** 무기를 소유자 메시에 부착합니다. */
	void AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const;

protected:

	/** 소유 캐릭터 캐시입니다. */
	UPROPERTY(Transient)
	TObjectPtr<APdCharacterBase> CachedOwner;

	/** 소유 ASC 캐시입니다. */
	UPROPERTY(Transient)
	TObjectPtr<UPdAbilitySystemComponent> CachedASC;

	/** 소유 Inventory 캐시입니다. */
	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> CachedInventory;

	/** 현재 무기 액터입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Equipment|Stat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> StatUpGameplayEffectClass;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	TObjectPtr<AWeaponBase> CurrentWeaponActor;


	/** 요청된 아이템 ID입니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid RequestedWeaponId;

	/** 현재 무기 아이템 ID입니다. */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid CurrentWeaponId;

	/** 현재 무기 스탯 스냅샷입니다. 해제 시 반대 수치로 GE를 적용해 되돌립니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	FEquippedItemStatSnapshot CurrentWeaponStatSnapshot;

};
