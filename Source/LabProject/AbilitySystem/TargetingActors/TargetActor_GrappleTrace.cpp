#include "AbilitySystem/TargetingActors/TargetActor_GrappleTrace.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Character/PdPlayer.h"
#include "Component/Player/GrappleComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TargetActor_GrappleTrace)

ATargetActor_GrappleTrace::ATargetActor_GrappleTrace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ShouldProduceTargetDataOnServer = false;
	bDestroyOnConfirmation = true;
	SetReplicates(false);
}

void ATargetActor_GrappleTrace::StartTargeting(UGameplayAbility* Ability)
{
	Super::StartTargeting(Ability);
	SourceActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
}

void ATargetActor_GrappleTrace::ConfirmTargetingAndContinue()
{
	if (!ShouldProduceTargetData())
	{
		return;
	}

	APdPlayer* Player = Cast<APdPlayer>(SourceActor.Get());
	UGrappleComponent* GrappleComponent = Player ? Player->GetGrappleComponent() : nullptr;
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ZeroVector;
	FHitResult GrappleHit;

	if (!Player
		|| !GrappleComponent
		|| !Player->GetWeaponAimViewPoint(ViewLocation, ViewDirection)
		|| !GrappleComponent->TraceGrappleFromView(ViewLocation, ViewDirection, GrappleHit))
	{
		CanceledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());
		return;
	}

	FGameplayAbilityTargetDataHandle TargetData(
		new FGameplayAbilityTargetData_SingleTargetHit(GrappleHit));
	TargetDataReadyDelegate.Broadcast(TargetData);
}
