#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "TimerManager.h"
#include "RewardChest.generated.h"

class UAnimationAsset;
class UAnimMontage;
class UInventoryComponent;
class UItemDefinition;
class UMaterialBillboardComponent;
class UNiagaraComponent;
class UPrimitiveComponent;
class URewardDefinition;
class USkinDefinition;
class USoundBase;
class UPandoraDefinition;
class USkeletalMeshComponent;
class UWidgetComponent;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class ERewardChestState : uint8
{
	Closed,
	Opening,
	Opened,
	Hidden
};

USTRUCT(BlueprintType)
struct FRewardChestItemCountChance
{
	GENERATED_BODY()

	FRewardChestItemCountChance() = default;

	FRewardChestItemCountChance(const int32 InItemCount, const float InChance)
		: ItemCount(InItemCount)
		, Chance(InChance)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Reward Chest|Reward", meta = (ClampMin = "1", UIMin = "1"))
	int32 ItemCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Reward Chest|Reward", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Chance = 1.0f;
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
	/** Inventory ownership does not exclude weapon or equipment candidates; duplicate instances are allowed. */
	void GetRewardItemsForInventory(
		const UInventoryComponent* InventoryComponent,
		TArray<FPrimaryAssetId>& OutItemDefinitionList);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!Reward Chest")
	void MarkOpened(AActor* RewardReceiver);

	/** Keeps an unused placed chest as a hidden spawn-location anchor. */
	void DeactivateForSpawnPool();

	UFUNCTION(BlueprintPure, Category = "!Reward Chest")
	bool IsOpened() const { return ChestState != ERewardChestState::Closed; }

	UFUNCTION(BlueprintPure, Category = "!Reward Chest")
	ERewardChestState GetChestState() const { return ChestState; }

	TSoftObjectPtr<URewardDefinition> GetRewardDefinitionAsset() const { return RewardDefinition; }
	bool IsRewardContentReady() const { return bRewardContentReady; }

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward",
		meta = (ToolTip = "Optional shared numeric reward data. Items, skins, and pandoras stay on the chest."))
	TSoftObjectPtr<URewardDefinition> RewardDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward")
	TArray<TSoftObjectPtr<UItemDefinition>> RewardItems;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward")
	bool bUseItemDefinitionDropRates = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward",
		meta = (ClampMin = "1", UIMin = "1", EditCondition = "bUseItemDefinitionDropRates && !bUseRandomRewardItemCountChances",
			DisplayName = "Random Item Count",
			ToolTip = "Fallback fixed item count when random item count chances are disabled or invalid."))
	int32 RandomRewardItemCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward", meta = (EditCondition = "bUseItemDefinitionDropRates"))
	bool bUseRandomRewardItemCountChances = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward",
		meta = (EditCondition = "bUseItemDefinitionDropRates && bUseRandomRewardItemCountChances",
			DisplayName = "Random Item Count Chances",
			ToolTip = "Weighted chances for how many random item rewards this chest drops. Chances are relative weights, not required to sum to 100."))
	TArray<FRewardChestItemCountChance> RandomRewardItemCountChances;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Reward", meta = (EditCondition = "bUseItemDefinitionDropRates"))
	bool bAllowDuplicateRandomItems = false;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Reward Chest|Respawn",
		meta = (ClampMin = "0.0", ForceUnits = "s",
			ToolTip = "Time measured from a successful open until this chest reappears at a random unoccupied placed chest location."))
	float RespawnDelayAfterOpen = 60.0f;

private:
	template <typename DefinitionType>
	void AppendPrimaryAssetIds(const TArray<TSoftObjectPtr<DefinitionType>>& SourceDefinitions, TArray<FPrimaryAssetId>& OutPrimaryAssetIds) const;

	void AppendRandomItemPrimaryAssetIds(
		TArray<FPrimaryAssetId>& OutPrimaryAssetIds) const;
	void AppendConfiguredItemPrimaryAssetIds(
		TArray<FPrimaryAssetId>& OutPrimaryAssetIds) const;
	int32 ResolveRandomRewardItemCount() const;
	static int32 SelectWeightedItemIndex(const TArray<float>& Weights, float TotalWeight);
	bool IsWeaponItemDefinition(const UItemDefinition* ItemDefinition) const;
	void BeginRewardContentPreload();
	void HandleRewardContentPreloadComplete();
	void ReleaseRewardContentPreload();

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
	void ScheduleRespawnAfterOpen();
	void RetryRespawnAtAvailableLocation();
	bool TryRespawnAtRandomAvailableLocation();
	bool IsOccupyingSpawnLocation(const FTransform& SpawnTransform) const;

	UPROPERTY(ReplicatedUsing = OnRep_ChestState)
	ERewardChestState ChestState = ERewardChestState::Closed;

	FTimerHandle FinishOpeningTimerHandle;
	FTimerHandle HideOpenedChestTimerHandle;
	FTimerHandle RespawnTimerHandle;
	FTransform OriginalSpawnTransform = FTransform::Identity;
	bool bOriginalSpawnTransformCaptured = false;
	TSharedPtr<FStreamableHandle> RewardContentPreloadHandle;
	bool bRewardContentReady = false;
};
