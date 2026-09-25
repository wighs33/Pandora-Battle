#pragma once

#include "CoreMinimal.h"
#include "Weapon/RangedWeaponBase.h"
#include "Bow.generated.h"

class UAnimMontage;
class UPdAbilitySystemComponent;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API ABow : public ARangedWeaponBase
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Network RPCs ----------------------------------------------------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerBeginDraw();

	UFUNCTION(Server, Reliable)
	void ServerLaunchArrow(
		FVector_NetQuantize RequestedViewLocation,
		FVector_NetQuantizeNormal RequestedViewDirection);

	UFUNCTION(Server, Reliable)
	void ServerHandleAimEnd();

public:
	// Event Handlers --------------------------------------------------------------------------------------------------
	virtual bool HandleAimStart(APdPlayer* PlayerCharacter) override;
	virtual void HandleAimEnd(APdPlayer* PlayerCharacter) override;
	virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter) override;
	virtual bool HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor) override;
	virtual bool HandleAIPrimaryAttackAtLocation(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation) override;

	virtual bool OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter) override;

protected:
	void HandleOwnerDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleServerDrawReady();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual UAnimMontage* GetConfiguredWeaponMontage() const override;
	TSubclassOf<AActor> GetArrowActorClass() const;
	FName GetArrowAttachSocketName() const;
	float GetArrowTraceRange() const;
	float GetMinimumDrawDuration() const;
	float GetBowFireInterval() const;
	TArray<TEnumAsByte<EObjectTypeQuery>> GetBowTraceObjectTypes() const;
	float GetEffectiveMinimumDrawDuration() const;
	float GetEffectiveBowFireInterval() const;
	bool IsServerFireCadenceReady() const;

	bool BeginServerDraw(APdPlayer* PlayerCharacter);
	void BindServerDrawInvalidation(UPdAbilitySystemComponent* AbilitySystemComponent);
	void UnbindServerDrawInvalidation();
	void InvalidateServerDrawState(bool bDestroyServerDrawnArrow);
	uint32 ConsumeServerDrawToken();
	void RecordServerArrowLaunch();
	AActor* SpawnArrowActor(ACharacterBase* Character, bool bAttachToCharacter);
	AActor* SpawnDrawnArrow(APdPlayer* PlayerCharacter);
	void DestroyDrawnArrow();
	bool TryGetArrowLaunchStartLocation(const ACharacterBase* Character, FVector& OutLocation) const;
	bool LaunchArrowAtTargetOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor);
	bool LaunchArrowAtLocationOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation);
	bool LaunchArrowOnServer(
		APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection);
	AActor* RefreshDrawnArrow(APdPlayer* PlayerCharacter);
	FVector CalculateArrowLaunchDirection(
		const APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		const FVector& LaunchStartLocation) const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<AActor> CurrentDrawnArrow = nullptr;

	UPROPERTY(Transient)
	bool bCanLaunchDrawnArrow = false;

	UPROPERTY(Transient)
	bool bServerDrawPending = false;

	UPROPERTY(Transient)
	uint32 ServerReadyDrawToken = 0;

	uint32 NextServerDrawToken = 0;
	double NextServerArrowLaunchTimeSeconds = 0.0;
	FTimerHandle ServerDrawReadyTimerHandle;
	TWeakObjectPtr<UPdAbilitySystemComponent> ServerDrawBoundAbilitySystemComponent;
	FDelegateHandle OwnerDeadTagChangedDelegateHandle;
};
