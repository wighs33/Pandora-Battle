#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Common/KillLogTypes.h"
#include "KillLogWidget.generated.h"

class UKillLogEntryWidget;
class UVerticalBox;

UCLASS()
class LABPROJECT_API UKillLogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!KillLog")
	void AddKillLogEntry(const FKillLogEntry& KillLogEntry);

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void RemoveKillLogEntry(UKillLogEntryWidget* EntryWidget);
	void TrimOverflowEntries();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!KillLog|Bind")
	TObjectPtr<UVerticalBox> VerticalBox_KillLogs = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Setup")
	TSubclassOf<UKillLogEntryWidget> KillLogEntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Setup", meta = (ClampMin = "1"))
	int32 MaxVisibleEntries = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Setup", meta = (ClampMin = "0.0"))
	float EntryLifetime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!KillLog|Setup")
	bool bNewestEntryOnTop = true;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UKillLogEntryWidget>> ActiveEntries;
};
