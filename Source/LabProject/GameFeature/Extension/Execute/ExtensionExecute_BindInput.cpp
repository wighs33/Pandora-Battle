#include "GameFeature/Extension/Execute/ExtensionExecute_BindInput.h"

#include "AssetRegistry/AssetBundleData.h"
#include "GameFeaturesSubsystemSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerController.h"
#include "PlayerComponent/ControllerInputComponent.h"
#include "PlayerComponent/ControllerInputDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExtensionExecute_BindInput)

void FExtensionExecute_BindInput::OnActivate(AActor* Owner) const
{
	if (InputDefinition.IsNull())
	{
		return;
	}

	UControllerInputComponent* InputComponent = FindControllerInputComponent(Owner);
	if (!InputComponent)
	{
		return;
	}

	WeakInputComponent = InputComponent;
	PreviousInputDefinition = InputComponent->GetInputDefinition();
	InputComponent->SetInputDefinition(InputDefinition);
}

void FExtensionExecute_BindInput::OnDeactivate(AActor* Owner) const
{
	UControllerInputComponent* InputComponent = WeakInputComponent.Get();
	if (!InputComponent)
	{
		InputComponent = FindControllerInputComponent(Owner);
	}

	if (InputComponent)
	{
		InputComponent->SetInputDefinition(PreviousInputDefinition);
	}

	WeakInputComponent.Reset();
	PreviousInputDefinition.Reset();
}

#if WITH_EDITORONLY_DATA
void FExtensionExecute_BindInput::AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) const
{
	if (!InputDefinition.IsNull())
	{
		AssetBundleData.AddBundleAsset(
			UGameFeaturesSubsystemSettings::LoadStateClient,
			InputDefinition.ToSoftObjectPath().GetAssetPath());
	}
}
#endif

UControllerInputComponent* FExtensionExecute_BindInput::FindControllerInputComponent(AActor* Owner) const
{
	if (!Owner)
	{
		return nullptr;
	}

	if (UControllerInputComponent* InputComponent = Owner->FindComponentByClass<UControllerInputComponent>())
	{
		return InputComponent;
	}

	const APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn)
	{
		return nullptr;
	}

	APlayerController* PlayerController = Pawn->GetController<APlayerController>();
	return PlayerController ? PlayerController->FindComponentByClass<UControllerInputComponent>() : nullptr;
}
