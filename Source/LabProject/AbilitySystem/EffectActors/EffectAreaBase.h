#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "EffectAreaBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class USphereComponent;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API AEffectAreaBase : public AActor
{
	GENERATED_BODY()

public:
	AEffectAreaBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "!EffectArea")
	USphereComponent* GetAreaCollision() const { return AreaCollision; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleAreaEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void ApplyEffectToActor(AActor* TargetActor);
	void RemoveEffectFromActor(AActor* TargetActor);
	UAbilitySystemComponent* GetTargetAbilitySystemComponent(AActor* TargetActor) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!EffectArea", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> AreaCollision;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!EffectArea|Effect", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> EffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!EffectArea|Effect", meta = (Categories = "Data", AllowPrivateAccess = "true"))
	FGameplayTag EffectMagnitudeDataTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!EffectArea|Effect", meta = (AllowPrivateAccess = "true"))
	float EffectMagnitudeValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!EffectArea|Effect", meta = (ClampMin = "1.0", AllowPrivateAccess = "true"))
	float EffectLevel = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!EffectArea|Network", meta = (AllowPrivateAccess = "true"))
	bool bApplyOnlyOnAuthority = true;

private:
	TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> ActiveEffectHandles;
};
