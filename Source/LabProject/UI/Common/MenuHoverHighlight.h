#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MenuHoverHighlight.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;

/** 강조 표식의 모양. */
UENUM()
enum class EMenuHoverMarker : uint8
{
	/** 왼쪽 세로 막대가 가운데에서 자라고, 띠가 오른쪽으로 옅어지며, 오른쪽 끝에 화살표가 나온다. 큰 메뉴 줄에 쓴다. */
	SideBar,
	/** 아래 밑줄이 왼쪽에서 자라고 띠가 고르게 깔린다. 작은 글자 버튼에 쓴다. */
	Underline,
};

/** 글자 목록 메뉴의 강조 모양과 속도. 화면 위젯의 기본값에서 고친다. */
USTRUCT(BlueprintType)
struct FMenuHoverHighlightStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Hover")
	FLinearColor AccentColor = FLinearColor::FromSRGBColor(FColor(214, 176, 100));

	/** 다 강조됐을 때 띠의 불투명도 */
	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "0", ClampMax = "1"))
	float BandOpacity = 0.22f;

	/** SideBar 띠가 다 옅어지는 지점(줄 폭에 대한 비율) */
	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "0.1", ClampMax = "1"))
	float BandFadeEnd = 0.85f;

	/** 막대 폭, 밑줄 두께 */
	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "1"))
	float MarkerThickness = 4.f;

	/** SideBar 막대가 다 자랐을 때 줄 높이에 대한 비율 */
	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "0.1", ClampMax = "1"))
	float MarkerLengthFraction = 0.72f;

	/** 강조될 때 원래 내용(글자)이 오른쪽으로 밀리는 거리 */
	UPROPERTY(EditAnywhere, Category = "Hover")
	float SlideDistance = 10.f;

	/** 강조되지 않은 내용의 불투명도. 강조되면 1로 밝아진다. */
	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "0", ClampMax = "1"))
	float IdleContentOpacity = 0.82f;

	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "8"))
	int32 ChevronFontSize = 24;

	/** 화살표가 오른쪽 끝에서 떨어진 거리 */
	UPROPERTY(EditAnywhere, Category = "Hover")
	float ChevronInset = 14.f;

	/** 강조가 다 켜지거나 꺼지는 시간 */
	UPROPERTY(EditAnywhere, Category = "Hover", meta = (ClampMin = "0.01", Units = "s"))
	float Duration = 0.12f;
};

/** 강조를 붙인 버튼 하나와 그 부품. */
USTRUCT()
struct FMenuHoverEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UButton> Button;

	UPROPERTY()
	TObjectPtr<UImage> Band;

	UPROPERTY()
	TObjectPtr<UImage> Marker;

	UPROPERTY()
	TObjectPtr<UTextBlock> Chevron;

	/** 버튼에 원래 있던 내용. 강조되면 함께 밀리고 밝아진다. */
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> Content;

	EMenuHoverMarker MarkerType = EMenuHoverMarker::SideBar;

	/** 0이면 원래 모습, 1이면 다 강조된 모습. 선형으로 움직이고 그릴 때 감속 곡선을 거친다. */
	float Progress = 0.f;
};

/**
 * 판 없는 글자 목록 메뉴의 버튼이 버튼처럼 느껴지도록, 마우스를 올리거나 패드로 고른 줄을 강조한다.
 * 버튼의 내용은 캔버스 패널이어야 한다. 띠·표식·화살표를 그 캔버스에 덧붙이고, 원래 내용은 밀고 밝힐 대상으로 둔다.
 * 비활성 버튼에는 마우스가 올라가도 Slate가 알리지 않으므로 강조되지 않는다.
 */
UCLASS()
class LABPROJECT_API UMenuHoverHighlight : public UObject
{
	GENERATED_BODY()

public:
	void SetStyle(const FMenuHoverHighlightStyle& InStyle);

	/** 버튼에 강조 부품을 붙인다. 같은 버튼에 두 번 붙이지 않는다. */
	void Decorate(UButton* Button, EMenuHoverMarker MarkerType);

	/** 매 프레임 부른다. 강조할 버튼은 1로, 나머지는 0으로 Duration에 걸쳐 옮긴다. */
	void Tick(float DeltaSeconds);

	/** 버튼의 강조 진행도(0~1). 붙이지 않은 버튼은 0. */
	float GetProgress(const UWidget* Button) const;

private:
	bool ShouldHighlight(const UButton& Button) const;
	void Apply(const FMenuHoverEntry& Entry) const;
	UTexture2D* GetBandFadeTexture();

	FMenuHoverHighlightStyle Style;

	UPROPERTY()
	TArray<FMenuHoverEntry> Entries;

	/** SideBar 띠의 왼쪽에서 오른쪽으로 옅어지는 모양. 실행 중에 한 번 만든다. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BandFadeTexture;
};
