#include "AI/Training/BTTask_TrainingBotMoveBase.h"
#include "AI/Training/TrainingBotMovement.h"

#include "AIController.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BTTask_TrainingBotMoveBase)

bool UBTTask_TrainingBotMoveBase::BuildMoveDestination(APawn* Pawn, AActor* TargetActor, FVector& OutDestination) const
{
	if (!Pawn || !TargetActor)
	{
		return false;
	}

	OutDestination = TrainingBotMovement::BuildSideStepDestination(
		*Pawn,
		*TargetActor,
		MinDistanceFromTarget,
		MaxDistanceFromTarget,
		MinSideStepMultiplier,
		MaxSideStepMultiplier,
		SideStepDistance);
	if (bProjectDestinationToNavigation)
	{
		OutDestination = TrainingBotMovement::ProjectToNavigation(Pawn->GetWorld(), OutDestination, NavigationProjectionExtent);
	}
	return true;
}

void UBTTask_TrainingBotMoveBase::UpdateFacing(AAIController* AIController, APawn* Pawn, AActor* TargetActor, float DeltaSeconds) const
{
	if (bFaceTargetWhileMoving && AIController && Pawn && TargetActor)
	{
		TrainingBotMovement::FaceTarget(*AIController, *Pawn, *TargetActor, DeltaSeconds, FaceTargetRotationInterpSpeed);
	}
}
