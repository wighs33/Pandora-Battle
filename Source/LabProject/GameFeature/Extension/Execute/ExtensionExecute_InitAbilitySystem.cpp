#include "GameFeature/Extension/Execute/ExtensionExecute_InitAbilitySystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemInterface.h"
#include "Character/CharacterBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExtensionExecute_InitAbilitySystem)

void FExtensionExecute_InitAbilitySystem::OnActivate(AActor* Owner) const
{
	if (ACharacterBase* Character = Cast<ACharacterBase>(Owner))
	{
		Character->InitializeAbilitySystemActorInfo();
		return;
	}

	APawn* Pawn = Cast<APawn>(Owner);
	AActor* OwnerActor = Pawn ? Pawn->GetPlayerState() : Owner;
	AActor* AvatarActor = Owner;

	if (!OwnerActor || !AvatarActor)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = nullptr;
	if (IAbilitySystemInterface* AbilitySystemOwner = Cast<IAbilitySystemInterface>(OwnerActor))
	{
		AbilitySystemComponent = AbilitySystemOwner->GetAbilitySystemComponent();
	}

	if (!AbilitySystemComponent)
	{
		AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	}

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(OwnerActor, AvatarActor);
	}
}

void FExtensionExecute_InitAbilitySystem::OnDeactivate(AActor* Owner) const
{
	if (ACharacterBase* Character = Cast<ACharacterBase>(Owner))
	{
		Character->ClearAbilitySystemActorInfo();
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner);
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->ClearActorInfo();
	}
}
