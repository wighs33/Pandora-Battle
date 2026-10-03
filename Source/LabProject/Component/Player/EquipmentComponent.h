#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Ability/EquipmentAbilityData.h"
#include "Common/Enum_Direction.h"
#include "Component/Character/AbilitySystemReadySubscription.h"
#include "Component/Player/WeaponPresentationLoader.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "EquipmentComponent.generated.h"

class AWeaponBase;
class ACharacterBase;
class UEquipmentEffectComponent;
class UInventoryComponent;
class UAnimInstance;
class UAnimMontage;
class UItemDefinition;
class UItemInstance;
class UPdAbilitySystemComponent;

DECLARE_LOG_CATEGORY_EXTERN(EquipmentComponentLog, Log, All);

/**
 * 캐릭터가 든 무기를 관리한다.
 *
 * 플레이어 소유 목록은 InventoryComponent에 두고, 서버에서 승인한 무기 전환과 현재 Pawn의 무기 Actor·애니메이션을 담당한다.
 * 무기와 방어구의 능력치는 UEquipmentEffectComponent가 GameplayEffect로 걸며, 무기가 바뀌면 이 컴포넌트가 알려 준다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UEquipmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	const UItemDefinition* GetRequestedWeaponDefinition() const;

	bool GetEquipData(FEquipData& OutEquipData) const;

	bool GetUnequipData(FUnequipData& OutUnequipData) const;

	bool ShouldEquipWeaponsWithoutAnimation() const;

	bool GetAttackData(FAttackData& OutAttackData) const;

	bool AllowsMovementDuringAttack() const;

	bool GetHitReactData(FHitReactData& OutHitReactData) const;

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	AWeaponBase* GetCurrentWeaponActor() const { return CurrentWeaponActor; }

	FGuid GetCurrentWeaponId() const { return CurrentWeaponId; }

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	const UItemDefinition* GetCurrentWeaponDefinition() const;

	float GetCurrentWeaponStatMagnitude(FGameplayTag StatTag) const;

	void RefreshCurrentWeaponAnimationLayer();

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	EEnum_Direction GetCurrentWeaponLoadoutDirection() const { return CurrentWeaponLoadoutDirection; }

	void ClearRequestedWeaponInstance();

	bool RequestWeaponSelectionForDirection(EEnum_Direction Direction, UItemInstance* WeaponInstance);

	bool RequestWeaponUnequip();

	bool EquipWeapon();

	bool CompletePendingWeaponSelectionWithoutAnimation();
	bool TryResumePendingWeaponSelection();

	bool EquipWeaponDefinition(const UItemDefinition* WeaponDefinition);

	bool UnequipCurrentWeapon();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleAbilitySystemReady(ACharacterBase* Character, UPdAbilitySystemComponent* ReadyAbilitySystem);
	void HandleAbilitySystemReleased(ACharacterBase* Character, UPdAbilitySystemComponent* ReleasedAbilitySystem);

	UFUNCTION()
	void OnRep_CurrentWeaponDefinition();

	UFUNCTION()
	void OnRep_CurrentWeaponActor();

	UFUNCTION()
	void OnRep_CurrentWeaponId();
	void HandleEquipCooldownTagChanged(FGameplayTag CallbackTag, int32 NewCount);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	ACharacterBase* GetCharacter() const;
	/** 이 캐릭터를 Avatar로 연결한 ASC. 연결 전이나 다른 Pawn으로 넘어간 뒤에는 nullptr. */
	UPdAbilitySystemComponent* GetReadyAbilitySystem() const;
	UInventoryComponent* GetInventory() const;
	UEquipmentEffectComponent* GetEquipmentEffects() const;
	void BindEquipCooldownTag(UPdAbilitySystemComponent* AbilitySystem);

	bool EquipWeaponInternal(UItemInstance* WeaponInstance, EEnum_Direction WeaponLoadoutDirection);

	bool ResolveWeaponEquipRequest(UItemInstance* WeaponInstance, const UItemDefinition*& OutItemDefinition, FGuid& OutWeaponId) const;

	bool IsCurrentWeapon(FGuid WeaponId) const;

	bool ApplyCurrentWeaponLoadoutDirection(FGuid WeaponId, EEnum_Direction Direction);

	bool TryActivateSingleAbilityTag(const FGameplayTag& AbilityTag) const;

	bool HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const;
	bool IsDeathTransitionActive() const;
	TSubclassOf<UAnimInstance> GetLoadedEquipAnimLayer(const UItemDefinition* ItemDefinition) const;

	bool IsWeaponPresentationLoaded(const UItemDefinition* ItemDefinition) const;
	bool RequestWeaponPresentationLoad(const UItemDefinition* ItemDefinition, FSimpleDelegate OnLoaded);
	void RefreshCurrentWeaponPresentation();

	AWeaponBase* SpawnAndAttachWeaponActor(TSubclassOf<AWeaponBase> WeaponClass, const UItemDefinition* ItemDefinition) const;

	bool ApplyEquipAbilityCooldown();

	void CommitCurrentWeaponState(FGuid NewCurrentWeaponId, AWeaponBase* NewWeaponActor,
		const UItemDefinition* NewWeaponDefinition, EEnum_Direction NewWeaponLoadoutDirection);

	bool UnequipCurrentWeaponInternal();

	// 승인된 장착 완료와 AI의 직접 장착이 공유하는 적용 단계다. 클라이언트 요청을 받지 않는다.
	bool ReplaceWeapon(const UItemDefinition* Definition, FGuid WeaponId, EEnum_Direction Direction);

	void NotifyCurrentWeaponDefinitionChanged();
	void NotifyCurrentWeaponStateChanged();
	void SyncWeaponEffect() const;

	UItemInstance* FindOwnedItemInstanceById(FGuid ItemId) const;

	void AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const;
	bool HasEquipmentAuthority() const;
	bool ResolveWeaponIdFromInstance(UItemInstance* WeaponInstance, FGuid& OutWeaponId) const;
	bool IsWeaponDefinitionEquipable(const UItemDefinition* ItemDefinition) const;
	void MarkCurrentWeaponStateDirty(
		bool bCurrentWeaponChanged,
		bool bCurrentWeaponIdChanged,
		bool bCurrentWeaponDefinitionChanged,
		bool bCurrentWeaponLoadoutDirectionChanged);
	void RefreshPandoraForWeaponChange() const;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeaponActor, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	TObjectPtr<AWeaponBase> CurrentWeaponActor;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeaponDefinition, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	TObjectPtr<const UItemDefinition> CurrentWeaponDefinition;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid RequestedWeaponId;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	EEnum_Direction RequestedWeaponLoadoutDirection = EEnum_Direction::Center;

	/** 마지막으로 받은 장착·해제 요청. 장착 쿨다운에 막힌 요청은 쿨다운이 끝나면 이 값으로 다시 시도한다. */
	TWeakObjectPtr<UItemInstance> LatestRequestedWeapon;
	EEnum_Direction LatestRequestedWeaponDirection = EEnum_Direction::Center;
	bool bHasLatestWeaponRequest = false;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeaponId, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid CurrentWeaponId;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	EEnum_Direction CurrentWeaponLoadoutDirection = EEnum_Direction::Center;

	bool bEndingPlay = false;

	FAbilitySystemReadySubscription AbilitySystemSubscription;
	TWeakObjectPtr<UPdAbilitySystemComponent> CooldownTagAbilitySystem;
	FDelegateHandle EquipCooldownTagChangedDelegateHandle;

	FWeaponPresentationLoader PresentationLoader;
	/** 선택이 바뀌거나 해제되면 올려서, 그 전에 요청한 무기 외형 로딩의 완료 콜백을 무시하게 한다. */
	uint32 WeaponPresentationRequestGeneration = 0;
};
