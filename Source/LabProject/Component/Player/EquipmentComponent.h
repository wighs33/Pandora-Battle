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

USTRUCT(BlueprintType)
struct FEquippedItemStatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	TMap<FGameplayTag, float> BaseStatMagnitudes;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	TMap<FGameplayTag, float> EnhancedStatMagnitudes;

	void Reset()
	{
		BaseStatMagnitudes.Reset();
		EnhancedStatMagnitudes.Reset();
	}

	bool HasAnyMagnitude() const
	{
		return !BaseStatMagnitudes.IsEmpty() || !EnhancedStatMagnitudes.IsEmpty();
	}
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEquipmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	void RefreshCurrentWeaponAnimationLayer();

	UFUNCTION(BlueprintPure, Category = "!Equipment")
	EEnum_Direction GetCurrentWeaponLoadoutDirection() const { return CurrentWeaponLoadoutDirection; }

	FOnCurrentWeaponDefinitionChanged OnCurrentWeaponDefinitionChanged;

	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool SetRequestedWeaponInstance(UItemInstance* WeaponInstance);

	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	void ClearRequestedWeaponInstance();

	bool RequestWeaponSelectionForDirection(EEnum_Direction Direction, UItemInstance* WeaponInstance);

	bool RequestCurrentWeaponLoadoutDirection(EEnum_Direction Direction, UItemInstance* WeaponInstance);

	bool RequestWeaponUnequip();

	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool EquipWeapon();

	bool CompletePendingWeaponSelectionWithoutAnimation();
	bool TryResumePendingWeaponSelection();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Equipment")
	bool EquipWeaponDefinition(const UItemDefinition* WeaponDefinition);

	UFUNCTION(BlueprintCallable, Category = "!Equipment")
	bool UnequipCurrentWeapon();

