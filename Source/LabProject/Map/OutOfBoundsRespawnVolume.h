#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OutOfBoundsRespawnVolume.generated.h"

class AActor;
class ACharacterBase;
class UPrimitiveComponent;
class USphereComponent;

UCLASS(Blueprintable)
class LABPROJECT_API AOutOfBoundsRespawnVolume : public AActor
{
	GENERATED_BODY()

public:
	AOutOfBoundsRespawnVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!OutOfBounds")
	TObjectPtr<USphereComponent> BoundarySphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!OutOfBounds")
	bool bRespawnPlayersOnEndOverlap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!OutOfBounds")
	bool bDestroyNonPlayerActorsOnEndOverlap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!OutOfBounds")
	bool bCleanupTransientActorsOnPlayerExit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!OutOfBounds")
	bool bAllowCleanupOfNetStartupActors = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!OutOfBounds", meta = (EditCondition = "bCleanupTransientActorsOnPlayerExit"))
	TArray<TSubclassOf<AActor>> CleanupActorClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!OutOfBounds")
	bool bLogOutOfBoundsRespawn = true;

	UFUNCTION(BlueprintImplementableEvent, Category = "!OutOfBounds", meta = (DisplayName = "On Player Respawned From Out Of Bounds"))
	void BP_OnPlayerRespawnedFromOutOfBounds(ACharacterBase* PlayerCharacter);

private:
	UFUNCTION()
	void HandleBoundaryEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	bool TryRespawnPlayer(ACharacterBase* PlayerCharacter);
	bool TryDestroyNonPlayerActor(AActor* Actor) const;
	int32 CleanupTransientActors(ACharacterBase* TriggeringPlayer) const;
	bool CanCleanupActor(const AActor* Actor, const ACharacterBase* TriggeringPlayer) const;
	bool IsConfiguredCleanupClass(const AActor* Actor) const;
	bool ShouldIgnoreEndOverlap(const AActor* OtherActor) const;
	void ConfigureBoundaryCollision() const;

	bool bEndingPlay = false;
};
