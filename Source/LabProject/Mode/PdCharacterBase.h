// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interface/CharacterAbilitySystemInterface.h"
#include "Interface/FactionInterface.h"
#include "Interface/InteractableInterface.h"
#include "PdCharacterBase.generated.h"

class UAnimInstance;
class UBoxComponent;
class UGameplayAbility;
class UPdAbilitySystemComponent;
class UCameraComponent;
class UEquipmentComponent;
class UPrimitiveComponent;
class USpringArmComponent;

DECLARE_LOG_CATEGORY_EXTERN(PdCharacterBaseLog, Log, All);

UCLASS()
class LABPROJECT_API APdCharacterBase :
	public ACharacter,
	public ICharacterAbilitySystemInterface,
	public IFactionInterface
{
	GENERATED_BODY()

public:
	APdCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UPdAbilitySystemComponent* GetPdAbilitySystemComponent() const override;
	virtual int32 GetFactionId() const override;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	UEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

	// 서버에서만 호출해야 하며, DefaultAbilities에 설정된 어빌리티를 중복 없이 부여합니다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!AbilitySystem|Grant")
	int32 GiveDefaultAbilities();

	UFUNCTION(BlueprintCallable, Category = "!Interaction", meta = (DisplayName = "HasCurrentInteractActors?"))
	bool HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const;

	UFUNCTION(BlueprintCallable, Category = "Animation")
	void ResetAnimationToDefault();

	UFUNCTION(BlueprintCallable, Category = "Animation")
	void SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass);

	void LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const;
	void HandleDeathAuth();

protected:
	void InitializeAbilitySystemActorInfo();
	bool TryMakeInteractableEntry(AActor* OtherActor, TScriptInterface<IInteractableInterface>& OutInteractableActor) const;

	UFUNCTION()
	void HandleInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UFUNCTION()
	void OnRep_CurrentAnimLayer();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!AbilitySystem|Grant")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> DefaultAnimLayer;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentAnimLayer, Transient, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> CurrentAnimLayer;

	UPROPERTY(BlueprintReadWrite, Transient, Category = "Default")
	TArray<TScriptInterface<IInteractableInterface>> CurrentInteractActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction")
	int32 FactionId = 0;
};
