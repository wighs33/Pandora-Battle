#include "GameFeature/Extension/Condition/ExtensionCondition_NetworkReady.h"

#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExtensionCondition_NetworkReady)

bool FExtensionCondition_NetworkReady::IsSatisfied(AActor* Owner) const
{
	const APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn)
	{
		return false;
	}

	AController* Controller = Pawn->GetController();
	if (!Controller)
	{
		return false;
	}

	if (bRequirePlayerController && !Cast<APlayerController>(Controller))
	{
		return false;
	}

	APlayerState* PlayerState = Pawn->GetPlayerState();
	if (!PlayerState)
	{
		return false;
	}

	if (bRequirePlayerStateLinked && (!Controller->PlayerState || Controller->PlayerState != PlayerState))
	{
		return false;
	}

	if (!bRequireAbilitySystemReady)
	{
		return true;
	}

	const IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(PlayerState);
	if (!AbilitySystemOwner)
	{
		AbilitySystemOwner = Cast<IAbilitySystemInterface>(Pawn);
	}

	UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemOwner ? AbilitySystemOwner->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent || !AbilitySystemComponent->IsRegistered())
	{
		return false;
	}

	if (const UPdAbilitySystemComponent* PdAbilitySystemComponent = Cast<UPdAbilitySystemComponent>(AbilitySystemComponent))
	{
		if (!PdAbilitySystemComponent->HasAbilityActorInfoAllocated())
		{
			return false;
		}
	}

	for (const TSubclassOf<UAttributeSet>& AttributeSetClass : RequiredAttributeSets)
	{
		if (AttributeSetClass && !AbilitySystemComponent->GetAttributeSet(AttributeSetClass))
		{
			return false;
		}
	}

	return true;
}
