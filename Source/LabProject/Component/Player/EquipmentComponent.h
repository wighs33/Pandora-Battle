#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Common/EquipmentAbilityData.h"
#include "Common/Enum_Direction.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "UObject/PrimaryAssetId.h"
#include "EquipmentComponent.generated.h"

class AWeaponBase;
class ACharacterBase;
class UInventoryComponent;
class UAnimInstance;
class UAnimMontage;
class UItemDefinition;
class UItemInstance;
class UGameplayEffect;
class UPdAbilitySystemComponent;
struct FStreamableHandle;

DECLARE_LOG_CATEGORY_EXTERN(EquipmentComponentLog, Log, All);
DECLARE_MULTICAST_DELEGATE(FOnCurrentWeaponDefinitionChanged);
DECLARE_MULTICAST_DELEGATE(FOnEquipmentStatsChanged);

USTRUCT(BlueprintType)
struct FEquippedItemStatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	TMap<FGameplayTag, float> BaseStatMagnitudes;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	TMap<FGameplayTag, float> EnhancedStatMagnitudes;

	// 무기 피해량은 전투가 직접 읽으므로 ASC에 더하지 않고 변경 감지에만 포함한다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	TMap<FGameplayTag, float> NonAttributeStatMagnitudes;

	void Reset()
	{
		BaseStatMagnitudes.Reset();
		EnhancedStatMagnitudes.Reset();
		NonAttributeStatMagnitudes.Reset();
	}

	bool HasAnyMagnitude() const
	{
		return !BaseStatMagnitudes.IsEmpty()
			|| !EnhancedStatMagnitudes.IsEmpty()
			|| !NonAttributeStatMagnitudes.IsEmpty();
	}
};

