#include "AI/StateTree_PdDistanceConditions.h"

#include "GameFramework/Actor.h"
#include "StateTreeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTree_PdDistanceConditions)

#define LOCTEXT_NAMESPACE "LabProjectStateTree"

#if WITH_EDITOR
namespace
{
FText GetBoundOrNumericValue(
	const FGuid& ID,
	const FName PropertyName,
	const double Value,
	const IStateTreeBindingLookup& BindingLookup,
	const EStateTreeNodeFormatting Formatting)
{
	FText Result = BindingLookup.GetBindingSourceDisplayName(
		FPropertyBindingPath(ID, PropertyName),
		Formatting);

	if (Result.IsEmpty())
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 0;
		Options.MaximumFractionalDigits = 2;
		Result = FText::AsNumber(Value, &Options);
	}

	return Result;
}
}
#endif

bool FStateTreePdPlayerDistanceCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.Actor) || !IsValid(InstanceData.TargetActor))
	{
		return false;
	}

	const double DistanceSquared = FVector::DistSquared2D(
		InstanceData.TargetActor->GetActorLocation(),
		InstanceData.Actor->GetActorLocation());

	return DistanceSquared > FMath::Square(InstanceData.TriggerDistance);
}

#if WITH_EDITOR
FText FStateTreePdPlayerDistanceCondition::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	const EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	FText TargetActor = BindingLookup.GetBindingSourceDisplayName(
		FPropertyBindingPath(
			ID,
			GET_MEMBER_NAME_CHECKED(FInstanceDataType, TargetActor)),
		Formatting);
	if (TargetActor.IsEmpty())
	{
		TargetActor = IsValid(InstanceData->TargetActor)
			? FText::FromString(InstanceData->TargetActor->GetActorNameOrLabel())
			: LOCTEXT("TargetActor", "Target Actor");
	}

	const FText TriggerDistance = GetBoundOrNumericValue(
		ID,
		GET_MEMBER_NAME_CHECKED(FInstanceDataType, TriggerDistance),
		InstanceData->TriggerDistance,
		BindingLookup,
		Formatting);

	return Formatting == EStateTreeNodeFormatting::RichText
		? FText::Format(
			LOCTEXT(
				"PlayerDistanceRich",
				"<b>{0}</> <s>is farther than</> {1} cm <s>from Actor</>"),
			TargetActor,
			TriggerDistance)
		: FText::Format(
			LOCTEXT(
				"PlayerDistance",
				"{0} is farther than {1} cm from Actor"),
			TargetActor,
			TriggerDistance);
}
#endif

bool FStateTreePdTargetDistanceCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	if (!IsValid(InstanceData.Actor))
	{
		return false;
	}

	const double DistanceSquared = FVector::DistSquared2D(
		InstanceData.Actor->GetActorLocation(),
		InstanceData.TargetLocation);
	const bool bIsFartherAway = DistanceSquared > FMath::Square(InstanceData.Distance);

	return InstanceData.bFartherAway ? bIsFartherAway : !bIsFartherAway;
}

#if WITH_EDITOR
FText FStateTreePdTargetDistanceCondition::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	const EStateTreeNodeFormatting Formatting) const
{
	const FInstanceDataType* InstanceData = InstanceDataView.GetPtr<FInstanceDataType>();
	check(InstanceData);

	const FText Distance = GetBoundOrNumericValue(
		ID,
		GET_MEMBER_NAME_CHECKED(FInstanceDataType, Distance),
		InstanceData->Distance,
		BindingLookup,
		Formatting);

	if (Formatting == EStateTreeNodeFormatting::RichText)
	{
		return InstanceData->bFartherAway
			? FText::Format(
				LOCTEXT(
					"TargetFartherRich",
					"<b>Actor</> <s>is farther away than</> {0} cm <s>from target location</>"),
				Distance)
			: FText::Format(
				LOCTEXT(
					"TargetCloserRich",
					"<b>Actor</> <s>is closer than</> {0} cm <s>from target location</>"),
				Distance);
	}

	return InstanceData->bFartherAway
		? FText::Format(
			LOCTEXT(
				"TargetFarther",
				"Actor is farther away than {0} cm from target location"),
			Distance)
		: FText::Format(
			LOCTEXT(
				"TargetCloser",
				"Actor is closer than {0} cm from target location"),
			Distance);
}
#endif

#undef LOCTEXT_NAMESPACE
