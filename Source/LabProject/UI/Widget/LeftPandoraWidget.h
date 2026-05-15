#pragma once

#include "Blueprint/UserWidget.h"
#include "Pandora/PandoraDefinition.h"
#include "UI/Widget/PandoraEquipSlotWidget.h"
#include "LeftPandoraWidget.generated.h"

class UImage;
class UPandoraInstance;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FPdOnClickedPandoraEquipSlot,
	UPandoraEquipSlotWidget*, SelectedPandoraEquipSlot,
	bool, bIsSelectedAnyButton);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API ULeftPandoraWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void ToggleActiveEquipSlots(bool bActive);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SelectPandoraEquipSlot(UPandoraEquipSlotWidget* InSelectedPandoraEquipSlot);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetSkillInfo(const TArray<FSkill>& InSkills);

	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Pandora")
	FPdOnClickedPandoraEquipSlot OnClicked_PandoraEquipSlot;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UPandoraEquipSlotWidget> FirstPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UPandoraEquipSlotWidget> SecondPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UPandoraEquipSlotWidget> ThirdPandora;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> FirstSkillIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> SecondSkillIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> ThirdSkillIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> FourthSkillIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> FirstSkillName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> SecondSkillName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> ThirdSkillName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> FourthSkillName;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TArray<TObjectPtr<UImage>> SkillIconList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TArray<TObjectPtr<UTextBlock>> SkillNameList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TArray<TObjectPtr<UPandoraEquipSlotWidget>> PandoraEquipSlotList;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	TObjectPtr<UPandoraEquipSlotWidget> SelectedPandoraEquipSlot;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora", meta = (DisplayName = "IsSelectedAnyButton?"))
	bool bIsSelectedAnyButton = false;

private:
	UFUNCTION()
	void HandlePandoraEquipSlotClicked(UPandoraEquipSlotWidget* PandoraEquipSlot);

	UFUNCTION()
	void HandlePandoraEquipSlotHovered(UPandoraEquipSlotWidget* PandoraEquipSlot);

	void RebuildSkillWidgetLists();
	void RebuildPandoraEquipSlotList();
	void BindPandoraEquipSlotCallbacks();
	void UnbindPandoraEquipSlotCallbacks();
	UPandoraInstance* GetCachedPandoraInstance(const UPandoraEquipSlotWidget* PandoraEquipSlot) const;
};
