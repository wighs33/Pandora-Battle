#pragma once

#include <initializer_list>

#include "Blueprint/UserWidget.h"

#include "EnemyAvatarWidget.generated.h"

class AActor;
class UEnemyShieldBarWidget;
class UEnemyHealthBarWidget;
class UStatusEffectsBarWidget;
class UUserWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UEnemyAvatarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Avatar")
	void SetOwnerActor(AActor* InOwnerActor);

	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Avatar")
	void UpdateWidgetSize();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Enemy|Avatar")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Enemy|Avatar")
	float MinDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Enemy|Avatar")
	float MaxDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Enemy|Avatar")
	float MinRenderScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Enemy|Avatar")
	float MaxRenderScale = 1.0f;

private:
	void PropagateOwnerActorToChildren();
	APawn* ResolveLocalPlayerPawn() const;
	UUserWidget* FindChildUserWidget(FName WidgetName) const;
	UUserWidget* FindFirstChildUserWidget(std::initializer_list<FName> WidgetNames) const;
};
