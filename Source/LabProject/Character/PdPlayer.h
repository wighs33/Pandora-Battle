#pragma once

#include "Character/PdCharacterBase.h"
#include "Common/WeaponDefinitionData.h"
#include "Interface/InteractableInterface.h"
#include "PdPlayer.generated.h"

class UBoxComponent;
class UCameraComponent;
class UPrimitiveComponent;
class USpringArmComponent;
class APdPlayerState;
class AActor;

/**
 * <플레이어 캐릭터>
 * - 상호작용 대상 감지를 담당합니다.
 * - 플레이어 전용 카메라 구성을 가집니다.
 * - ASC 소유자를 PlayerState로 사용합니다.
 */
UCLASS()
class LABPROJECT_API APdPlayer : public APdCharacterBase
{
	GENERATED_BODY()

public:
	/** 플레이어 기본 상태를 초기화합니다. */
	APdPlayer(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Timing hooks
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** PlayerState 기준 ASC를 반환합니다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 현재 상호작용 대상이 있는지 반환합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Interaction", meta = (DisplayName = "HasCurrentInteractActors?"))
	bool HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const;

	UFUNCTION(BlueprintCallable, Category = "!Weapon|Aim")
	void SetWeaponAimActive(bool bEnabled, const FWeaponAimCameraSettings& AimCameraSettings);

	UFUNCTION(BlueprintPure, Category = "!Weapon|Aim")
	bool IsWeaponAimActive() const { return bIsWeaponAimActive; }

	bool GetWeaponAimViewPoint(FVector& OutLocation, FVector& OutDirection) const;

	bool CanInteractWithActor(AActor* InteractableActor) const;

protected:
	// Delegate callbacks
	/** 상호작용 박스 진입을 처리합니다. */
	UFUNCTION()
	void HandleInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 상호작용 박스 이탈을 처리합니다. */
	UFUNCTION()
	void HandleInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// Ability-system timing hooks
	/** ASC 소유 액터를 반환합니다. */
	virtual AActor* GetAbilitySystemOwnerActor() const override;
	void ApplyWeaponAimState(bool bEnabled, const FWeaponAimCameraSettings& AimCameraSettings);
	void UpdateWeaponAimCamera(float DeltaSeconds);

	UFUNCTION(Server, Reliable)
	void ServerSetWeaponAimActive(bool bEnabled, FWeaponAimCameraSettings AimCameraSettings);

	/** PlayerState를 프로젝트 타입으로 반환합니다. */
	APdPlayerState* GetPdPlayerState() const;

	/** 액터를 상호작용 엔트리로 변환합니다. */
	bool TryMakeInteractableEntry(AActor* OtherActor, TScriptInterface<IInteractableInterface>& OutInteractableActor) const;

protected:
	/** 현재 상호작용 가능한 액터 목록입니다. */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "!Interaction")
	TArray<TScriptInterface<IInteractableInterface>> CurrentInteractActors;

	/** 상호작용 감지 박스입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Interaction", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float InteractionServerValidationDistance = 250.0f;

	/** 카메라 붐입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 추적 카메라입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim")
	bool bIsWeaponAimActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	bool bWeaponAimCameraActive = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	FWeaponAimCameraSettings ActiveWeaponAimCameraSettings;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	bool bHasCachedWeaponAimCameraDefaults = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	float DefaultCameraFOV = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	FVector DefaultCameraBoomSocketOffset = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!Weapon|Aim|Camera")
	FRotator DefaultFollowCameraRelativeRotation = FRotator::ZeroRotator;
};
