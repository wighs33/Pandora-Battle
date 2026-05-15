#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "Bow.generated.h"

class UAnimMontage;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API ABow : public AWeaponBase
{
	GENERATED_BODY()

public:
	// Input commands
	virtual bool HandleAimStart(APdPlayer* PlayerCharacter) override;
	virtual void HandleAimEnd(APdPlayer* PlayerCharacter) override;
	virtual bool HandlePrimaryAttack(APdPlayer* PlayerCharacter) override;

	// Delegate callbacks
	virtual bool OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter) override;

protected:
	// Timing hooks
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Network timing callbacks
	UFUNCTION(Server, Reliable)
	void ServerLaunchArrow(
		FVector_NetQuantize RequestedViewLocation,
		FVector_NetQuantizeNormal RequestedViewDirection,
		FVector_NetQuantize RequestedLaunchStartLocation);

	UFUNCTION(Server, Reliable)
	void ServerHandleAimEnd();

	// Query helpers
	virtual UAnimMontage* GetConfiguredWeaponMontage() const override;
	virtual FName GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const override;
	TSubclassOf<AActor> GetArrowActorClass() const;
	FName GetArrowAttachSocketName() const;
	float GetArrowTraceRange() const;
	const TArray<TEnumAsByte<EObjectTypeQuery>>& GetBowTraceObjectTypes() const;

	// Action helpers
	AActor* SpawnArrowActor(APdPlayer* PlayerCharacter, bool bAttachToCharacter);
	AActor* SpawnDrawnArrow(APdPlayer* PlayerCharacter);
	void DestroyDrawnArrow();
	bool TryGetArrowLaunchStartLocation(const APdPlayer* PlayerCharacter, FVector& OutLocation) const;
	bool ResolveServerArrowLaunchStartLocation(
		const APdPlayer* PlayerCharacter,
		const FVector& RequestedLaunchStartLocation,
		FVector& OutLocation) const;
	bool LaunchArrowOnServer(
		APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		const FVector& RequestedLaunchStartLocation);
	AActor* RefreshDrawnArrow(APdPlayer* PlayerCharacter);
	FName ResolveArrowAttachSocketName() const;
	FVector CalculateArrowLaunchDirection(
		const APdPlayer* PlayerCharacter,
		const FVector& RequestedViewLocation,
		const FVector& RequestedViewDirection,
		const FVector& RequestedLaunchStartLocation) const;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CurrentDrawnArrow = nullptr;

	UPROPERTY(Transient)
	bool bCanLaunchDrawnArrow = false;
};
