#include "Map/TransientActorRegistrySubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TransientActorRegistrySubsystem)

namespace
{
	constexpr int32 MaximumOwnershipDepth = 16;
}

void UTransientActorRegistrySubsystem::Deinitialize()
{
	ActorsBySource.Reset();
	SourceByActor.Reset();

	Super::Deinitialize();
}

void UTransientActorRegistrySubsystem::RegisterTransientActor(
	AActor* TransientActor,
	AActor* SourceActor)
{
	UWorld* World = GetWorld();
	if (!World
		|| World->GetNetMode() == NM_Client
		|| !IsValid(TransientActor)
		|| TransientActor->GetWorld() != World)
	{
		return;
	}

	if (!IsValid(SourceActor))
	{
		SourceActor = TransientActor->GetOwner();
	}
	if (!IsValid(SourceActor))
	{
		SourceActor = TransientActor->GetInstigator();
	}

	AActor* RegistrySource = ResolveRegistrySource(SourceActor);
	if (!IsValid(RegistrySource)
		|| RegistrySource == TransientActor
		|| RegistrySource->GetWorld() != World)
	{
		return;
	}

	const TWeakObjectPtr<AActor> ActorKey(TransientActor);
	const TWeakObjectPtr<AActor> SourceKey(RegistrySource);
	if (const TWeakObjectPtr<AActor>* ExistingSourceKey = SourceByActor.Find(ActorKey))
	{
		if (*ExistingSourceKey == SourceKey)
		{
			return;
		}

		UnregisterTransientActor(TransientActor);
	}

	ActorsBySource.FindOrAdd(SourceKey).Add(ActorKey);
	SourceByActor.Add(ActorKey, SourceKey);
}

void UTransientActorRegistrySubsystem::UnregisterTransientActor(AActor* TransientActor)
{
	if (!TransientActor)
	{
		return;
	}

	const TWeakObjectPtr<AActor> ActorKey(TransientActor);
	TWeakObjectPtr<AActor> SourceKey;
	if (!SourceByActor.RemoveAndCopyValue(ActorKey, SourceKey))
	{
		return;
	}

	TSet<TWeakObjectPtr<AActor>>* RegisteredActors = ActorsBySource.Find(SourceKey);
	if (!RegisteredActors)
	{
		return;
	}

	RegisteredActors->Remove(ActorKey);
	if (RegisteredActors->IsEmpty())
	{
		ActorsBySource.Remove(SourceKey);
	}
}

void UTransientActorRegistrySubsystem::GetTransientActorsForSource(
	AActor* SourceActor,
	TArray<AActor*>& OutActors)
{
	OutActors.Reset();

	AActor* RegistrySource = ResolveRegistrySource(SourceActor);
	if (!IsValid(RegistrySource))
	{
		return;
	}

	const TWeakObjectPtr<AActor> SourceKey(RegistrySource);
	TSet<TWeakObjectPtr<AActor>>* RegisteredActors = ActorsBySource.Find(SourceKey);
	if (!RegisteredActors)
	{
		return;
	}

	UWorld* World = GetWorld();
	for (auto ActorIt = RegisteredActors->CreateIterator(); ActorIt; ++ActorIt)
	{
		const TWeakObjectPtr<AActor> ActorKey = *ActorIt;
		AActor* RegisteredActor = ActorKey.Get();
		if (!IsValid(RegisteredActor)
			|| RegisteredActor->IsActorBeingDestroyed()
			|| RegisteredActor->GetWorld() != World)
		{
			SourceByActor.Remove(ActorKey);
			ActorIt.RemoveCurrent();
			continue;
		}

		OutActors.Add(RegisteredActor);
	}

	if (RegisteredActors->IsEmpty())
	{
		ActorsBySource.Remove(SourceKey);
	}
}

AActor* UTransientActorRegistrySubsystem::ResolveRegistrySource(AActor* SourceActor)
{
	AActor* CurrentActor = SourceActor;
	TSet<TWeakObjectPtr<AActor>> VisitedActors;
	for (int32 OwnershipDepth = 0;
		OwnershipDepth < MaximumOwnershipDepth && IsValid(CurrentActor);
		++OwnershipDepth)
	{
		if (CurrentActor->IsA<APawn>())
		{
			return CurrentActor;
		}

		const TWeakObjectPtr<AActor> CurrentActorKey(CurrentActor);
		if (VisitedActors.Contains(CurrentActorKey))
		{
			return nullptr;
		}
		VisitedActors.Add(CurrentActorKey);

		if (APawn* InstigatorPawn = CurrentActor->GetInstigator())
		{
			return InstigatorPawn;
		}

		CurrentActor = CurrentActor->GetOwner();
	}

	return IsValid(CurrentActor) ? CurrentActor : SourceActor;
}
