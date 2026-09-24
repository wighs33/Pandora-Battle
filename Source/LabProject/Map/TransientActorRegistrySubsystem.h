#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TransientActorRegistrySubsystem.generated.h"

class AActor;

UCLASS()
class LABPROJECT_API UTransientActorRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void Deinitialize() override;

	// Public API ------------------------------------------------------------------------------------------------------
	void RegisterTransientActor(AActor* TransientActor, AActor* SourceActor = nullptr);
	void UnregisterTransientActor(AActor* TransientActor);
	void GetTransientActorsForSource(AActor* SourceActor, TArray<AActor*>& OutActors);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	static AActor* ResolveRegistrySource(AActor* SourceActor);

private:
	TMap<TWeakObjectPtr<AActor>, TSet<TWeakObjectPtr<AActor>>> ActorsBySource;
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<AActor>> SourceByActor;
};
