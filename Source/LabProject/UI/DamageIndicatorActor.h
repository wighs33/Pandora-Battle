#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageIndicatorActor.generated.h"

class UCurveFloat;
class USceneComponent;
class UWidgetComponent;

USTRUCT(BlueprintType)
struct LABPROJECT_API FDamageIndicatorPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!DamageIndicator")
	float DamageAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!DamageIndicator")
	bool bCriticalHit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!DamageIndicator")
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!DamageIndicator")
	FVector EndLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!DamageIndicator")
	TObjectPtr<AActor> DamagedActor = nullptr;
};

UCLASS(Blueprintable)
class LABPROJECT_API ADamageIndicatorActor : public AActor
{
	GENERATED_BODY()

public:
	ADamageIndicatorActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Timing hooks
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Public API
	UFUNCTION(BlueprintCallable, Category = "!DamageIndicator")
	void InitializeDamageIndicator(const FDamageIndicatorPayload& InPayload);

	UFUNCTION(BlueprintPure, Category = "!DamageIndicator")
	const FDamageIndicatorPayload& GetDamageIndicatorPayload() const { return Payload; }

protected:
	// Blueprint timing events
	UFUNCTION(BlueprintImplementableEvent, Category = "!DamageIndicator", meta = (DisplayName = "On Damage Indicator Initialized"))
	void ReceiveDamageIndicatorInitialized(const FDamageIndicatorPayload& InPayload);

	UFUNCTION(BlueprintImplementableEvent, Category = "!DamageIndicator", meta = (DisplayName = "On Damage Indicator Updated"))
	void ReceiveDamageIndicatorUpdated(float Alpha);

	UFUNCTION(BlueprintImplementableEvent, Category = "!DamageIndicator", meta = (DisplayName = "On Damage Indicator Finished"))
	void ReceiveDamageIndicatorFinished();

	void ApplyPayloadToWidget();
	void StartMovement();
	void FinishMovement();
	void UpdateFacing();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!DamageIndicator", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!DamageIndicator", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> DamageWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Movement", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float Duration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCurveFloat> MovementAlphaCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Movement", meta = (AllowPrivateAccess = "true"))
	bool bDestroyOnFinish = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Facing", meta = (AllowPrivateAccess = "true"))
	bool bFaceLocalCamera = true;

	UPROPERTY(BlueprintReadOnly, Category = "!DamageIndicator")
	FDamageIndicatorPayload Payload;

private:
	bool bPayloadInitialized = false;
	bool bMovementActive = false;
	float ElapsedTime = 0.0f;
};
