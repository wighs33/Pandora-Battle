#pragma once

#include "Components/WidgetComponent.h"
#include "CharacterHealthBarComponent.generated.h"

class ACharacterBase;
class APlayerController;
class UAbilitySystemComponent;
class UHealthBarViewModel;
class UUserWidget;

/**
 * 모든 ACharacterBase 유형의 월드 체력바 위젯·뷰 모델·로컬 표시 정책과
 * 횟수가 제한된 초기화 재시도를 관리한다.
 */
UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class LABPROJECT_API UCharacterHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
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
	// Event Handlers --------------------------------------------------------------------------------------------------
	void RetryRefreshViewModel();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	ACharacterBase* GetCharacterOwner() const;
	void ConfigureWidget();
	bool TryApplyViewModelToWidget(UUserWidget* InWidget);
	bool TryApplyViewModelToWidget();
	bool BindViewModelToASC(UAbilitySystemComponent* AbilitySystemComponent);
	bool IsAttributeDataReady(const UAbilitySystemComponent* AbilitySystemComponent) const;
	void QueueViewModelRefreshRetry();
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

private:
	UPROPERTY(Transient)
	TObjectPtr<UHealthBarViewModel> HealthBarViewModel;

	FTimerHandle ViewModelRetryTimerHandle;

	UPROPERTY(Transient)
	int32 ViewModelRetryCount = 0;

	UPROPERTY(Transient)
	double LastVisibleTimeSeconds = 0.0;
};
