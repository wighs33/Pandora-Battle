#include "GameFeature/Extension/Condition/ExtensionCondition_HasInputComponent.h"

#include "Components/InputComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExtensionCondition_HasInputComponent)

bool FExtensionCondition_HasInputComponent::IsSatisfied(AActor* Owner) const
{
	UInputComponent* InputComponent = nullptr;

	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		InputComponent = Pawn->InputComponent;
	}
	else if (const AController* Controller = Cast<AController>(Owner))
	{
		InputComponent = Controller->InputComponent;
	}

	if (!InputComponent)
	{
		return false;
	}

	return !RequiredClass || InputComponent->IsA(RequiredClass);
}
