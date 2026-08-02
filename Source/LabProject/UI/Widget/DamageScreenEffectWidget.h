#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageScreenEffectWidget.generated.h"

class UImage;
class UWidgetAnimation;

UCLASS()
class LABPROJECT_API UDamageScreenEffectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDamageScreenEffectWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "!Damage|ScreenEffect")
	void PlayDamageScreenEffect(float DamageAmount = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "!Damage|ScreenEffect")
	void HideDamageScreenEffect();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!Damage|Bind")
	TObjectPtr<UImage> Img_DamageScreenEffect = nullptr;

	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> DamageFlash = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Damage|Visual")
	FLinearColor DamageScreenTint = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Damage|Visual", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOpacity = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Damage|Timing", meta = (ClampMin = "0.01"))
	float FallbackVisibleDuration = 0.35f;

private:
	void ApplyDamageScreenTint();

	FTimerHandle HideTimerHandle;
};
