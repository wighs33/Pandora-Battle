#pragma once

#include "CoreMinimal.h"
#include "Definition/Item/ItemFragments.h"
#include "Definition/Shop/ShopTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "ItemDefinition.generated.h"

class UGameplayEffect;
class UTexture2D;

/** 조각으로 나누기 전의 무기 데이터. 옛 애셋을 읽어 조각으로 옮기는 데만 쓴다. */
USTRUCT()
struct LABPROJECT_API FWeaponDefinitionData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FWeaponEquipDefinitionData Equip;

	UPROPERTY()
	FWeaponAttackDefinitionData Attack;

	UPROPERTY()
	FWeaponMovementDefinitionData Movement;

	UPROPERTY()
	FWeaponHitReactDefinitionData HitReact;

	UPROPERTY()
	FWeaponAimDefinitionData Aim;

	UPROPERTY()
	FItemFragment_Bow Bow;

	UPROPERTY()
	FItemFragment_Gun Gun;

	UPROPERTY()
	FWeaponAIDefinitionData AI;
};

/**
 * 아이템 한 종류의 고정 데이터.
 *
 * 모든 아이템이 갖는 이름, 아이콘, 종류 태그, 상점 정보는 정의에 두고, 능력치·소모·드롭·무기처럼 종류마다 다른 데이터는
 * 필요한 조각(FItemFragment)만 골라 Fragments에 담는다.
 */
UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// Public API ------------------------------------------------------------------------------------------------------
	UItemDefinition();
	static FName GetWeaponPresentationBundleName();
	void GetWeaponPresentationAssetPaths(TArray<FSoftObjectPath>& OutAssetPaths) const;

	bool MatchesItemType(FGameplayTag ItemTypeTag) const;
	bool IsWeaponDefinition(FGameplayTag WeaponTypeTag) const;
	bool IsConsumableDefinition(FGameplayTag ConsumableTypeTag) const;

	template <typename FragmentType>
	const FragmentType* FindFragment() const
	{
		for (const TInstancedStruct<FItemFragment>& Fragment : Fragments)
		{
			if (const FragmentType* Found = Fragment.GetPtr<FragmentType>())
			{
				return Found;
			}
		}
		return nullptr;
	}

	template <typename FragmentType>
	bool HasFragment() const
	{
		return FindFragment<FragmentType>() != nullptr;
	}

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FGameplayTag IdTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Shop")
	FShopProductDefinitionData ShopData;

	/** 이 아이템에 필요한 조각만 담는다. 같은 종류의 조각은 하나만 둔다(검증이 막는다). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Fragments", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FItemFragment>> Fragments;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
#if WITH_EDITORONLY_DATA
	void MoveLegacyDataToFragments();

	// 조각으로 나누기 전에 저장한 애셋의 값. 불러올 때 조각으로 옮기고, 다시 저장하면 남지 않는다.
	UPROPERTY()
	TMap<FGameplayTag, float> Map_Stat_Magnitude_DEPRECATED;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> ConsumeGameplayEffectClass_DEPRECATED;

	UPROPERTY()
	TMap<FGameplayTag, float> Map_Consume_Magnitude_DEPRECATED;

	UPROPERTY()
	int32 QuantityToConsume_DEPRECATED = 1;

	UPROPERTY()
	bool bCanDropFromRewardChest_DEPRECATED = true;

	UPROPERTY()
	float DropRate_DEPRECATED = 1.0f;

	UPROPERTY()
	FWeaponDefinitionData WeaponData_DEPRECATED;
#endif
};

/** 정의가 없을 수도 있는 호출부용. 정의나 조각이 없으면 nullptr이다. */
template <typename FragmentType>
const FragmentType* FindItemFragment(const UItemDefinition* ItemDefinition)
{
	return ItemDefinition ? ItemDefinition->FindFragment<FragmentType>() : nullptr;
}
