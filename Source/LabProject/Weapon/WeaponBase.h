#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class APdCharacterBase;

/**
 * <무기 베이스 액터>
 * - 무기 충돌을 처리합니다.
 * - 타격 대상을 판정합니다.
 * - 서버 데미지 적용을 요청합니다.
 */
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	/** 무기 기본 상태를 초기화합니다. */
	AWeaponBase();

	/** 서버에 데미지 적용을 요청합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Weapon|Damage")
	void RequestServerApplyDamage(AActor* TargetActor);

	/** BeginOverlap 판정을 켜거나 끕니다. */
	UFUNCTION(BlueprintCallable, Category = "!Weapon|Collision")
	void SetBeginOverlapEnabled(bool bEnabled);

	/** 루트 컴포넌트입니다. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon")
	TObjectPtr<USceneComponent> SceneRoot;

	/** 무기 메시 컴포넌트입니다. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	/** 타격 박스 컴포넌트입니다. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon")
	TObjectPtr<UBoxComponent> Box;

protected:
	/** 서버에서 데미지를 적용합니다. */
	UFUNCTION(Server, Reliable)
	void ServerApplyDamage(AActor* TargetActor);

	/** 박스 BeginOverlap을 처리합니다. */
	UFUNCTION()
	void HandleBoxBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	/** 소유 캐릭터를 반환합니다. */
	APdCharacterBase* GetOwningCharacter() const;

	/** 현재 오버랩을 처리할 수 있는지 반환합니다. */
	bool CanProcessOverlapWith(AActor* OtherActor, UPrimitiveComponent* OtherComp) const;

	/** 오버랩 대상을 BoxTrace로 다시 확인합니다. */
	bool TryTraceOverlapTarget(UPrimitiveComponent* OtherComp, FHitResult& OutHitResult) const;

	/** 성공한 타격을 디버그 출력합니다. */
	void DebugSuccessfulHit(const FHitResult& HitResult) const;

	/** 대상에게 실제 데미지를 적용합니다. */
	void ApplyDamageToTarget(AActor* TargetActor);

	/** 현재 공격에서 이미 맞은 액터 목록입니다. */
	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActorsInCurrentAttack;
};