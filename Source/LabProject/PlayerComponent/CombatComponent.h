#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

class AActor;
class AWeaponBase;
class UGameplayEffect;
class UItemInstance;
struct FGameplayAttribute;

/**
 * <전투 처리 컴포넌트>
 * - 무기 데미지를 계산합니다.
 * - 무기 데미지를 대상에게 적용합니다.
 * - ASC 기반 데미지 처리를 담당합니다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 전투 컴포넌트 기본 상태를 초기화합니다. */
	UCombatComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 현재 무기 데미지 수치를 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!Combat")
	float GetCurrentWeaponDamageMagnitude();

	/** 아이템 정보로 무기 데미지를 계산합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Combat")
	bool ApplyResolvedDamageToWeapon(AWeaponBase* InWeaponActor, UItemInstance* InItemInstance);

	/** 현재 무기 데미지를 대상에게 적용합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Combat")
	bool ApplyWeaponDamageToTarget(AActor* TargetActor);


protected:
	/** 무기 데미지용 Attribute를 해석합니다. */
	bool ResolveWeaponDamageAttribute(FGameplayAttribute& OutAttribute) const;

	/** 소유 캐릭터를 반환합니다. */
	class APdCharacterBase* GetCharacterOwner() const;

	/** 소유자의 프로젝트 ASC를 반환합니다. */
	class UPdAbilitySystemComponent* GetOwnerPdAbilitySystemComponent() const;

	bool TryActivateHitReactAbilityOnTarget(class APdCharacterBase* TargetCharacter) const;

protected:
	/** 무기 데미지 계산에 사용할 스탯 태그입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Combat", meta = (Categories = "GameplayTag", AllowPrivateAccess = "true"))
	FGameplayTag WeaponDamageStatTag;

	/** 대상에게 적용할 데미지 이펙트 클래스입니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Combat", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> WeaponDamageEffectClass;


	/** 현재 계산된 무기 데미지 값입니다. */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "!Combat", meta = (AllowPrivateAccess = "true"))
	float CurrentWeaponDamageMagnitude = 0.f;
};
