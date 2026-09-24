#pragma once

#include "CoreMinimal.h"
#include "ViewModel/CommonViewModelBase.h"
#include "PandoraTreeViewModel.generated.h"

UCLASS(BlueprintType)
class LABPROJECT_API UPandoraTreeViewModel : public UCommonViewModelBase
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UPandoraTreeViewModel();

	void ResetViewData();
	void SetPointsAvailable(int32 InPointsAvailable);
	void SetPandoraPointsText(const FText& InPandoraPointsText);

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Tree ViewModel")
	int32 PointsAvailable = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "!Pandora Tree ViewModel")
	FText PandoraPointsText;

	static const FName ViewModelName;
};
