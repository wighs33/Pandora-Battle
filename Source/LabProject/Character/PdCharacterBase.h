#pragma once

#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Character.h"
#include "GameplayCueInterface.h"
#include "TimerManager.h"
#include "PdCharacterBase.generated.h"

class UAbilitySystemComponent;
class UAnimInstance;
class UCombatComponent;
class UDamageIndicatorComponent;
class UGameplayEffect;
class UEquipmentComponent;
class UHealthBarViewModel;
class USkinEquipmentComponent;
class UPdAbilitySystemComponent;
class UUserWidget;
class UWidgetComponent;
struct FOnAttributeChangeData;

/** 캐릭??로그 카테고리?�니?? */
DECLARE_LOG_CATEGORY_EXTERN(PdCharacterBaseLog, Log, All);

/**
 * <공용 캐릭??베이??
 * - ASC 초기?��? ?�당?�니??
 * - Startup abilities are granted by GameFeatureAction_AddAbilities.
 * - 체력�?ViewModel�??�님 ?�이?��? 관리합?�다.
 */
UCLASS()
class LABPROJECT_API APdCharacterBase : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	APdCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void UnPossessed() override;

	//------------------------------------------------------------------------------------------------------------------
	//--- Blueprint Timing Events
	UFUNCTION(BlueprintImplementableEvent, Category = "!Damage", meta = (DisplayName = "On Damage Taken"))
	void OnDamageTaken(float DamageAmount, bool bCriticalHit, FVector WorldLocation);

	//------------------------------------------------------------------------------------------------------------------
	//--- Ability System
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void HandleGameplayCue(AActor* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType,
		const FGameplayCueParameters& Parameters) override;

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem")
	UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const;

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "!AbilitySystem")
	void ServerSendGameplayEventToSelf(FGameplayEventData EventData);

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "!AbilitySystem")
	void MulticastSendGameplayEventToActor(AActor* TargetActor, FGameplayEventData EventData);

	void InitializeAbilitySystemActorInfo();
	void ClearAbilitySystemActorInfo();

	//------------------------------------------------------------------------------------------------------------------
	//--- Component
	UFUNCTION(BlueprintPure, Category = "!Equipment")
	UEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Combat")
	UCombatComponent* GetCombatComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Skin")
	USkinEquipmentComponent* GetSkinEquipmentComponent() const { return SkinEquipmentComponent; }

	UFUNCTION(BlueprintPure, Category = "!DamageIndicator")
	UDamageIndicatorComponent* GetDamageIndicatorComponent() const;

	//------------------------------------------------------------------------------------------------------------------
	//--- ViewModel
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void RefreshHealthBarViewModel();

	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void ApplyHealthBarViewModelToWidget(UUserWidget* InWidget);

	UFUNCTION(BlueprintPure, Category = "!ViewModel")
	UHealthBarViewModel* GetHealthBarViewModel() const { return HealthBarViewModel; }

	//------------------------------------------------------------------------------------------------------------------
	//--- Animation
	UFUNCTION(BlueprintPure, Category = "!Animation|Aim")
	float GetAimYawForAnimation() const { return AimYawForAnimation; }

	UFUNCTION(BlueprintPure, Category = "!Animation|Aim")
	float GetAimPitchForAnimation() const { return AimPitchForAnimation; }

	UFUNCTION(BlueprintCallable, Category = "!Animation")
	void ResetAnimationToDefault();

	UFUNCTION(BlueprintCallable, Category = "!Animation")
	void SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass);

	void LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const;

	//------------------------------------------------------------------------------------------------------------------
	//--- Damage
	void HandleDeathAuth();
	void HandleDamageTaken(float DamageAmount, bool bCriticalHit = false);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "!Damage")
	void HandleDeath();

	//------------------------------------------------------------------------------------------------------------------
	//--- Faction
	UFUNCTION(BlueprintPure, Category = "!Faction")
	int32 GetFactionId() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "!AbilitySystem|Cue", meta = (DisplayName = "On Dash Cue Activated"))
	void OnDashCueActivated(const FGameplayCueParameters& Parameters);

	UFUNCTION(BlueprintImplementableEvent, Category = "!AbilitySystem|Cue", meta = (DisplayName = "On Dash Cue Removed"))
	void OnDashCueRemoved(const FGameplayCueParameters& Parameters);

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Network Callbacks
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHandleDamageTaken(float DamageAmount, bool bCriticalHit, FVector_NetQuantize WorldLocation);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastHandleDeath();

	//------------------------------------------------------------------------------------------------------------------
	//--- Replication Callbacks
	UFUNCTION()
	void OnRep_CurrentAnimLayer();

	//------------------------------------------------------------------------------------------------------------------
	//--- Ability System Hooks
	void QueueAbilitySystemActorInfoInitializationRetry();
	virtual AActor* GetAbilitySystemOwnerActor() const;
	virtual AActor* GetAbilitySystemAvatarActor() const;
	void HandleDashGameplayCue(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters);
	void BindStaminaRegenToASC(UAbilitySystemComponent* InASC);
	void UnbindStaminaRegenFromASC();
	void BindDeadTagEvent(UAbilitySystemComponent* InASC);
	void UnbindDeadTagEvent();
	void OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	void ApplyStaminaRegenEffect();
	void RemoveStaminaRegenEffects();
	float GetCurrentMaxStamina() const;

	//------------------------------------------------------------------------------------------------------------------
	//--- ViewModel Helpers
	bool TryApplyHealthBarViewModelToWidget(UUserWidget* InWidget);
	bool TryApplyHealthBarViewModelToWidget();
	bool BindHealthBarViewModelToASC(UAbilitySystemComponent* InASC);
	bool IsHealthBarAttributeDataReady(const UAbilitySystemComponent* InASC) const;
	void QueueHealthBarViewModelRefreshRetry();
	void RetryRefreshHealthBarViewModel();
	void UpdateHealthBarFacing();

	//------------------------------------------------------------------------------------------------------------------
	//--- Animation Helpers
	void UpdateAimOffsetForAnimation();

	//------------------------------------------------------------------------------------------------------------------
	//--- Damage Helpers
	FVector GetDamageIndicatorWorldLocation() const;

