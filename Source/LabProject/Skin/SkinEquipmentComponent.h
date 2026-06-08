#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "SkinEquipmentComponent.generated.h"

class APdCharacterBase;
class USkinDefinition;
class USkinInstance;

DECLARE_LOG_CATEGORY_EXTERN(SkinEquipmentComponentLog, Log, All);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPdOnEquippedSkinsChanged);

USTRUCT(BlueprintType)
struct LABPROJECT_API FEquippedSkinSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "!Skin|Equipment")
	FGameplayTag SlotTag;

	UPROPERTY(BlueprintReadOnly, Category = "!Skin|Equipment")
	TObjectPtr<const USkinDefinition> SkinDefinition = nullptr;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API USkinEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkinEquipmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "!Skin|Equipment")
	bool RequestEquipSkin(USkinInstance* SkinInstance, FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "!Skin|Equipment")
	bool RequestUnequipSkinSlot(FGameplayTag SlotTag);

	UFUNCTION(BlueprintPure, Category = "!Skin|Equipment")
	const USkinDefinition* GetEquippedSkinDefinition(FGameplayTag SlotTag) const;

	UPROPERTY(BlueprintAssignable, Category = "!Skin|Equipment")
	FPdOnEquippedSkinsChanged OnEquippedSkinsChanged;

protected:
	UFUNCTION(Server, Reliable)
	void ServerEquipSkin(USkinDefinition* SkinDefinition, FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void ServerUnequipSkinSlot(FGameplayTag SlotTag);

	UFUNCTION()
	void OnRep_EquippedSkins();

	bool EquipSkinDefinition(const USkinDefinition* SkinDefinition, FGameplayTag SlotTag);
	bool UnequipSkinSlotInternal(FGameplayTag SlotTag);
	bool CanEquipSkinDefinition(const USkinDefinition* SkinDefinition, FGameplayTag SlotTag) const;
	int32 FindEquippedSkinSlotIndex(FGameplayTag SlotTag) const;
	APdCharacterBase* GetCharacterOwner() const;
	void RebuildEquippedSkinActors();
	void DestroyEquippedSkinActors();
	AActor* SpawnAndAttachSkinActor(const USkinDefinition* SkinDefinition) const;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedSkins, Transient, BlueprintReadOnly, Category = "!Skin|Equipment")
	TArray<FEquippedSkinSlot> EquippedSkins;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<AActor>> EquippedSkinActors;
};
