#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KillBoxWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UKillBoxWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

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
};
