#pragma once

#include "UI/Common/LocalizedMenuWidget.h"

#include "PandoraDescriptionWidget.generated.h"

class UPandoraTreeComponent;
class UPandoraDefinition;
class UPandoraDescriptionViewModel;
class UImage;

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API UPandoraDescriptionWidget : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void OnMenuLanguageChanged() override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraDefinition(const UPandoraDefinition* InPandoraDefinition);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraTreeComponent(UPandoraTreeComponent* InPandoraTreeComponent);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetDetails();

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void ResolvePandoraTreeComponent();
	void ApplyEffectIconResources();
	UPandoraDescriptionViewModel* GetOrCreatePandoraDescriptionViewModel();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Effect Icons")
	TObjectPtr<UImage> EffectIconResource1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Effect Icons")
	TObjectPtr<UImage> EffectIconResource2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Effect Icons")
	TObjectPtr<UImage> EffectIconResource3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style")
	FSlateColor WeaponRequirementTextColor = FSlateColor(FLinearColor(1.0f, 0.22f, 0.05f, 1.0f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ExposeOnSpawn = "true"), Category = "!UI|Pandora")
	TObjectPtr<const UPandoraDefinition> PandoraDefinition;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|Internal")
	TObjectPtr<UPandoraTreeComponent> PandoraTreeComponent;

private:
	UPROPERTY(BlueprintReadOnly, Transient, Category = "!UI|Pandora|ViewModel", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPandoraDescriptionViewModel> PandoraDescriptionViewModel;
};
