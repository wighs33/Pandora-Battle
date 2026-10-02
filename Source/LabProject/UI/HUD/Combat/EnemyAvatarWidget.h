#pragma once

#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"

#include "EnemyAvatarWidget.generated.h"

class AActor;
class UImage;
class UStatusEffectsBarWidget;
class UTexture2D;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UEnemyAvatarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Avatar")
	void SetOwnerActor(AActor* InOwnerActor);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandleAvatarUpdateTick();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void StartAvatarUpdateTimer();
	void StopAvatarUpdateTimer();
	void PropagateOwnerActorToChildren();
	void ApplyLocalPlayerPresentation();
	void RestoreOriginalWidgetVisibilities();
	void RefreshAvatarImage();
	void RestoreDefaultAvatarBrush(UImage* TargetAvatarImage);
	bool IsPlayerOwner() const;
	bool IsLocalPlayerOwner() const;
	UTexture2D* ResolvePlayerAchievementTexture() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Enemy|Avatar")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Enemy|Avatar")
	TObjectPtr<UImage> AvatarImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Enemy|Avatar")
	TObjectPtr<UStatusEffectsBarWidget> StatusEffectsBar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Enemy|Avatar", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float AvatarUpdateInterval = 0.1f;

private:
	TMap<TWeakObjectPtr<UWidget>, ESlateVisibility> OriginalWidgetVisibilities;
	FSlateBrush DefaultAvatarBrush;
	bool bHasDefaultAvatarBrush = false;
	bool bLocalPlayerPresentationInitialized = false;
	bool bLastLocalPlayerOwner = false;

	FTimerHandle AvatarUpdateTimerHandle;
};
