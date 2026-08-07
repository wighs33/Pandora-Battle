#include "UI/Presenter/InfoTabPresenterBase.h"

#include "Mode/PdPlayerController.h"
#include "Mode/PdPlayerState.h"
#include "UI/Widget/InfoWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoTabPresenterBase)

UWorld* UInfoTabPresenterBase::GetWorld() const
{
	if (const APdPlayerController* Controller = GetController())
	{
		return Controller->GetWorld();
	}

	return Super::GetWorld();
}

void UInfoTabPresenterBase::Initialize(APdPlayerController* InController)
{
	OwningController = InController;
}

void UInfoTabPresenterBase::BindInfoUi(UInfoWidget* InInfoWidget)
{
	BoundInfoWidget = InInfoWidget;
}

void UInfoTabPresenterBase::Deinitialize()
{
	BoundInfoWidget = nullptr;
	OwningController = nullptr;
}

APdPlayerController* UInfoTabPresenterBase::GetController() const
{
	return OwningController.Get();
}

APdPlayerState* UInfoTabPresenterBase::GetPlayerState() const
{
	const APdPlayerController* Controller = GetController();
	return Controller ? Controller->GetPlayerState<APdPlayerState>() : nullptr;
}

UInfoWidget* UInfoTabPresenterBase::GetInfoWidget() const
{
	return BoundInfoWidget.Get();
}
