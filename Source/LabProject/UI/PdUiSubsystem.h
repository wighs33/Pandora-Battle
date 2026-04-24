#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "PdUiSubsystem.generated.h"

class UAbilitySystemComponent;
class UStatusViewModel;
class UUserWidget;

DECLARE_LOG_CATEGORY_EXTERN(PdUiSubsystemLog, Log, All);

UCLASS()
class LABPROJECT_API UPdUiSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void RefreshStatusViewModel();

	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void ApplyStatusViewModelToWidget(UUserWidget* InWidget);

	UFUNCTION(BlueprintPure, Category = "!ViewModel")
	UStatusViewModel* GetStatusViewModel() const { return StatusViewModel; }

private:
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;
	void BindStatusViewModelToASC(UAbilitySystemComponent* InASC);

	UPROPERTY(Transient)
	TObjectPtr<UStatusViewModel> StatusViewModel;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
};
