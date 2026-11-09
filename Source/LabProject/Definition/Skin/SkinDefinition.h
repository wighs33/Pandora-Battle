#pragma once

#include "CoreMinimal.h"
#include "Definition/Shop/ShopTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"

#include "SkinDefinition.generated.h"

class UTexture2D;
class UAnimMontage;
class AActor;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API USkinDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 제스처의 소유권과 슬롯 배정은 DefaultProvisionDefinition.GestureGrants에서 가져옵니다. */
	bool IsDefaultProfileSkin() const { return bGrantedByDefault && !GestureMontage; }

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

	/** 이 스킨을 입는 동안 함께 막는 다른 장착 칸. 입으면 그 칸의 스킨을 벗기고, 이 스킨을 벗을 때까지 그 칸에 입히지 못한다.
	 *  예: 상의 칸에 입는 한 벌 옷은 하의 칸을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Equip", meta = (Categories = "Skin.Cosmetics"))
	FGameplayTagContainer CoveredSlotTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Gesture")
	TObjectPtr<UAnimMontage> GestureMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Pet")
	TSoftClassPtr<AActor> PetActorClass;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin")
	FGameplayTag IdTag;

	/** 제스처가 아닌 코스메틱을 로컬 프로필에서 영구적으로 사용할 수 있게 합니다. 제스처는 GestureGrants를 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Skin|Default Grant",
		meta = (DisplayName = "Granted By Default"))
	bool bGrantedByDefault = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop")
	FShopProductDefinitionData ShopData;
};
