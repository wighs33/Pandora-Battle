#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UiSubsystem.generated.h"

class UAbilitySystemComponent;
class UStatusViewModel;
class UUserWidget;

/** UI 서브시스템 로그 카테고리입니다. */
DECLARE_LOG_CATEGORY_EXTERN(PdUiSubsystemLog, Log, All);

/**
 * <로컬 UI 서브시스템>
 * - 상태창 ViewModel을 관리합니다.
 * - ASC와 ViewModel을 바인딩합니다.
 * - 위젯에 ViewModel을 적용합니다.
 */
UCLASS()
class LABPROJECT_API UUiSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	//------------------------------------------------------------------------------------------------------------------
	//--- Engine Callbacks
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//------------------------------------------------------------------------------------------------------------------
	//--- ViewModel
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void RefreshStatusViewModel();

	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void ApplyStatusViewModelToWidget(UUserWidget* InWidget);

	UFUNCTION(BlueprintPure, Category = "!ViewModel")
	UStatusViewModel* GetStatusViewModel() const { return StatusViewModel; }

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- Ability System
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;

private:
	//------------------------------------------------------------------------------------------------------------------
	//--- ViewModel
	UPROPERTY(Transient)
	TObjectPtr<UStatusViewModel> StatusViewModel;
};
