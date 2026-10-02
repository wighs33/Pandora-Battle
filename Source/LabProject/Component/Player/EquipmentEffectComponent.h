#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Component/Character/AbilitySystemReadySubscription.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "EquipmentEffectComponent.generated.h"

class ACharacterBase;
class UInventoryComponent;
class UItemDefinition;
class UItemInstance;
class UPdAbilitySystemComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogEquipmentEffect, Log, All);
DECLARE_MULTICAST_DELEGATE(FOnEquipmentStatsChanged);

/**
 * 장착한 아이템의 능력치를 캐릭터의 ASC에 Infinite GameplayEffect로 건다.
 *
 * 방어구·장신구 슬롯마다, 그리고 현재 무기에 효과 핸들을 하나씩 둔다. 아이템이 바뀌면 그 아이템의 핸들만 지우고 다시 걸며,
 * 무기 효과는 능력치와 함께 무기 아이템 태그도 부여한다. 서버만 효과를 걸고, 클라이언트는 화면에 보여 줄 합계만 계산한다.
 * 무기는 UEquipmentComponent가 바꿀 때 SetWeapon으로 알려 준다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UEquipmentEffectComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UEquipmentEffectComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * 현재 무기를 바꾼다. 무기가 없으면 nullptr. 인벤토리에 없는 무기(AI 기본 무기)는 정의의 능력치만 쓴다.
	 * 서버에서는 무기 효과를 바로 바꾸고, ASC가 아직 준비되지 않았으면 준비될 때 건다.
	 */
	void SetWeapon(const UItemDefinition* WeaponDefinition, FGuid WeaponItemId);

	/** 장착한 방어구·장신구와 현재 무기가 더하는 능력치 합계. 무기 피해량처럼 속성에 더하지 않는 값도 포함한다. */
	void GetEquipmentBonusStatMagnitudes(TMap<FGameplayTag, float>& OutStatMagnitudes) const;

private:
	/** 아이템 하나가 ASC에 걸어 둔(또는 걸어야 할) 효과. */
	struct FItemEffect
	{
		TMap<FGameplayTag, float> StatMagnitudes;
		FGameplayTag GrantedTag;
		TWeakObjectPtr<const UObject> SourceObject;
		bool bWeapon = false;
		FActiveGameplayEffectHandle Handle;

		bool HasSameEffect(const FItemEffect& Other) const;
	};

	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleAbilitySystemReady(ACharacterBase* Character, UPdAbilitySystemComponent* ReadyAbilitySystem);
	void HandleAbilitySystemReleased(ACharacterBase* Character, UPdAbilitySystemComponent* ReleasedAbilitySystem);
	void HandleInventoryChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool HasEffectAuthority() const;
	void BindInventory(UInventoryComponent* NewInventory);

	/** 인벤토리 슬롯과 현재 무기에서 지금 걸려 있어야 할 효과를 만든다. 핸들은 비어 있다. */
	void BuildDesiredEffects(TMap<FGameplayTag, FItemEffect>& OutSlotEffects, FItemEffect& OutWeaponEffect) const;
	static void CollectItemStats(const UItemDefinition* Definition, const UItemInstance* Instance, FItemEffect& OutEffect);

	/** 바뀐 효과만 다시 건다. 능력치가 달라졌으면 알림을 보낸다. */
	void RefreshEffects();
	bool UpdateEffect(FItemEffect& AppliedEffect, const FItemEffect& DesiredEffect);
	void ApplyEffect(FItemEffect& Effect);
	void RemoveEffect(FItemEffect& Effect);
	void RemoveAllEffects();
	void NotifyEquipmentStatsChanged();

public:
	FOnEquipmentStatsChanged OnEquipmentStatsChanged;

private:
	FAbilitySystemReadySubscription AbilitySystemSubscription;

	/** 효과를 건 ASC. 캐릭터가 다른 ASC로 옮겨 가면 이 ASC에서 먼저 거둔다. */
	TWeakObjectPtr<UPdAbilitySystemComponent> AbilitySystem;

	TWeakObjectPtr<UInventoryComponent> Inventory;
	FDelegateHandle EquipmentSlotsChangedHandle;
	FDelegateHandle InventoryChangedHandle;

	UPROPERTY(Transient)
	TObjectPtr<const UItemDefinition> WeaponDefinition;
	FGuid WeaponItemId;

	TMap<FGameplayTag, FItemEffect> SlotEffects;
	FItemEffect WeaponEffect;
};
