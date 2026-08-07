#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/Presenter/InfoTabPresenterBase.h"
#include "InfoStatusTabPresenter.generated.h"

UCLASS()
class LABPROJECT_API UInfoStatusTabPresenter : public UInfoTabPresenterBase
{
	GENERATED_BODY()

public:
	virtual void BindInfoUi(UInfoWidget* InInfoWidget) override;
	virtual void Deinitialize() override;

	void Activate();
	void RequestStatUp(FGameplayTag StatTag);
	void RequestStatDown(FGameplayTag StatTag);

private:
	void BindEvents();
	void UnbindEvents();

	UFUNCTION()
	void HandleStatUpClicked(FGameplayTag StatTag);

	UFUNCTION()
	void HandleStatDownClicked(FGameplayTag StatTag);
};
