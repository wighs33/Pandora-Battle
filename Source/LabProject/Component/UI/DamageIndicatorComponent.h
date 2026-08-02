#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DamageIndicatorComponent.generated.h"

class ADamageIndicatorActor;

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UDamageIndicatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDamageIndicatorComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!DamageIndicator")
	void ShowDamageIndicator(float DamageAmount, FVector WorldLocation, bool bCriticalHit = false);

	UFUNCTION(BlueprintPure, Category = "!DamageIndicator")
	FVector ResolveDamageIndicatorWorldLocation() const;

protected:
	bool ResolveCameraAxes(FVector& OutCameraUp, FVector& OutCameraRight) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ADamageIndicatorActor> DamageIndicatorActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator", meta = (AllowPrivateAccess = "true"))
	FName AnchorSocketName = TEXT("neck_01");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator", meta = (AllowPrivateAccess = "true"))
	FVector FallbackWorldOffset = FVector(0.0f, 0.0f, 120.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Spawn", meta = (AllowPrivateAccess = "true"))
	float InitialCameraUpOffset = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Spawn", meta = (AllowPrivateAccess = "true"))
	float InitialCameraRightOffset = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Movement", meta = (AllowPrivateAccess = "true"))
	float EndCameraUpOffsetMin = 40.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Movement", meta = (AllowPrivateAccess = "true"))
	float EndCameraUpOffsetMax = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Movement", meta = (AllowPrivateAccess = "true"))
	float EndCameraRightOffsetMin = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Movement", meta = (AllowPrivateAccess = "true"))
	float EndCameraRightOffsetMax = 30.0f;
};