/**
 * 캐릭터에 적용할 무기와 장비 능력치를 관리한다.
 *
 * 플레이어 소유 목록은 InventoryComponent에 두고, 서버에서 승인한 무기 전환과
 * 현재 Pawn의 무기 Actor·애니메이션·능력치 적용을 담당한다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEquipmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//------------------------------------------------------------------------------------------------------------------
	void RefreshCachedReferences();

	const UItemDefinition* GetRequestedWeaponDefinition() const;

	bool GetEquipData(FEquipData& OutEquipData) const;

	bool GetUnequipData(FUnequipData& OutUnequipData) const;

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	bool ShouldEquipWeaponsWithoutAnimation() const;

	bool GetAttackData(FAttackData& OutAttackData) const;

	bool AllowsMovementDuringAttack() const;

	bool GetHitReactData(FHitReactData& OutHitReactData) const;

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	AWeaponBase* GetCurrentWeaponActor() const { return CurrentWeaponActor; }

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	FGuid GetCurrentWeaponId() const { return CurrentWeaponId; }

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	const UItemDefinition* GetCurrentWeaponDefinition() const;

	float GetCurrentWeaponStatMagnitude(FGameplayTag StatTag) const;

	void GetEquipmentBonusStatMagnitudes(
		TMap<FGameplayTag, float>& OutStatMagnitudes) const;

	void RefreshCurrentWeaponAnimationLayer();

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	EEnum_Direction GetCurrentWeaponLoadoutDirection() const { return CurrentWeaponLoadoutDirection; }

	FOnCurrentWeaponDefinitionChanged OnCurrentWeaponDefinitionChanged;
	FOnEquipmentStatsChanged OnEquipmentStatsChanged;

	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	void ClearRequestedWeaponInstance();

	bool RequestWeaponSelectionForDirection(EEnum_Direction Direction, UItemInstance* WeaponInstance);

	bool RequestWeaponUnequip();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Equipment")
	bool EquipWeapon();

	bool CompletePendingWeaponSelectionWithoutAnimation();
	bool TryResumePendingWeaponSelection();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Equipment")
	bool EquipWeaponDefinition(const UItemDefinition* WeaponDefinition);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Equipment")
	bool UnequipCurrentWeapon();

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//------------------------------------------------------------------------------------------------------------------
	bool EquipWeaponInternal(UItemInstance* WeaponInstance, EEnum_Direction WeaponLoadoutDirection);

	bool ResolveWeaponEquipRequest(UItemInstance* WeaponInstance, const UItemDefinition*& OutItemDefinition, FGuid& OutWeaponId) const;

	bool IsCurrentWeapon(FGuid WeaponId) const;

	bool ApplyCurrentWeaponLoadoutDirection(FGuid WeaponId, EEnum_Direction Direction);

	bool TryActivateSingleAbilityTag(const FGameplayTag& AbilityTag) const;

	bool HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const;
	FGameplayTag GetEquipAbilityTag() const;
	FGameplayTag GetUnequipAbilityTag() const;
	UAnimMontage* GetLoadedEquipMontage(const UItemDefinition* ItemDefinition) const;
	TSubclassOf<UAnimInstance> GetLoadedEquipAnimLayer(const UItemDefinition* ItemDefinition) const;
	UAnimMontage* GetLoadedUnequipMontage(const UItemDefinition* ItemDefinition) const;
	UAnimMontage* GetLoadedAttackMontage(const UItemDefinition* ItemDefinition) const;
	UAnimMontage* GetLoadedHitReactMontage(const UItemDefinition* ItemDefinition) const;

	TSubclassOf<AWeaponBase> GetLoadedWeaponActorClass(const UItemDefinition* ItemDefinition) const;
	bool IsWeaponPresentationLoaded(const UItemDefinition* ItemDefinition) const;
	bool RequestWeaponPresentationLoad(const UItemDefinition* ItemDefinition, FSimpleDelegate OnLoaded);
	void HandleWeaponPresentationLoaded(FPrimaryAssetId ItemDefinitionId);
	void RefreshCurrentWeaponPresentation();
	void ReleaseWeaponPresentationLoads();

	AWeaponBase* SpawnAndAttachWeaponActor(TSubclassOf<AWeaponBase> WeaponClass, const UItemDefinition* ItemDefinition) const;

	bool ApplyAndStoreWeaponStats(const FEquippedItemStatSnapshot& PendingStatSnapshot);
	void ApplyCurrentWeaponTagEffect(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		const UItemDefinition* ItemDefinition);
	bool ApplyEquipAbilityCooldown();
	void RemoveCurrentWeaponTagEffect(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		const UItemDefinition* ItemDefinition);

	bool RemoveCurrentWeaponStats();

	void CommitCurrentWeaponState(FGuid NewCurrentWeaponId, AWeaponBase* NewWeaponActor,
		const UItemDefinition* NewWeaponDefinition, EEnum_Direction NewWeaponLoadoutDirection);

	bool UnequipCurrentWeaponInternal();

	// 승인된 장착 완료와 AI의 직접 장착이 공유하는 적용 단계다. 클라이언트 요청을 받지 않는다.
	bool ReplaceWeapon(const UItemDefinition* Definition, FGuid WeaponId, EEnum_Direction Direction,
		const FEquippedItemStatSnapshot& StatSnapshot);

	UFUNCTION()
	void OnRep_CurrentWeaponDefinition();

	UFUNCTION()
	void OnRep_CurrentWeaponActor();

	UFUNCTION()
	void OnRep_CurrentWeaponId();

	void NotifyCurrentWeaponDefinitionChanged();
	void NotifyCurrentWeaponStateChanged();
	void HandleEquipCooldownTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void HandleEquipmentSlotsChanged();
	void HandleInventoryChanged();

	bool BuildItemStatSnapshot(const UItemInstance* ItemInstance, FEquippedItemStatSnapshot& OutSnapshot) const;
	bool BuildItemDefinitionStatSnapshot(const UItemDefinition* ItemDefinition, FEquippedItemStatSnapshot& OutSnapshot) const;
	bool BuildEquippedItemsStatSnapshot(FEquippedItemStatSnapshot& OutSnapshot) const;
	bool BuildCurrentWeaponStatSnapshot(FEquippedItemStatSnapshot& OutSnapshot) const;

	bool ApplyItemStatSnapshot(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		const FEquippedItemStatSnapshot& StatSnapshot,
		float MagnitudeScale) const;
	bool SetAppliedStatSnapshot(
		UPdAbilitySystemComponent* AbilitySystemComponent,
		FEquippedItemStatSnapshot& AppliedSnapshot,
		const FEquippedItemStatSnapshot& DesiredSnapshot,
		bool& bOutChanged) const;
	bool RefreshEquipmentStats();
	bool ClearAppliedEquipmentState(UPdAbilitySystemComponent* AbilitySystemComponent);
	void NotifyEquipmentStatsChanged();
	void UnbindEquipmentSlotsChanged();
	void UnbindInventoryChanged();

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

	UPROPERTY(Transient)
	TObjectPtr<ACharacterBase> CachedOwner;

	UPROPERTY(Transient)
	TObjectPtr<UPdAbilitySystemComponent> CachedASC;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> CachedInventory;

	FDelegateHandle EquipCooldownTagChangedDelegateHandle;
	FDelegateHandle EquipmentSlotsChangedDelegateHandle;
	FDelegateHandle InventoryChangedDelegateHandle;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> EquipmentStatGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Equipment|Effect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> EquippedItemEffectClass;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeaponActor, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	TObjectPtr<AWeaponBase> CurrentWeaponActor;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeaponDefinition, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	TObjectPtr<const UItemDefinition> CurrentWeaponDefinition;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid RequestedWeaponId;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	EEnum_Direction RequestedWeaponLoadoutDirection = EEnum_Direction::Center;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeaponId, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid CurrentWeaponId;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	EEnum_Direction CurrentWeaponLoadoutDirection = EEnum_Direction::Center;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	FEquippedItemStatSnapshot CurrentWeaponStatSnapshot;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	FEquippedItemStatSnapshot EquippedItemsStatSnapshot;

	bool bEndingPlay = false;
	bool bEquipmentStatsInitialized = false;
	bool bRefreshingEquipmentStats = false;

	FActiveGameplayEffectHandle CurrentWeaponTagEffectHandle;

	TMap<FPrimaryAssetId, TSharedPtr<FStreamableHandle>> WeaponPresentationLoadHandles;
	TMap<FPrimaryAssetId, TArray<FSimpleDelegate>> PendingWeaponPresentationCallbacks;
	uint32 WeaponPresentationRequestGeneration = 0;
};
