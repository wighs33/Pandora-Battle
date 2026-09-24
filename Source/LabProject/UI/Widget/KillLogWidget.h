#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/KillLogTypes.h"
#include "KillLogWidget.generated.h"

class UKillLogEntryWidget;
class UPanelWidget;

UCLASS()
class LABPROJECT_API UKillLogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;

	// Public API ------------------------------------------------------------------------------------------------------
	UKillLogWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "!KillLog")
	void AddKillLogEntry(const FKillLogEntry& KillLogEntry);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	UPanelWidget* FindKillLogContainer();
	void RemoveKillLogEntry(UKillLogEntryWidget* EntryWidget);
	void TrimOverflowEntries();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!KillLog|Bind")
	TObjectPtr<UPanelWidget> KillLogContainer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Setup")
	TSubclassOf<UKillLogEntryWidget> KillLogEntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Setup", meta = (ClampMin = "1"))
	int32 MaxVisibleEntries = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Setup", meta = (ClampMin = "0.0"))
	float EntryLifetime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Setup")
	bool bNewestEntryOnTop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Setup")
	TArray<FName> KillLogContainerCandidateNames;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKillLogEntryWidget>> ActiveEntries;
};
