#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class USceneComponent;
class UStaticMeshComponent;

DECLARE_LOG_CATEGORY_EXTERN(WeaponBaseLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UStaticMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	USceneComponent* GetTrailStartPoint() const { return TrailStartPoint; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	USceneComponent* GetTrailEndPoint() const { return TrailEndPoint; }

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trail")
	TObjectPtr<USceneComponent> TrailStartPoint;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trail")
	TObjectPtr<USceneComponent> TrailEndPoint;
};
