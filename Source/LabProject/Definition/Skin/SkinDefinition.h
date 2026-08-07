#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "UI/Shop/ShopTypes.h"

#include "SkinDefinition.generated.h"

class UTexture2D;
class UAnimMontage;
class AActor;

DECLARE_LOG_CATEGORY_EXTERN(SkinDefinitionLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API USkinDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	bool IsGrantedByDefault() const { return bGrantedByDefault; }

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin")
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Equip")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Equip")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Equip")
	bool bUseOwnerMeshAsLeaderPose = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Gesture")
	TObjectPtr<UAnimMontage> GestureMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Pet")
	TSoftClassPtr<AActor> PetActorClass;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin")
	FGameplayTag IdTag;

	/** Makes this cosmetic or gesture permanently available in lobby and gameplay profiles. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Default Grant",
		meta = (DisplayName = "Granted By Default"))
	bool bGrantedByDefault = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop")
	FShopProductDefinitionData ShopData;
};
