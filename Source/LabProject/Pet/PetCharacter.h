#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PetCharacter.generated.h"

UCLASS(Blueprintable)
class LABPROJECT_API APetCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APetCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!Pet")
	void SetFollowTargetActor(AActor* InFollowTargetActor);

	UFUNCTION(BlueprintPure, Category = "!Pet")
	AActor* GetFollowTargetActor() const { return FollowTargetActor.Get(); }

protected:
	UPROPERTY(Replicated, Transient, BlueprintReadOnly, Category = "!Pet")
	TObjectPtr<AActor> FollowTargetActor;
};
