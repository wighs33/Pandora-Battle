#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "ItemDefinition.generated.h"

class AWeaponBase;
class UAnimInstance;
class UAnimMontage;
class UTexture2D;

DECLARE_LOG_CATEGORY_EXTERN(ItemDefinitionLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

public:
	/** 아이템 표시 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FText DisplayName;

	/** 아이템 설명 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FText Description;

	/** 아이콘 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	TSoftObjectPtr<UTexture2D> IconTexture = nullptr;
	
	/** 게임 플레이 태그 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	FGameplayTag IdTag;

	/** 효과 목록 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	TMap<FGameplayTag, float> Map_Stat_Magnitude;
	
	/** 티어 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item")
	int Tier;

	/** 블루프린트 클래스 넣기 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Equip")
	TSoftClassPtr<AWeaponBase> EquippedActorClass;

	/** 아이템을 부착할 캐릭터의 소켓 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Equip")
	FName EquipAttachSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Equip")
	TSoftObjectPtr<UAnimMontage> EquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Equip")
	TSoftObjectPtr<UAnimMontage> UnequipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Attack")
	TSoftObjectPtr<UAnimMontage> AttackMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|HitReact")
	TSoftObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Item|Equip")
	TSoftClassPtr<UAnimInstance> EquipAnimLayer;
};
