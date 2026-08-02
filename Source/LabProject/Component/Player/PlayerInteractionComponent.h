#pragma once

#include "Components/BoxComponent.h"
#include "Interface/InteractableInterface.h"

#include "PlayerInteractionComponent.generated.h"

class APdPlayer;
class UAnimMontage;
struct FPlayerInteractionSettings;

/**
 * Interaction sensor and networked interaction-montage owner.
 *
 * The component keeps the legacy "InteractionBox" subobject name so existing
 * Blueprint component lookups and aura policies continue to resolve it.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UPlayerInteractionComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UPlayerInteractionComponent();

	virtual void BeginPlay() override;

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
	TObjectPtr<UAnimMontage> ActiveInteractionMontage;

	UPROPERTY(Transient)
	float ServerValidationDistance = 250.0f;
};
