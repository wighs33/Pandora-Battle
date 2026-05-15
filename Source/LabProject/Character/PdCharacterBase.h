#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Character.h"
#include "PdCharacterBase.generated.h"

class UAbilitySystemComponent;
class UAnimInstance;
class UCombatComponent;
class UDamageIndicatorComponent;
class UGameplayAbility;
class UEquipmentComponent;
class UHealthBarViewModel;
class UPdAbilitySystemComponent;
class UUserWidget;
class UWidgetComponent;

/** 캐릭터 로그 카테고리입니다. */
DECLARE_LOG_CATEGORY_EXTERN(PdCharacterBaseLog, Log, All);

/**
 * <공용 캐릭터 베이스>
 * - ASC 초기화를 담당합니다.
 * - 기본 Ability 지급을 담당합니다.
 * - 체력바 ViewModel과 애님 레이어를 관리합니다.
 */
UCLASS()
class LABPROJECT_API APdCharacterBase : public ACharacter, public IAbilitySystemInterface
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

	UFUNCTION(BlueprintPure, Category = "!AbilitySystem")
	UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!AbilitySystem|Grant")
	int32 GiveDefaultAbilities();

	void InitializeAbilitySystemActorInfo();
	void ClearAbilitySystemActorInfo();

	//------------------------------------------------------------------------------------------------------------------
	//--- Component
	UFUNCTION(BlueprintPure, Category = "!Equipment")
	UEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category = "!Combat")
	UCombatComponent* GetCombatComponent() const;

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

	//------------------------------------------------------------------------------------------------------------------
	//--- Faction
	UFUNCTION(BlueprintPure, Category = "!Faction")
	int32 GetFactionId() const;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Network Callbacks
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHandleDamageTaken(float DamageAmount, bool bCriticalHit, FVector_NetQuantize WorldLocation);

	//------------------------------------------------------------------------------------------------------------------
	//--- Replication Callbacks
	UFUNCTION()
	void OnRep_CurrentAnimLayer();

	//------------------------------------------------------------------------------------------------------------------
	//--- Ability System Hooks
	void QueueAbilitySystemActorInfoInitializationRetry();
	virtual AActor* GetAbilitySystemOwnerActor() const;
	virtual AActor* GetAbilitySystemAvatarActor() const;

	//------------------------------------------------------------------------------------------------------------------
	//--- ViewModel Helpers
	void ApplyHealthBarViewModelToWidget();
	void BindHealthBarViewModelToASC(UAbilitySystemComponent* InASC);
	void UpdateHealthBarFacing();

	//------------------------------------------------------------------------------------------------------------------
	//--- Animation Helpers
	void UpdateAimOffsetForAnimation();

	//------------------------------------------------------------------------------------------------------------------
	//--- Damage Helpers
	FVector GetDamageIndicatorWorldLocation() const;

protected:
	//------------------------------------------------------------------------------------------------------------------
	//--- Ability System
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AbilitySystem|Grant")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	UPROPERTY(Transient)
	bool bAbilitySystemActorInfoInitializationQueued = false;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!DamageIndicator", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDamageIndicatorComponent> DamageIndicatorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Widget", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> HealthBarWidget;

	//------------------------------------------------------------------------------------------------------------------
	//--- ViewModel
	UPROPERTY(Transient)
	TObjectPtr<UHealthBarViewModel> HealthBarViewModel;

	//------------------------------------------------------------------------------------------------------------------
	//--- Faction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Faction")
	int32 FactionId = 0;
};
