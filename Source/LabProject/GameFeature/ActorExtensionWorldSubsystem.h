#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ActorExtensionWorldSubsystem.generated.h"

class AActor;
struct FComponentRequestHandle;

DECLARE_DELEGATE_RetVal_OneParam(bool, FPdActorExtensionCanActivate, AActor*);
DECLARE_DELEGATE_OneParam(FPdActorExtensionExecute, AActor*);

class UActorExtensionWorldSubsystem;

struct FPdActorExtensionSpec
{
	FPdActorExtensionCanActivate CanActivate;
	FPdActorExtensionExecute OnActivate;
	FPdActorExtensionExecute OnDeactivate;
	bool bUseClientRoleFilter = false;
	bool bAddToLocallyControlled = false;
	bool bAddToSimulatedProxy = false;
};

class LABPROJECT_API FActorExtensionHandle
{
public:
	FActorExtensionHandle(UActorExtensionWorldSubsystem* InSubsystem, int32 InExtensionId);
	~FActorExtensionHandle();

	void Unregister();
	bool IsValid() const { return ExtensionId != INDEX_NONE; }

private:
	TWeakObjectPtr<UActorExtensionWorldSubsystem> Subsystem;
	int32 ExtensionId = INDEX_NONE;
};

struct FPdRegisteredActorExtension
{
	TWeakObjectPtr<AActor> Actor;
	TArray<int32> ExtensionIds;
};

UCLASS()
class LABPROJECT_API UActorExtensionWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual bool IsTickable() const override;

	TSharedPtr<FActorExtensionHandle> RegisterExtensionForClass(UClass* TargetClass, FPdActorExtensionSpec ExtensionSpec);
	void UnregisterExtension(int32 ExtensionId);

private:
	void EnsureExtensionEventHandler(UClass* TargetClass);
	void HandleActorExtensionEvent(AActor* Actor, FName EventName);
	void RefreshExperienceLoadState();
	void QueueActor(AActor* Actor);
	void QueueExistingActorsForClass(UClass* TargetClass);
	void CollectExtensionsForActor(AActor* Actor, TArray<int32>& OutExtensionIds) const;
	void RemoveActor(AActor* Actor);
	void DeactivateExtensionForActor(AActor* Actor, int32 ExtensionId, const FPdActorExtensionSpec& ExtensionSpec);
	bool IsExtensionActiveForActor(AActor* Actor, int32 ExtensionId) const;
	bool IsExtensionPendingForActor(AActor* Actor, int32 ExtensionId) const;
	bool ShouldApplyExtensionToActor(AActor* Actor, const FPdActorExtensionSpec& ExtensionSpec) const;

private:
	TArray<TWeakObjectPtr<AActor>> UncheckedActors;
	TArray<FPdRegisteredActorExtension> RegisterActors;
	TMap<TWeakObjectPtr<AActor>, TSet<int32>> ActiveActorExtensions;

	TMap<int32, FPdActorExtensionSpec> ExtensionById;
	TMap<UClass*, TSet<int32>> ClassExtensionIds;
	TMap<UClass*, TSharedPtr<FComponentRequestHandle>> ExtensionEventHandles;
	int32 NextExtensionId = 1;
	bool bExperienceLoaded = false;
};
