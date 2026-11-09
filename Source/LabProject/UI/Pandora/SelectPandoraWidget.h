#pragma once

#include "CoreMinimal.h"
#include "Common/Enum_Direction.h"
#include "UI/Common/UiMotion.h"
#include "UI/Core/ScreenWidget.h"
#include "SelectPandoraWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelectedPandoraDirection, EEnum_Direction, Direction);

UCLASS(Blueprintable, BlueprintType)
class LABPROJECT_API USelectPandoraWidget : public UScreenWidget
{
	GENERATED_BODY()

public:
	USelectPandoraWidget(const FObjectInitializer& ObjectInitializer);

	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Public API ------------------------------------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraImage(int32 Nth, UTexture2D* PandoraImage);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetPandoraEnabled(int32 Nth, bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetWeaponImage(int32 Nth, UTexture2D* WeaponImage);

	UFUNCTION(BlueprintCallable, Category = "!UI|Pandora")
	void SetDirection(int32 Index);

	/** 마우스가 가리키는 칸(-1 가운데, 0 위, 1 오른쪽, 2 아래, 3 왼쪽). 휠이 그 칸을 강조하고 가운데에 이름을 보인다. */
	void SetHoveredDirection(int32 Index);

protected:
	virtual void OnMenuLanguageChanged() override;

private:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	void CacheDefaultPandoraBrushes();
	void RefreshSelectedLoadoutNumber();
	void SetSelectedLoadoutNumberVisibility(int32 LoadoutNumber) const;
	void SelectDirection(EEnum_Direction InDirection, bool bBroadcast);
	void RefreshCenterLabel();
	/** 칸(0 위, 1 오른쪽, 2 아래, 3 왼쪽)의 뚜껑 패널. 뚜껑·소켓·무기판이 함께 들린다. */
	UWidget* GetSegmentPanel(int32 SegmentIndex) const;
	/** 칸을 가리켰을 때 켜지는 뚜껑 테두리·소켓 빛 */
	UWidget* GetSegmentLit(int32 SegmentIndex) const;

public:
	UPROPERTY(BlueprintAssignable, BlueprintCallable, Category = "!UI|Pandora")
	FOnSelectedPandoraDirection OnSelected;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> FirstPandoraImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> SecondPandoraImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> ThirdPandoraImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> FirstWeaponImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> SecondWeaponImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> ThirdWeaponImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> Img_FirstCut;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> Img_SecondCut;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> Img_ThirdCut;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> Img_Highlight1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> Img_Highlight2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "!UI|Pandora|Bind")
	TObjectPtr<UImage> Img_Highlight3;

	/** 칸마다 뚜껑 한 쪽과 그 위의 소켓·무기판을 묶은 패널. 가리킨 칸이 바깥으로 들린다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> Pnl_SecondSegment;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> Pnl_ThirdSegment;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> Pnl_UnequipSegment;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> Pnl_FirstSegment;

	/** 칸마다 가리켰을 때 켜지는 빛(뚜껑 테두리, 소켓 이와 고리) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> Img_SecondLit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> Img_ThirdLit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> Img_UnequipLit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> Img_FirstLit;

	/** 가리킨 칸 양옆 틈의 원소 빛과 봉인 띠의 빛. 위 칸을 가리킨 모습으로 그려 두고 가리킨 칸 쪽으로 돌린다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UWidget> Img_WheelHover;

	/** 가운데에 가리키는 칸의 이름을 보인다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> Txt_WheelCenter;

	/** 아래 칸(장착 해제) 글자 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "!UI|Pandora|Bind")
	TObjectPtr<UTextBlock> Txt_Unequip;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "!UI|Pandora")
	EEnum_Direction Direction = EEnum_Direction::Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style", meta = (AllowPrivateAccess = "true"))
	FLinearColor EnabledPandoraTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!UI|Pandora|Style", meta = (AllowPrivateAccess = "true"))
	FLinearColor DisabledPandoraTint = FLinearColor(0.15f, 0.15f, 0.15f, 0.55f);

	/** 휠이 열릴 때 이 크기에서 시작해 스프링으로 1배에 자리 잡는다. */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|Pandora|Motion", meta = (ClampMin = "0.1", ClampMax = "1"))
	float OpenStartScale = 0.86f;

	/** 칸들이 차례로 나타나는 간격 */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|Pandora|Motion", meta = (ClampMin = "0", Units = "s"))
	float SegmentStagger = 0.04f;

	/** 칸 하나가 다 나타나는 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|Pandora|Motion", meta = (ClampMin = "0.01", Units = "s"))
	float SegmentFadeDuration = 0.18f;

	/** 가리킨 칸의 뚜껑이 바깥으로 들리는 거리 */
	UPROPERTY(EditDefaultsOnly, Category = "!UI|Pandora|Motion", meta = (ClampMin = "0"))
	float LidLiftDistance = 12.f;

private:
	/** 판도라가 없는 소켓의 브러시(빈 소켓 그림). 판도라 그림은 이 브러시에 그림만 바꿔 끼워 같은 원 모양으로 그린다. */
	UPROPERTY(Transient)
	TArray<FSlateBrush> DefaultPandoraBrushes;

	UPROPERTY(Transient)
	bool bDefaultPandoraBrushesCached = false;

	int32 HoveredDirectionIndex = INDEX_NONE;
	float OpenElapsed = 0.f;
	UiMotion::FSpring OpenScale;
	UiMotion::FSpring HoverAngle;
	UiMotion::FSpring HoverAmount;
	UiMotion::FSpring SegmentLifts[4];
};
