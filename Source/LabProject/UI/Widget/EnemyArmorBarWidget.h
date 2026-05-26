#pragma once

#include "UI/Widget/EnemyShieldBarWidget.h"

#include "EnemyArmorBarWidget.generated.h"

UCLASS(Blueprintable, BlueprintType, meta = (DeprecatedNode, DeprecationMessage = "Use EnemyShieldBarWidget."))
class LABPROJECT_API UEnemyArmorBarWidget : public UEnemyShieldBarWidget
{
	GENERATED_BODY()

public:
	// Kept so existing imported WBP_EnemyArmorBar graphs keep compiling until the asset is renamed/reparented.
	UFUNCTION(BlueprintCallable, Category = "!UI|Enemy|Armor")
	void UpdateArmorPercent() { UpdateShieldPercent(); }
};
