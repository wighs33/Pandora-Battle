#pragma once

#include "Components/WidgetComponent.h"
#include "CharacterHealthBarComponent.generated.h"

class ACharacterBase;
class APlayerController;
class UAbilitySystemComponent;
class UHealthBarViewModel;
class UUserWidget;

/**
 * Owns the world health-bar widget, its view model, local visibility policy,
 * and bounded initialization retries for every ACharacterBase archetype.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UCharacterHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UCharacterHealthBarComponent();

	void InitializeHealthBar();
	void ShutdownHealthBar();
	void RefreshViewModel();
	void TryRefreshViewModel();

	void SetVisibleForLocalViewer(bool bRequestedVisible);
	void UpdateVisibilityForLocalViewer(
		APlayerController* LocalPlayerController,
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float MaxDistanceSquared);

	UHealthBarViewModel* GetHealthBarViewModel() const { return HealthBarViewModel.Get(); }

private:
	ACharacterBase* GetCharacterOwner() const;
	void ConfigureWidget();
	bool TryApplyViewModelToWidget(UUserWidget* InWidget);
	bool TryApplyViewModelToWidget();
	bool BindViewModelToASC(UAbilitySystemComponent* AbilitySystemComponent);
	bool IsAttributeDataReady(const UAbilitySystemComponent* AbilitySystemComponent) const;
	void QueueViewModelRefreshRetry();
	void RetryRefreshViewModel();
	void UpdateFacing();
	bool ShouldShowForLocalViewer(
		APlayerController* LocalPlayerController,
		const FVector& CameraLocation,
		const FRotator& CameraRotation) const;
	bool HasLineOfSight(const FVector& TraceStartLocation) const;
	FVector GetLineOfSightStartLocation(
		const APlayerController* LocalPlayerController,
		const FVector& FallbackCameraLocation) const;
	FVector GetVisibilityTargetLocation() const;

	UPROPERTY(Transient)
	TObjectPtr<UHealthBarViewModel> HealthBarViewModel;

	FTimerHandle ViewModelRetryTimerHandle;

	UPROPERTY(Transient)
	int32 ViewModelRetryCount = 0;

	UPROPERTY(Transient)
	double LastVisibleTimeSeconds = 0.0;
};
