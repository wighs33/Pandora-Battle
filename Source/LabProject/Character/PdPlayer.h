#pragma once

#include "Character/PdCharacterBase.h"
#include "Interface/InteractableInterface.h"
#include "PdPlayer.generated.h"

class UBoxComponent;
class UCameraComponent;
class UPrimitiveComponent;
class USpringArmComponent;
class APdPlayerState;

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

	/** PlayerState 기준 ASC를 반환합니다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** 현재 상호작용 대상이 있는지 반환합니다. */
	UFUNCTION(BlueprintCallable, Category = "!Interaction", meta = (DisplayName = "HasCurrentInteractActors?"))
	bool HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const;

	bool CanInteractWithActor(AActor* InteractableActor) const;

protected:
	/** ASC 소유 액터를 반환합니다. */
	virtual AActor* GetAbilitySystemOwnerActor() const override;

	/** PlayerState를 프로젝트 타입으로 반환합니다. */
	APdPlayerState* GetPdPlayerState() const;

	/** 액터를 상호작용 엔트리로 변환합니다. */
	bool TryMakeInteractableEntry(AActor* OtherActor, TScriptInterface<IInteractableInterface>& OutInteractableActor) const;

	/** 상호작용 박스 진입을 처리합니다. */
	UFUNCTION()
	void HandleInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 상호작용 박스 이탈을 처리합니다. */
	UFUNCTION()
	void HandleInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

protected:
	/** 현재 상호작용 가능한 액터 목록입니다. */
	UPROPERTY(BlueprintReadWrite, Transient, Category = "!Interaction")
	TArray<TScriptInterface<IInteractableInterface>> CurrentInteractActors;

	/** 상호작용 감지 박스입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionBox;

	/** 카메라 붐입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 추적 카메라입니다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
};
