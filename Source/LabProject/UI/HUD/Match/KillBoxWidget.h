#pragma once

#include "CoreMinimal.h"
#include "UI/Common/LocalizedMenuWidget.h"
#include "UI/Common/UiMotion.h"
#include "KillBoxWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UKillBoxWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|KillBox")
	void SetTeamInfo(int32 InTeamColorIndex, const FText& InTeamName, const FLinearColor& InTeamColor);

	UFUNCTION(BlueprintCallable, Category = "!UI|KillBox")
	void SetKillCount(int32 InKillCount);

	UFUNCTION(BlueprintCallable, Category = "!UI|KillBox")
	void RefreshUI();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|KillBox|Bind")
	TObjectPtr<UTextBlock> Txt_Team;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|KillBox|Bind")
	TObjectPtr<UTextBlock> Txt_KillCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|KillBox|Bind")
	TObjectPtr<UBorder> Border_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|KillBox|Bind")
	TObjectPtr<UBorder> Border_OutLine;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|KillBox|Style", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TeamColorAlpha = 0.7f;

private:
	int32 TeamColorIndex = INDEX_NONE;
	FText TeamName;
	FLinearColor TeamColor = FLinearColor::White;
	int32 KillCount = 0;

	// 처치 수가 오르면 숫자가 튀었다가 돌아온다. 처음 받은 값은 튀지 않는다.
	UiMotion::FSpring CountScale{1.f, 0.f};
	bool bHasKillCount = false;
	bool bCountSettling = false;
};
