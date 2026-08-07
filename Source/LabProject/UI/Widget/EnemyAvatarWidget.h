#pragma once

#include <initializer_list>

#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"

#include "EnemyAvatarWidget.generated.h"

class AActor;
class UEnemyShieldBarWidget;
class UEnemyHealthBarWidget;
class UImage;
class UStatusEffectsBarWidget;
class UUserWidget;
class UWidget;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UEnemyAvatarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Avatar")
	void SetOwnerActor(AActor* InOwnerActor);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Enemy|Avatar")
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Enemy|Avatar")
	TObjectPtr<UImage> AvatarImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Enemy|Avatar")
	TObjectPtr<UStatusEffectsBarWidget> StatusEffectsBar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Enemy|Avatar", meta = (ClampMin = "0.02", ForceUnits = "s"))
	float AvatarUpdateInterval = 0.1f;

private:
	void StartAvatarUpdateTimer();
	void StopAvatarUpdateTimer();
	void HandleAvatarUpdateTick();
	void PropagateOwnerActorToChildren();
	void ApplyLocalPlayerPresentation();
	void RestoreOriginalWidgetVisibilities();
	void RefreshAvatarImage();
	bool IsPlayerOwner() const;
	bool IsLocalPlayerOwner() const;
	bool FindPlayerAchievementBrush(FSlateBrush& OutBrush) const;
	UUserWidget* FindChildUserWidget(FName WidgetName) const;
	UUserWidget* FindFirstChildUserWidget(std::initializer_list<FName> WidgetNames) const;
	UImage* ResolveAvatarImage() const;
	UImage* ResolveAchievementSourceImage() const;
	UImage* FindImageInUserWidget(UUserWidget* RootWidget, FName ImageName) const;
	UImage* FindImageInWidget(UWidget* RootWidget, FName ImageName) const;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CachedAvatarImage;

	UPROPERTY(Transient)
	TWeakObjectPtr<UImage> CachedAchievementSourceImage;

	TMap<TWeakObjectPtr<UWidget>, ESlateVisibility> OriginalWidgetVisibilities;
	bool bLocalPlayerPresentationInitialized = false;
	bool bLastLocalPlayerOwner = false;

	FTimerHandle AvatarUpdateTimerHandle;
};
