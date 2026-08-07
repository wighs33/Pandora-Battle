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
class UPandoraInstance;
struct FStreamableHandle;
enum class EInfoLoadoutStateChange : uint8;

/**
 * Stable top-level presenter facade used by the HUD and Blueprint configuration.
 * Tab-specific behavior lives in the four presenters owned by this object.
 */
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UInfoUiPresenter : public UObject
{
	GENERATED_BODY()

public:
	UInfoUiPresenter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual UWorld* GetWorld() const override;

	void Initialize(APdPlayerController* InController);
	void Deinitialize();
	void BindInfoUi(UInfoWidget* InInfoWidget);
	void UnbindInfoUi(const UInfoWidget* ExpectedInfoWidget = nullptr);
	UItemInstance* GetSelectedWeapon(EEnum_Direction Direction) const;
	UPandoraInstance* GetSelectedPandora(EEnum_Direction Direction) const;
	bool WouldSelectedPandoraDirectionChangeLoadout(EEnum_Direction Direction) const;

	UFUNCTION()
	void HandleSelectedPandoraDirection(EEnum_Direction Direction);

	UFUNCTION()
	void HandleOpenedInfoUi();

	UFUNCTION()
	void HandleClickedInfoCenterButton(FGameplayTag LeftUiTag, FGameplayTag RightUiTag);

private:
	void EnsureTabPresenters();
	void BindLoadoutStateNotification();
	void UnbindLoadoutStateNotification();
	void BeginItemPresentationPreload();
	void ReleaseItemPresentationPreload();
	void SetActiveTab(FGameplayTag LeftUiTag);

	void HandleLoadoutStateChanged(EInfoLoadoutStateChange Change);

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
