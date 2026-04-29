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
	/** 서브시스템을 초기화합니다. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 서브시스템을 정리합니다. */
	virtual void Deinitialize() override;

	/** 상태창 ViewModel을 새로 고칩니다. */
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void RefreshStatusViewModel();

	/** 위젯에 상태창 ViewModel을 적용합니다. */
	UFUNCTION(BlueprintCallable, Category = "!ViewModel")
	void ApplyStatusViewModelToWidget(UUserWidget* InWidget);

	/** 상태창 ViewModel을 반환합니다. */
	UFUNCTION(BlueprintPure, Category = "!ViewModel")
	UStatusViewModel* GetStatusViewModel() const { return StatusViewModel; }

private:
	/** 현재 로컬 플레이어의 ASC를 찾습니다. */
	UAbilitySystemComponent* ResolveAbilitySystemComponent() const;

	/** 상태창 ViewModel을 ASC에 바인딩합니다. */
	void BindStatusViewModelToASC(UAbilitySystemComponent* InASC);

private:
	/** 상태창 ViewModel입니다. */
	UPROPERTY(Transient)
	TObjectPtr<UStatusViewModel> StatusViewModel;

	/** 현재 바인딩된 ASC입니다. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
};