#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Layout/Margin.h"
#include "Definition/Player/CharacterActionDefinition.h"
#include "ActionSlotWidget.generated.h"

class UActionSlotEntryWidget;
class UCharacterActionDefinition;
class UHorizontalBox;
struct FStreamableHandle;

UCLASS(BlueprintType, Blueprintable)
class LABPROJECT_API UActionSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|ActionSlot")
	void FillActionSlotBar();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BeginActionContentPreload();
	void BeginActionPresentationPreload(int32 PreloadGeneration);
	void ReleaseActionContentPreloads();
	void RebuildActionSlotBar();
	void AddActionSlotEntry(int32 SlotIndex);
	UActionSlotEntryWidget* CreateActionSlotEntryWidget() const;
	void AddWidgetToBar(UWidget* Widget) const;
	TSubclassOf<UActionSlotEntryWidget> ResolveEntryWidgetClass() const;
	TSoftObjectPtr<UCharacterActionDefinition> ResolveActionDefinitionReference() const;
	UCharacterActionDefinition* ResolveActionDefinition() const;
	ECharacterActionType ResolveActionType(int32 SlotIndex) const;
	FMargin ResolveSlotPadding() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot|Classes", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UActionSlotEntryWidget> EntryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot", meta = (AllowPrivateAccess = "true"))
	FMargin SlotPadding = FMargin(5.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|ActionSlot|Actions", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UCharacterActionDefinition> CharacterActionDefinition;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> ContainerHorizontalBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActionSlotEntryWidget>> EntryWidgets;

	FTimerHandle RebuildActionSlotTimerHandle;
	int32 ActionContentPreloadGeneration = 0;
	TSharedPtr<FStreamableHandle> ActionDefinitionPreloadHandle;
	TSharedPtr<FStreamableHandle> ActionPresentationPreloadHandle;
};
