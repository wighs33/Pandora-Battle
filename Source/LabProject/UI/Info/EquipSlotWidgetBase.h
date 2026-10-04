#pragma once

#include "UI/Common/LocalizedMenuWidget.h"
#include "GameplayTagContainer.h"
#include "Styling/SlateTypes.h"
#include "EquipSlotWidgetBase.generated.h"

class UButton;
class UDragDropOperation;
class UImage;
class UInfoWidget;
class UTextBlock;
class UTexture2D;

/**
 * Info 화면 왼쪽 장비 슬롯(아이템·스킨)의 공통 흐름.
 * 버튼 연결, 호버·드래그 강조, 빈 슬롯 아이콘, 선택 테두리, 상세 팝업 열고 닫기를 맡는다.
 * 슬롯이 담는 데이터와 상세 내용, 받을 수 있는 드롭, 데이터가 있을 때의 그리기는 파생 클래스가 정한다.
 */
UCLASS(Abstract)
class LABPROJECT_API UEquipSlotWidgetBase : public ULocalizedMenuWidget
{
	GENERATED_BODY()

protected:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

public:
	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|EquipSlot")
	void SetText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "!UI|EquipSlot")
	void SetIcon(UTexture2D* InIconTexture);

	UFUNCTION(BlueprintCallable, Category = "!UI|EquipSlot")
	void SetHoverIcon(UTexture2D* InIconTexture);

	UFUNCTION(BlueprintCallable, Category = "!UI|EquipSlot")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "!UI|EquipSlot")
	bool IsSelected() const { return bIsSelected; }

	UFUNCTION(BlueprintPure, Category = "!UI|EquipSlot")
	FText GetSlotText() const { return SlotText; }

	UFUNCTION(BlueprintPure, Category = "!UI|EquipSlot")
	UTexture2D* GetSlotIconTexture() const { return SlotIconTexture; }

	UFUNCTION(BlueprintPure, Category = "!UI|EquipSlot")
	UTexture2D* GetSlotHoverIconTexture() const { return SlotHoverIconTexture; }

	UFUNCTION(BlueprintPure, Category = "!UI|EquipSlot", meta = (Categories = "Item,Skin"))
	FGameplayTag GetEquipTypeTag() const { return EquipTypeTag; }

	UFUNCTION(BlueprintCallable, Category = "!UI|EquipSlot", meta = (Categories = "Item,Skin"))
	void SetResolvedEquipTypeTag(FGameplayTag InResolvedEquipTypeTag);

	/** 슬롯에 지정한 장비 태그를 우선하고, 없으면 목록이 정해 준 태그를 쓴다. */
	UFUNCTION(BlueprintPure, Category = "!UI|EquipSlot", meta = (Categories = "Item,Skin"))
	virtual FGameplayTag GetAcceptedEquipTypeTag() const;

protected:
	/** 슬롯에 아이템이나 스킨이 들어 있는지. 비어 있을 때만 슬롯 아이콘을 바꾼다. */
	virtual bool HasSlotContent() const PURE_VIRTUAL(UEquipSlotWidgetBase::HasSlotContent, return false;);

	/** 담긴 데이터의 상세 팝업을 이 슬롯 옆에 띄운다. 비어 있으면 false를 돌려 팝업을 닫게 한다. */
	virtual bool ShowSlotDetail(UInfoWidget& InfoWidget) PURE_VIRTUAL(UEquipSlotWidgetBase::ShowSlotDetail, return false;);

	/** 드래그 중인 항목을 이 슬롯이 받을 수 있는지. */
	virtual bool CanAcceptDragOperation(UDragDropOperation* Operation) const PURE_VIRTUAL(UEquipSlotWidgetBase::CanAcceptDragOperation, return false;);

	/** 받을 수 있는 항목이 떨어졌을 때 파생 클래스의 드롭 알림을 보낸다. */
	virtual void BroadcastAcceptedDrop(UDragDropOperation* Operation) PURE_VIRTUAL(UEquipSlotWidgetBase::BroadcastAcceptedDrop, );

	/** 버튼이 눌렸을 때 파생 클래스의 클릭 알림을 보낸다. */
	virtual void BroadcastSlotClicked() PURE_VIRTUAL(UEquipSlotWidgetBase::BroadcastSlotClicked, );

	/** 현재 상태로 버튼 배경, 아이콘, 글자, 선택 테두리를 다시 그린다. */
	virtual void ApplySlotVisual() PURE_VIRTUAL(UEquipSlotWidgetBase::ApplySlotVisual, );

	virtual UTexture2D* GetCurrentIconTexture(bool bForHover) const;

	// Internal Helpers ------------------------------------------------------------------------------------------------
	/** 호버나 받을 수 있는 드래그가 올라와 있으면 호버 아이콘을 보여 준다. */
	UTexture2D* GetDisplayIconTexture() const;

	/** 버튼 스타일에 그릴 평상시 아이콘. 받을 수 있는 드래그가 올라와 있으면 호버 아이콘을 쓴다. */
	UTexture2D* GetRestIconTexture() const;

	void ResetSlotIcons();
	void ApplySlotText(bool bHasAnyIcon);
	void ApplySelectionBorder();
	FButtonStyle MakeSlotIconButtonStyle(FButtonStyle ButtonStyle, float Opacity) const;
	static FSlateBrush MakeSolidBrush(const FSlateBrush& SourceBrush, const FLinearColor& TintColor);

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	UFUNCTION()
	void HandleButtonClicked();

	UFUNCTION()
	void HandleButtonHovered();

	UFUNCTION()
	void HandleButtonUnhovered();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	void CacheDefaultButtonStyle();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|EquipSlot", meta = (Categories = "Item,Skin"))
	FGameplayTag EquipTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|EquipSlot")
	TObjectPtr<UTexture2D> SlotIconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|EquipSlot")
	TObjectPtr<UTexture2D> SlotHoverIconTexture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|EquipSlot|Style")
	FLinearColor SelectionBorderDefaultColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.35f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|EquipSlot|Style")
	FLinearColor SelectionBorderSelectedColor = FLinearColor(0.0f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|EquipSlot|Style")
	FLinearColor ButtonNormalColor = FLinearColor(0.55f, 0.85f, 0.38f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|EquipSlot|Style")
	FLinearColor ButtonHoverColor = FLinearColor(1.0f, 0.92f, 0.1f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|EquipSlot|Style")
	FLinearColor ButtonPressedColor = FLinearColor(0.72f, 0.66f, 0.08f, 1.0f);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|EquipSlot|Bind")
	TObjectPtr<UButton> ItemButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|EquipSlot|Bind")
	TObjectPtr<UTextBlock> ApplyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|EquipSlot|Bind")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|EquipSlot|Bind")
	TObjectPtr<UImage> SelectionBorderImage;

	UPROPERTY(Transient)
	FText SlotText;

	UPROPERTY(Transient)
	FGameplayTag ResolvedEquipTypeTag;

	FButtonStyle DefaultButtonStyle;
	bool bHasDefaultButtonStyle = false;
	bool bIsButtonHovered = false;
	bool bIsAcceptedDragHovered = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CurrentHoverIconTexture;

	bool bIsSelected = false;
};
