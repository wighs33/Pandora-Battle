#include "Pet/PetCharacter.h"

#include "AI/PetAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PetCharacter)

APetCharacter::APetCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(true);

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = APetAIController::StaticClass();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = 450.0f;
		MovementComponent->bOrientRotationToMovement = true;
	}

	bUseControllerRotationYaw = false;
}

void APetCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(APetCharacter, FollowTargetActor, Params);
}

void APetCharacter::SetFollowTargetActor(AActor* InFollowTargetActor)
{
	if (FollowTargetActor == InFollowTargetActor)
	{
		if (APetAIController* PetController = Cast<APetAIController>(GetController()))
		{
			PetController->RefreshFollowTarget();
		}
		return;
	}

	FollowTargetActor = InFollowTargetActor;
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(APetCharacter, FollowTargetActor, this);
	}

	if (APetAIController* PetController = Cast<APetAIController>(GetController()))
	{
		PetController->RefreshFollowTarget();
	}
}
