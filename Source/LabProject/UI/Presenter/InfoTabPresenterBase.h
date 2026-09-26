#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InfoTabPresenterBase.generated.h"

class APdPlayerController;
class APdPlayerState;
class UInfoWidget;

/** UInfoUiPresenter가 소유한 프레젠터의 공통 수명과 뷰 컨텍스트를 제공한다. */
UCLASS(Abstract)
class LABPROJECT_API UInfoTabPresenterBase : public UObject
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual UWorld* GetWorld() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	virtual void Initialize(APdPlayerController* InController);
	virtual void BindInfoUi(UInfoWidget* InInfoWidget);
	virtual void Deinitialize();

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	APdPlayerController* GetController() const;
	APdPlayerState* GetPlayerState() const;
	UInfoWidget* GetInfoWidget() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<APdPlayerController> OwningController;

	UPROPERTY(Transient)
	TObjectPtr<UInfoWidget> BoundInfoWidget;
};
