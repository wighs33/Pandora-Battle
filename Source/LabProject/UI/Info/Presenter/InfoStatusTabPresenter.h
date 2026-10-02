#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Info/Presenter/InfoTabPresenterBase.h"
#include "InfoStatusTabPresenter.generated.h"

UCLASS()
class LABPROJECT_API UInfoStatusTabPresenter : public UInfoTabPresenterBase
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	virtual void BindInfoUi(UInfoWidget* InInfoWidget) override;
	virtual void Deinitialize() override;

	void Activate();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleStatUpClicked(FGameplayTag StatTag);

	UFUNCTION()
	void HandleStatDownClicked(FGameplayTag StatTag);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void BindEvents();
	void UnbindEvents();
};
