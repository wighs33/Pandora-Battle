#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageIndicatorActor.generated.h"

class UCurveFloat;
class USceneComponent;
class UUserWidget;
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

/**
 * 맞은 자리에서 떠올랐다 사라지는 데미지 숫자 하나.
 * 움직임이 끝나면 지우지 않고 숨긴 채 UCombatUiPool로 돌아가고, 다음 숫자가 InitializeDamageIndicator로 다시 띄운다.
 */
UCLASS(Blueprintable)
class LABPROJECT_API ADamageIndicatorActor : public AActor
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// Public API ------------------------------------------------------------------------------------------------------
	ADamageIndicatorActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!DamageIndicator")
	void InitializeDamageIndicator(const FDamageIndicatorPayload& InPayload);

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintImplementableEvent, Category = "!DamageIndicator", meta = (DisplayName = "On Damage Indicator Initialized"))
	void ReceiveDamageIndicatorInitialized(const FDamageIndicatorPayload& InPayload);

	UFUNCTION(BlueprintImplementableEvent, Category = "!DamageIndicator", meta = (DisplayName = "On Damage Indicator Updated"))
	void ReceiveDamageIndicatorUpdated(float Alpha);

	UFUNCTION(BlueprintImplementableEvent, Category = "!DamageIndicator", meta = (DisplayName = "On Damage Indicator Finished"))
	void ReceiveDamageIndicatorFinished();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ApplyPayloadToWidget();
	void ApplyDamageOutline(UUserWidget& UserWidget);
	void StartMovement();
	void FinishMovement();
	void SetPresented(bool bPresented);
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!DamageIndicator|Facing", meta = (AllowPrivateAccess = "true"))
	bool bFaceLocalCamera = true;

	UPROPERTY(BlueprintReadOnly, Category = "!DamageIndicator")
	FDamageIndicatorPayload Payload;

private:
	bool bPayloadInitialized = false;
	bool bMovementActive = false;
	float ElapsedTime = 0.0f;

	/** 위젯 글자 테두리의 원래 색. 내 캐릭터가 맞은 숫자만 테두리 색을 바꾸므로, 다시 쓸 때 되돌릴 값으로 처음 한 번 읽어 둔다. */
	TOptional<FLinearColor> DefaultDamageOutlineColor;
};
