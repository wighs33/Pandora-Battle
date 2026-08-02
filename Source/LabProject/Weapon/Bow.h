#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "Bow.generated.h"

class UAnimMontage;
class UPdAbilitySystemComponent;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API ABow : public AWeaponBase
{
	GENERATED_BODY()

public:
	// Input commands
	virtual bool HandleAimStart(APdPlayer* PlayerCharacter) override;
	virtual void HandleAimEnd(APdPlayer* PlayerCharacter) override;
	virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter) override;
	virtual bool HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor) override;
	virtual bool HandleAIPrimaryAttackAtLocation(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation) override;

	// Delegate callbacks
	virtual bool OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter) override;

protected:
	// Timing hooks
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Network timing callbacks
	UFUNCTION(Server, Reliable)
	void ServerBeginDraw();

	UFUNCTION(Server, Reliable)
	void ServerLaunchArrow(
		FVector_NetQuantize RequestedViewLocation,
		FVector_NetQuantizeNormal RequestedViewDirection);

	UFUNCTION(Server, Reliable)
	void ServerHandleAimEnd();

	// Query helpers
	virtual UAnimMontage* GetConfiguredWeaponMontage() const override;
	virtual FName GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const override;
	TSubclassOf<AActor> GetArrowActorClass() const;
	FName GetArrowAttachSocketName() const;
	float GetArrowTraceRange() const;
	float GetMinimumDrawDuration() const;
	float GetBowFireInterval() const;
	TArray<TEnumAsByte<EObjectTypeQuery>> GetBowTraceObjectTypes() const;
	float GetEffectiveMinimumDrawDuration() const;
	float GetEffectiveBowFireInterval() const;
	bool CanServerUseBow(const ACharacterBase* AttackingCharacter, bool bRequirePlayerAim) const;
	bool IsServerFireCadenceReady() const;

	// Action helpers
	bool BeginServerDraw(APdPlayer* PlayerCharacter);
	void BindServerDrawInvalidation(UPdAbilitySystemComponent* AbilitySystemComponent);
	void UnbindServerDrawInvalidation();
	void HandleOwnerDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void HandleServerDrawReady();
	void InvalidateServerDrawState(bool bDestroyServerDrawnArrow);
	uint32 ConsumeServerDrawToken();
	void RecordServerArrowLaunch();
	AActor* SpawnArrowActor(ACharacterBase* Character, bool bAttachToCharacter);
	AActor* SpawnDrawnArrow(APdPlayer* PlayerCharacter);
	void DestroyDrawnArrow();
	bool TryGetArrowLaunchStartLocation(const ACharacterBase* Character, FVector& OutLocation) const;
	FVector GetAIArrowAimLocation(const AActor* TargetActor) const;
	bool LaunchArrowAtTargetOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor);
	bool LaunchArrowAtLocationOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation);
	bool LaunchArrowOnServer(
		APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection);
	AActor* RefreshDrawnArrow(APdPlayer* PlayerCharacter);
	FName ResolveArrowAttachSocketName() const;
	FVector CalculateArrowLaunchDirection(
		const APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		const FVector& LaunchStartLocation) const;

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