protected:
	UPROPERTY(Transient)
	bool bAbilitySystemActorInfoInitializationQueued = false;

	UPROPERTY(Transient)
	bool bDeathHandled = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Damage|Death", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float DeathImpulseHorizontalStrength = 35000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Damage|Death", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float DeathImpulseUpwardStrength = 12000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Damage|Death", meta = (AllowPrivateAccess = "true"))
	float DeathImpulseSideStrength = 8000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Damage|Death", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float DeathImpulseLocationZOffset = 80.0f;

	TWeakObjectPtr<UAbilitySystemComponent> DeadTagBoundAbilitySystemComponent;
	FDelegateHandle DeadTagChangedDelegateHandle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AbilitySystem|Stamina", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> StaminaRegenEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AbilitySystem|Stamina", meta = (ClampMin = "0.0", ForceUnits = "s", AllowPrivateAccess = "true"))
	float StaminaRegenDelay = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AbilitySystem|Stamina", meta = (ClampMin = "1.0", AllowPrivateAccess = "true"))
	float StaminaRegenEffectLevel = 1.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> StaminaRegenASC;

	FDelegateHandle StaminaChangedDelegateHandle;
	FTimerHandle StaminaRegenDelayTimerHandle;

	//------------------------------------------------------------------------------------------------------------------
	//--- Animation
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Animation")
	TSubclassOf<UAnimInstance> DefaultAnimLayer;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentAnimLayer, Transient, BlueprintReadOnly, Category = "!Animation")
	TSubclassOf<UAnimInstance> CurrentAnimLayer;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Animation|Aim")
	float AimYawForAnimation = 0.0f;

	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Animation|Aim")
	float AimPitchForAnimation = 0.0f;

	//------------------------------------------------------------------------------------------------------------------
	//--- Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Skin", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkinEquipmentComponent> SkinEquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!DamageIndicator", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDamageIndicatorComponent> DamageIndicatorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Widget", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidget;

	//------------------------------------------------------------------------------------------------------------------
	//--- ViewModel
	UPROPERTY(Transient)
	TObjectPtr<UHealthBarViewModel> HealthBarViewModel;

	FTimerHandle HealthBarViewModelRetryTimerHandle;

	//------------------------------------------------------------------------------------------------------------------
	//--- Faction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Faction")
	int32 FactionId = 0;
};
