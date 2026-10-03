#pragma once

#include "CoreMinimal.h"
#include "UI/Info/RightListPanelWidget.h"
#include "RightPandoraWidget.generated.h"

/** 판도라 목록. 판도라 정의를 그대로 타일 항목으로 쓰고, 보유 상태는 항목 위젯이 다시 그린다. */
UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API URightPandoraWidget : public URightListPanelWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void ApplyWidgetDefinitionSettings() override;
	virtual void RebuildTileView() override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UButton> OffensiveButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UButton> DefensiveButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UButton> SupportButton;

	UPROPERTY(BlueprintReadOnly, Category = "!UI|Pandora", meta = (BindWidget))
	TObjectPtr<UButton> SpecialButton;
};
