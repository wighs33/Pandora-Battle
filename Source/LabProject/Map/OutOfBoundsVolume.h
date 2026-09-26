#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OutOfBoundsVolume.generated.h"

class AActor;
class ACharacterBase;
class UPrimitiveComponent;
class USphereComponent;

UCLASS(Blueprintable)
class LABPROJECT_API AOutOfBoundsVolume : public AActor
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	AOutOfBoundsVolume(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintImplementableEvent, Category = "!OutOfBounds", meta = (DisplayName = "On Player Respawned From Out Of Bounds"))
	void BP_OnPlayerRespawnedFromOutOfBounds(ACharacterBase* PlayerCharacter);

private:
	UFUNCTION()
	void HandleBoundaryEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	// Internal Helpers ------------------------------------------------------------------------------------------------
	bool TryRespawnPlayer(ACharacterBase* PlayerCharacter);
	bool TryDestroyNonPlayerActor(AActor* Actor) const;
	bool ShouldIgnoreEndOverlap(const AActor* OtherActor) const;
	void ConfigureBoundaryCollision() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!OutOfBounds")
	TObjectPtr<USphereComponent> BoundarySphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!OutOfBounds")
	bool bRespawnPlayersOnEndOverlap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!OutOfBounds")
	bool bDestroyNonPlayerActorsOnEndOverlap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!OutOfBounds")
	bool bLogOutOfBoundsRespawn = true;

private:
	bool bEndingPlay = false;
};
