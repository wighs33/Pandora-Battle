#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "Sword.generated.h"

class UBoxComponent;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API ASword : public AWeaponBase
{
	GENERATED_BODY()

public:
	ASword();

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "!Weapon|Collision")
	TObjectPtr<UBoxComponent> Box;

protected:
	virtual UBoxComponent* GetCollisionBox() const override;
};