protected:
	// Timing hooks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Network timing callbacks
	UFUNCTION(Server, Reliable)
	void ServerSetRequestedWeapon(FGuid WeaponId, EEnum_Direction RequestedDirection);

	UFUNCTION(Server, Reliable)
	void ServerSetCurrentWeaponLoadoutDirection(FGuid WeaponId, EEnum_Direction RequestedDirection);

	UFUNCTION(Server, Reliable)
	void ServerEquipWeapon();

	bool EquipWeaponInternal(UItemInstance* WeaponInstance, EEnum_Direction WeaponLoadoutDirection);

	bool ResolveWeaponEquipRequest(UItemInstance* WeaponInstance, const UItemDefinition*& OutItemDefinition, FGuid& OutWeaponId) const;

	bool IsCurrentWeapon(FGuid WeaponId) const;

	bool ApplyCurrentWeaponLoadoutDirection(FGuid WeaponId, EEnum_Direction Direction);

	bool TryActivateSingleAbilityTag(const FGameplayTag& AbilityTag) const;

	bool HasActiveAbilityWithTags(const FGameplayTagContainer& AbilityTags) const;
	FGameplayTag GetEquipAbilityTag() const;
	FGameplayTag GetUnequipAbilityTag() const;
	UAnimMontage* GetCachedEquipMontage(const UItemDefinition* ItemDefinition) const;
	TSubclassOf<UAnimInstance> GetCachedEquipAnimLayer(const UItemDefinition* ItemDefinition) const;
	UAnimMontage* GetCachedUnequipMontage(const UItemDefinition* ItemDefinition) const;
	UAnimMontage* GetCachedAttackMontage(const UItemDefinition* ItemDefinition) const;
	UAnimMontage* GetCachedHitReactMontage(const UItemDefinition* ItemDefinition) const;

	TSubclassOf<AWeaponBase> LoadWeaponActorClass(const UItemDefinition* ItemDefinition) const;
	bool IsWeaponPresentationLoaded(const UItemDefinition* ItemDefinition) const;
	bool RequestWeaponPresentationLoad(const UItemDefinition* ItemDefinition, FSimpleDelegate OnLoaded);
	void HandleWeaponPresentationLoaded(FPrimaryAssetId ItemDefinitionId);
	void RefreshCurrentWeaponPresentation();
	void ReleaseWeaponPresentationLoads();

	AWeaponBase* SpawnAndAttachWeaponActor(TSubclassOf<AWeaponBase> WeaponClass, const UItemDefinition* ItemDefinition) const;

	void ApplyAndStoreWeaponStats(const UItemDefinition* ItemDefinition, FEquippedItemStatSnapshot& PendingStatSnapshot);
	void ApplyCurrentWeaponTagEffect(const UItemDefinition* ItemDefinition);
	bool ApplyEquipAbilityCooldown();
	void RemoveCurrentWeaponTagEffect(const UItemDefinition* ItemDefinition);

	void RemoveCurrentWeaponStats();

	void CommitCurrentWeaponState(FGuid NewCurrentWeaponId, AWeaponBase* NewWeaponActor, const UItemDefinition* NewWeaponDefinition, EEnum_Direction NewWeaponLoadoutDirection);

	bool UnequipCurrentWeaponInternal();

	void ClearRequestedWeapon();

	UFUNCTION()
	void OnRep_CurrentWeaponDefinition();

	UFUNCTION()
	void OnRep_CurrentWeaponActor();

	void NotifyCurrentWeaponDefinitionChanged();
	void NotifyCurrentWeaponStateChanged();

	bool BuildItemStatSnapshot(const UItemInstance* ItemInstance, FEquippedItemStatSnapshot& OutSnapshot) const;
	bool BuildItemDefinitionStatSnapshot(const UItemDefinition* ItemDefinition, FEquippedItemStatSnapshot& OutSnapshot) const;

	bool ApplyItemStatSnapshot(const FEquippedItemStatSnapshot& StatSnapshot, float MagnitudeScale) const;

	UItemInstance* FindOwnedItemInstanceById(FGuid ItemId) const;

	void AttachWeaponToOwner(AWeaponBase* WeaponActor, const UItemDefinition* ItemDefinition) const;
	bool HasEquipmentAuthority() const;
	bool ResolveWeaponIdFromInstance(UItemInstance* WeaponInstance, FGuid& OutWeaponId) const;
	bool ResolveOwnedWeaponById(FGuid WeaponId, UItemInstance*& OutWeaponInstance, const UItemDefinition*& OutItemDefinition) const;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Equipment|Stat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> StatUpGameplayEffectClass;

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

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	FGuid CurrentWeaponId;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment")
	EEnum_Direction CurrentWeaponLoadoutDirection = EEnum_Direction::Center;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Equipment|Stat")
	FEquippedItemStatSnapshot CurrentWeaponStatSnapshot;

	FActiveGameplayEffectHandle CurrentWeaponTagEffectHandle;

	mutable const UItemDefinition* CachedEquipDataItemDefinition = nullptr;
	mutable TWeakObjectPtr<UAnimMontage> CachedEquipMontage;
	mutable TWeakObjectPtr<UClass> CachedEquipAnimLayerClass;
	mutable const UItemDefinition* CachedUnequipDataItemDefinition = nullptr;
	mutable TWeakObjectPtr<UAnimMontage> CachedUnequipMontage;
	mutable const UItemDefinition* CachedAttackDataItemDefinition = nullptr;
	mutable TWeakObjectPtr<UAnimMontage> CachedAttackMontage;
	mutable const UItemDefinition* CachedHitReactDataItemDefinition = nullptr;
	mutable TWeakObjectPtr<UAnimMontage> CachedHitReactMontage;
	mutable const UItemDefinition* CachedWeaponActorClassItemDefinition = nullptr;
	mutable TWeakObjectPtr<UClass> CachedWeaponActorClass;

	TMap<FPrimaryAssetId, TSharedPtr<FStreamableHandle>> WeaponPresentationLoadHandles;
	TMap<FPrimaryAssetId, TArray<FSimpleDelegate>> PendingWeaponPresentationCallbacks;
	uint32 WeaponPresentationRequestGeneration = 0;
	FPrimaryAssetId PendingDefinitionEquipAssetId;
};
