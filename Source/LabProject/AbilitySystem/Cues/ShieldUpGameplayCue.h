#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "ShieldUpGameplayCue.generated.h"

class UMaterialInterface;
class USkeletalMeshComponent;
class USoundBase;

UCLASS(Blueprintable)
class LABPROJECT_API AShieldUpGameplayCue : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AShieldUpGameplayCue();

	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Shield")
	TObjectPtr<UMaterialInterface> ShieldOverlayMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!GameplayCue|Shield")
	TObjectPtr<USoundBase> ShieldUpSound;

private:
	USkeletalMeshComponent* ResolveSkeletalMesh(AActor* MyTarget) const;
	bool ApplyShieldOverlay(AActor* MyTarget, UMaterialInterface* OverlayMaterial, bool bPlaySound) const;
};
