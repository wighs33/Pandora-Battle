#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "TimerManager.h"
#include "SkillTipWidget.generated.h"

class UImage;
class UPandoraComponent;
class UPandoraDefinition;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API USkillTipWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Skill Tip")
	void RefreshSkillTips();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void InitializePandoraBinding();

	UFUNCTION()
	void HandlePandoraSelectionChanged(UPandoraDefinition* PandoraDefinition);

	UFUNCTION()
	void HandlePandoraLoadoutChanged();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void UnbindPandoraComponent();
	void SchedulePandoraBindingRetry();
	UPandoraComponent* ResolvePandoraComponent() const;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skill Tip|Widgets")
	TObjectPtr<UImage> Img_FirstSkillTip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skill Tip|Widgets")
	TObjectPtr<UImage> Img_SecondSkillTip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skill Tip|Widgets")
	TObjectPtr<UImage> Img_ThirdSkillTip;

private:
	TWeakObjectPtr<UPandoraComponent> CachedPandoraComponent;
	FTimerHandle PandoraBindingRetryTimerHandle;
};
