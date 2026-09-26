#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "InfoUiPresenter.generated.h"

class APdPlayerController;
class UInfoItemTabPresenter;
class UInfoLoadoutStore;
class UInfoPandoraTabPresenter;
class UInfoSkinTabPresenter;
class UInfoStatusTabPresenter;
class UInfoWidget;
class UItemInstance;
struct FStreamableHandle;
enum class EInfoLoadoutStateChange : uint8;

/**
 * HUD와 블루프린트 설정이 사용하는 최상위 프레젠터 진입점을 제공한다.
 * 탭별 동작은 이 객체가 소유한 네 개의 프레젠터에서 처리한다.
 */
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UInfoUiPresenter : public UObject
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual UWorld* GetWorld() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	UInfoUiPresenter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void Initialize(APdPlayerController* InController);
	void Deinitialize();
	void BindInfoUi(UInfoWidget* InInfoWidget);
	void UnbindInfoUi(const UInfoWidget* ExpectedInfoWidget = nullptr);
	UItemInstance* GetSelectedWeapon(EEnum_Direction Direction) const;
	bool WouldSelectedPandoraDirectionChangeLoadout(EEnum_Direction Direction) const;

	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleSelectedPandoraDirection(EEnum_Direction Direction);

	UFUNCTION()
	void HandleOpenedInfoUi();

	UFUNCTION()
	void HandleClickedInfoCenterButton(FGameplayTag LeftUiTag, FGameplayTag RightUiTag);

private:
	void HandleLoadoutStateChanged(EInfoLoadoutStateChange Change);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void EnsureTabPresenters();
	void BindLoadoutStateNotification();
	void UnbindLoadoutStateNotification();
	void BeginItemPresentationPreload();
	void ReleaseItemPresentationPreload();
	void SetActiveTab(FGameplayTag LeftUiTag);

private:
	UPROPERTY(Transient)
	TObjectPtr<APdPlayerController> OwningController;
	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> InfoWidget;
	UPROPERTY(Transient)
	TObjectPtr<UInfoLoadoutStore> LoadoutStore;
	UPROPERTY(Transient)
	TObjectPtr<UInfoStatusTabPresenter> StatusPresenter;
	UPROPERTY(Transient)
	TObjectPtr<UInfoItemTabPresenter> ItemPresenter;
	UPROPERTY(Transient)
	TObjectPtr<UInfoSkinTabPresenter> SkinPresenter;
	UPROPERTY(Transient)
	TObjectPtr<UInfoPandoraTabPresenter> PandoraPresenter;
	FDelegateHandle LoadoutStateChangedDelegateHandle;
	int32 ItemPresentationPreloadGeneration = 0;
	TSharedPtr<FStreamableHandle> ItemPresentationPreloadHandle;
};
