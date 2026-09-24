#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PetCharacter.generated.h"

UCLASS(Blueprintable)
class LABPROJECT_API APetCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	APetCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!Pet")
	void SetFollowTargetActor(AActor* InFollowTargetActor);

	UFUNCTION(BlueprintPure, Category = "!Pet")
	AActor* GetFollowTargetActor() const { return FollowTargetActor.Get(); }

protected:
	UPROPERTY(Replicated, Transient, BlueprintReadOnly, Category = "!Pet")
	TObjectPtr<AActor> FollowTargetActor;
};
