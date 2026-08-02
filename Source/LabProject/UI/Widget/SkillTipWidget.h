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

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Skill Tip")
	void RefreshSkillTips();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skill Tip|Widgets")
	TObjectPtr<UImage> Img_FirstSkillTip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skill Tip|Widgets")
	TObjectPtr<UImage> Img_SecondSkillTip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Skill Tip|Widgets")
	TObjectPtr<UImage> Img_ThirdSkillTip;

private:
	void InitializePandoraBinding();
	void UnbindPandoraComponent();
	void SchedulePandoraBindingRetry();
	UPandoraComponent* ResolvePandoraComponent() const;

	UFUNCTION()
	void HandlePandoraSelectionChanged(UPandoraDefinition* PandoraDefinition);

	UFUNCTION()
	void HandlePandoraLoadoutChanged();

	TWeakObjectPtr<UPandoraComponent> CachedPandoraComponent;
	FTimerHandle PandoraBindingRetryTimerHandle;
};
