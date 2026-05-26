#pragma once

#include "Blueprint/UserWidget.h"

#include "PandoraDescriptionWidget.generated.h"

class UPandoraTreeComponent;
class UPandoraDefinition;
class UPandoraDescriptionViewModel;
class UWidgetAnimation;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPandoraDescriptionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraTreeComponent(UPandoraTreeComponent* InPandoraTreeComponent);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetDetails();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional), Category = "!UI|Pandora|Animation")
	TObjectPtr<UWidgetAnimation> ScaleUp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FSlateColor WeaponRequirementTextColor = FSlateColor(FLinearColor(1.0f, 0.22f, 0.05f, 1.0f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Pandora")
	TObjectPtr<UPandoraDefinition> PandoraDefinition;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|Internal")
	TObjectPtr<UPandoraTreeComponent> PandoraTreeComponent;

private:
	void ResolvePandoraTreeComponent();
	UPandoraDescriptionViewModel* GetOrCreatePandoraDescriptionViewModel();
	void ApplyPandoraDescriptionViewModelToMvvmView();
	FText GetDescriptionForLevel(int32 Level) const;
	FText GetWeaponRequirementText() const;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPandoraDescriptionViewModel> PandoraDescriptionViewModel;
};
