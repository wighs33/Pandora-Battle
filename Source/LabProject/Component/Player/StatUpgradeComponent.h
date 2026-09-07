#pragma once

#include "CoreMinimal.h"
#include "Components/PlayerStateComponent.h"
#include "GameplayTagContainer.h"
#include "StatUpgradeComponent.generated.h"

class UAbilitySystemComponent;
class UStatUpgradeDefinition;
struct FStreamableHandle;

DECLARE_LOG_CATEGORY_EXTERN(StatUpgradeComponentLog, Log, All);

/**
 * 플레이어의 스탯 투자와 환불 요청을 처리한다.
 *
 * 서버에서 비용과 투자 한도를 확인하고 투자분만 능력치에 반영한다.
 * 기본값 적용은 ASC의 속성 초기화에, 자동 회복은 패시브 능력에 맡긴다.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LABPROJECT_API UStatUpgradeComponent : public UPlayerStateComponent
{
	GENERATED_BODY()

public:
	UStatUpgradeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	//------------------------------------------------------------------------------------------------------------------

	// 클라이언트의 true는 요청 전송을 뜻하며, 확정 결과는 복제된 능력치로 확인한다.
	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Stat", meta = (GameplayTagFilter = "Status"))
	bool RequestStatUp(FGameplayTag StatTag);

	UFUNCTION(BlueprintCallable, Category = "!AbilitySystem|Stat", meta = (GameplayTagFilter = "Status"))
	bool RequestStatDown(FGameplayTag StatTag);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "!AbilitySystem|Stat|Points")
	bool SetPointsForAllCategories(float Value);

	bool ApplyConfiguredAttributeDefaults();

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestStatUp(FGameplayTag StatTag);

	UFUNCTION(Server, Reliable)
	void ServerRequestStatDown(FGameplayTag StatTag);

	bool ApplyStatChange(FGameplayTag StatTag, int32 LevelDelta);
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	UStatUpgradeDefinition* LoadStatUpgradeDefinition();
	void BeginStatUpgradeDefinitionPreload();
	void HandleStatUpgradeDefinitionPreloaded(uint32 RequestGeneration);
	void ReleaseStatUpgradeDefinitionPreload();

	UPROPERTY(EditDefaultsOnly, Category = "!AbilitySystem|Stat", meta = (AllowPrivateAccess = "true", DisplayName = "DA Stat"))
	TSoftObjectPtr<UStatUpgradeDefinition> StatUpgradeDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UStatUpgradeDefinition> LoadedStatUpgradeDefinition;

	TSharedPtr<FStreamableHandle> StatUpgradeDefinitionLoadHandle;
	uint32 StatUpgradeDefinitionLoadGeneration = 0;
	bool bApplyDefaultsWhenDefinitionReady = false;
	bool bApplyingStatChange = false;
};
