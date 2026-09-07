#pragma once

#include "Components/BoxComponent.h"
#include "Interface/InteractableInterface.h"

#include "PlayerInteractionComponent.generated.h"

class APdPlayer;
class UAnimMontage;
struct FPlayerInteractionSettings;

/**
 * 주변 상호작용 대상과 상호작용 몽타주의 실행 상태를 관리한다.
 *
 * 기존 Blueprint와 오라 탐색을 위해 센서의 하위 객체 이름은 InteractionBox로 유지한다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerInteractionComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UPlayerInteractionComponent();

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//------------------------------------------------------------------------------------------------------------------

	void ApplySettings(const FPlayerInteractionSettings& Settings);

	bool HasCurrentInteractActors(
		TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const;
	AActor* GetCurrentInteractActor() const;
	bool InteractWithCurrentTarget();
	bool CanInteractWithActor(AActor* InteractableActor) const;

	void PlayInteractionMontage(UAnimMontage* Montage, float PlayRate);
	void StopInteractionMontage(float BlendOutTime);
	bool IsInteractionMontagePlaying() const;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayInteractionMontage(UAnimMontage* Montage, float PlayRate);

	UFUNCTION(Server, Reliable)
	void ServerStopInteractionMontage(float BlendOutTime);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopInteractionMontage(float BlendOutTime);

	APdPlayer* GetPlayerOwner() const;
	bool TryMakeInteractableEntry(
		AActor* OtherActor,
		TScriptInterface<IInteractableInterface>& OutInteractableActor) const;
	bool StopInteractionMontageLocally(float BlendOutTime);

	UPROPERTY(Transient)
	TArray<TScriptInterface<IInteractableInterface>> CurrentInteractActors;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveInteractionMontage;

	UPROPERTY(Transient)
	float ServerValidationDistance = 250.0f;
};
