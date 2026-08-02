#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "SkinEquipmentComponent.generated.h"

class ACharacterBase;
class USkinDefinition;
class USkinInstance;
class UAnimMontage;
struct FStreamableHandle;

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

	UFUNCTION()
	void GetEquippedSkinSlots(TArray<FEquippedSkinSlot>& OutEquippedSkins) const;

	UFUNCTION()
	bool RequestEquipSkinDefinition(USkinDefinition* SkinDefinition, FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "!Skin|Gesture")
	bool RequestPlayGestureSlot(int32 GestureSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "!Skin|Gesture")
	bool RequestCancelActiveGestureMontage(float BlendOutTime = 0.12f);

	UPROPERTY(BlueprintAssignable, Category = "!Skin|Equipment")
	FPdOnEquippedSkinsChanged OnEquippedSkinsChanged;

protected:
	UFUNCTION(Server, Reliable)
	void ServerEquipSkin(USkinDefinition* SkinDefinition, FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void ServerUnequipSkinSlot(FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void ServerPlayGestureSlot(int32 GestureSlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerCancelActiveGestureMontage(float BlendOutTime);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayGestureMontage(UAnimMontage* GestureMontage);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCancelActiveGestureMontage(float BlendOutTime);

	UFUNCTION()
	void OnRep_EquippedSkins();

	bool EquipSkinDefinition(const USkinDefinition* SkinDefinition, FGameplayTag SlotTag);
	bool UnequipSkinSlotInternal(FGameplayTag SlotTag);
	bool CanEquipSkinDefinition(const USkinDefinition* SkinDefinition, FGameplayTag SlotTag) const;
	bool CanReferenceSkinDefinition(const USkinDefinition* SkinDefinition) const;
	bool HasSkinEquipmentAuthority() const;
	bool TryConsumeGestureNetworkEvent(double& LastRequestTime, double MinInterval);
	bool TryConsumeGesturePlayRequest();
	bool TryConsumeGestureCancelRequest();
	float GetClampedGestureBlendOutTime(float BlendOutTime) const;
	int32 FindEquippedSkinSlotIndex(FGameplayTag SlotTag) const;
	ACharacterBase* GetCharacterOwner() const;
	bool PlayGestureMontage(UAnimMontage* GestureMontage) const;
	bool CancelActiveGestureMontage(float BlendOutTime) const;
	void RebuildEquippedSkinActors();
	void RebuildEquippedSkinActorsFromLoadedContent(uint32 RequestGeneration);
	void ReleaseSkinPresentationLoad();
	void DestroyEquippedSkinActors();
	AActor* SpawnPetSkinActor(const USkinDefinition* SkinDefinition) const;
	AActor* SpawnAndAttachSkinActor(const USkinDefinition* SkinDefinition) const;

	UPROPERTY(ReplicatedUsing = OnRep_EquippedSkins, Transient, BlueprintReadOnly, Category = "!Skin|Equipment")
	TArray<FEquippedSkinSlot> EquippedSkins;

	UPROPERTY(Transient)
	TMap<FGameplayTag, TObjectPtr<AActor>> EquippedSkinActors;

	UPROPERTY(EditDefaultsOnly, Category = "!Skin|Gesture|Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double GesturePlayRequestMinInterval = 0.15;

	UPROPERTY(EditDefaultsOnly, Category = "!Skin|Gesture|Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double GestureCancelRequestMinInterval = 0.05;

	UPROPERTY(EditDefaultsOnly, Category = "!Skin|Gesture|Network", meta = (ClampMin = "0.0", ForceUnits = "s"))
	double MaxGestureCancelBlendOutTime = 2.0;

	UPROPERTY(Transient)
	double LastGesturePlayRequestTime = -1.0;

	UPROPERTY(Transient)
	double LastGestureCancelRequestTime = -1.0;

	TSharedPtr<FStreamableHandle> SkinPresentationLoadHandle;
	uint32 SkinPresentationRequestGeneration = 0;
};
