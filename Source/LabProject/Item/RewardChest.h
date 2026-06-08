#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "TimerManager.h"
#include "RewardChest.generated.h"

class UAnimationAsset;
class UAnimMontage;
class UItemDefinition;
class UMaterialBillboardComponent;
class UNiagaraComponent;
class UPrimitiveComponent;
class USkinDefinition;
class USoundBase;
class UPandoraDefinition;
class USkeletalMeshComponent;
class UWidgetComponent;

UENUM(BlueprintType)
enum class ERewardChestState : uint8
{
	Closed,
	Opening,
	Opened,
	Hidden
};

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ARewardChest : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ARewardChest(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(AActor* InteractingActor) override;
	virtual bool Interact_Implementation(AActor* InteractingActor) override;
	virtual FText GetInteractText_Implementation(AActor* InteractingActor) override;
	virtual void GetRewardItems_Implementation(TArray<FPrimaryAssetId>& OutItemDefinitionList) override;
	virtual void GetRewardSkins_Implementation(TArray<FPrimaryAssetId>& OutSkinDefinitionList) override;
	virtual void GetRewardPandoras_Implementation(TArray<FPrimaryAssetId>& OutPandoraDefinitionList) override;
	virtual void OnRewardsClaimed_Implementation(AActor* RewardReceiver) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Reward Chest")
	void MarkOpened(AActor* RewardReceiver);

	UFUNCTION(BlueprintPure, Category = "!Reward Chest")
	bool IsOpened() const { return ChestState != ERewardChestState::Closed; }

	UFUNCTION(BlueprintPure, Category = "!Reward Chest")
	ERewardChestState GetChestState() const { return ChestState; }

protected:
	UFUNCTION()
	void OnRep_ChestState();

	UFUNCTION(BlueprintImplementableEvent, Category = "!Reward Chest", meta = (DisplayName = "On Chest Opened"))
	void BP_OnChestOpened(AActor* RewardReceiver);

	UFUNCTION()
	void HandleChestBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleChestEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Components")
	TObjectPtr<USkeletalMeshComponent> ChestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Components")
	TObjectPtr<UNiagaraComponent> OpenEffect;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Components")
	TObjectPtr<UWidgetComponent> InteractionTipWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Components")
	TObjectPtr<UMaterialBillboardComponent> InteractionBillboard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward")
	TArray<TSoftObjectPtr<UItemDefinition>> RewardItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward")
	TArray<TSoftObjectPtr<USkinDefinition>> RewardSkins;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward")
	TArray<TSoftObjectPtr<UPandoraDefinition>> RewardPandoras;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Interaction")
	TObjectPtr<UAnimMontage> CharacterInteractionMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Interaction", meta = (ClampMin = "0.0"))
	float CharacterInteractionMontagePlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Open")
	TObjectPtr<UAnimationAsset> OpenAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Open")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Open", meta = (ClampMin = "0.0"))
	float OpenSoundVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Open", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float HideAfterOpenFallbackDelay = 0.1f;

private:
	template <typename DefinitionType>
	void AppendPrimaryAssetIds(const TArray<TSoftObjectPtr<DefinitionType>>& SourceDefinitions, TArray<FPrimaryAssetId>& OutPrimaryAssetIds) const;

	void ConfigureChestCollision(bool bEnableInteraction) const;
	void PlayCharacterInteractionAnimation(AActor* RewardReceiver) const;
	void SetInteractionAnchorVisible(bool bVisible) const;
	void SetInteractionTipVisible(bool bVisible) const;
	void SetChestState(ERewardChestState NewState, AActor* RewardReceiver);
	void ApplyChestState(AActor* RewardReceiver);
	void ApplyClosedState();
	void ApplyOpeningState(AActor* RewardReceiver);
	void ApplyOpenedState();
	void ApplyHiddenState();
	void ScheduleFinishOpening();
	void FinishOpening();
	void ScheduleHideOpenedChest();
	void HideOpenedChest();

	UPROPERTY(ReplicatedUsing = OnRep_ChestState)
	ERewardChestState ChestState = ERewardChestState::Closed;

	FTimerHandle FinishOpeningTimerHandle;
	FTimerHandle HideOpenedChestTimerHandle;
};
