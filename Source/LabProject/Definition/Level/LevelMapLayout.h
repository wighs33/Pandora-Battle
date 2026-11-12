#pragma once

#include "CoreMinimal.h"
#include "Common/PlayerMapRegion.h"
#include "LevelMapLayout.generated.h"

class UTexture2D;

// 이 파일의 구조체는 맵 목록(DA_Level)에만 들어가는 데이터다. 지도 스크립트(Scripts/MapDesign/level_map.py)가
// 구조체 사본의 값을 고칠 수 있게 EditAnywhere로 둔다.

/**
 * 위에서 똑바로 내려다본 지도 한 장(지역 지도, 또는 지도가 한 장뿐인 레벨의 지도).
 * 그림은 이 월드 사각형을 정사영으로 찍은 것이라, 같은 값으로 플레이어 위치를 그림 위에 놓는다.
 */
USTRUCT(BlueprintType)
struct LABPROJECT_API FLevelMapPage
{
	GENERATED_BODY()

	/** 지역 지도일 때 그 지역. 지역 트리거가 플레이어에게 정한 지역과 같은 지도에만 표식을 놓는다. 지도가 한 장인 레벨에서는 쓰지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map")
	EPlayerMapRegion Region = EPlayerMapRegion::Dome;

	/** 지도 이름(MenuText 키). 비우면 레벨 이름을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map")
	FName NameKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Texture;

	/** 그림 가운데의 월드 위치(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map|World")
	FVector2D WorldCenter = FVector2D::ZeroVector;

	/** 그림 오른쪽이 가리키는 월드 방향각(도). 그림 아래쪽은 여기서 90도 돈 방향이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map|World")
	float WorldYaw = 0.0f;

	/** 그림이 덮는 월드 넓이(cm, 그림 가로·세로). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map|World", meta = (ClampMin = "1.0"))
	FVector2D WorldSize = FVector2D(10000.0f, 10000.0f);

	/** 전체 지도에서 이 지역 단추가 가리키는 월드 위치(지역의 가운데). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map|Area")
	FVector AreaAnchor = FVector::ZeroVector;

	/** 전체 지도에서 이 지역 단추에 마우스를 올리면 덧그리는 빛(전체 지도와 같은 크기의 투명 그림). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map|Area", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> AreaHighlight;

#if WITH_EDITORONLY_DATA
	/** 지도를 찍을 때 이 높이 위를 잘라 낸다(동굴 천장이나 바위가 덮은 지역). 지도 찍기 도구만 쓴다. */
	UPROPERTY(EditAnywhere, Category = "!Map|Capture")
	bool bCaptureClipAbove = false;

	UPROPERTY(EditAnywhere, Category = "!Map|Capture", meta = (EditCondition = "bCaptureClipAbove"))
	float CaptureClipHeight = 0.0f;
#endif

	/** 월드 위치를 그림 위 비율 위치(0~1, 왼쪽 위 원점)로 옮긴다. 그림 밖이면 가장자리로 붙인다. */
	FVector2D ToMapUV(const FVector& WorldLocation) const;

	/** 월드 방향을 표식 회전각(도, UMG처럼 시계 방향)으로 옮긴다. 표식 그림이 아래를 향한 것이 0도다. */
	float ToMapAngle(const FVector& WorldForward) const;

	/** 그림을 월드 비율 그대로 MaxSize 안에 가장 크게 넣은 화면 크기. */
	FVector2D FitViewSize(const FVector2D& MaxSize) const;
};

/** 지역을 고르는 전체 지도: 레벨 전체를 비스듬히 내려다본 정사영 그림과 그 카메라. 지역 단추 자리는 이 카메라로 정한다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FLevelMapAreaView
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UTexture2D> Texture;

	/** 카메라가 보는 점(그림 가운데)의 월드 위치. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map")
	FVector Center = FVector::ZeroVector;

	/** 카메라가 보는 방향각과 내려다보는 각(도, 음수가 아래). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map")
	float Yaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map", meta = (ClampMin = "-89.0", ClampMax = "-1.0"))
	float Pitch = -40.0f;

	/** 그림(정사각형) 한 변이 덮는 월드 길이(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map", meta = (ClampMin = "1.0"))
	float WorldWidth = 10000.0f;

	/** 월드 위치를 전체 지도 그림 위 비율 위치(0~1)로 옮긴다. */
	FVector2D ToMapUV(const FVector& WorldLocation) const;
};

/** 레벨의 지도: 지역 지도들과, 지역이 둘 이상이면 지역을 고르는 전체 지도. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FLevelMapLayout
{
	GENERATED_BODY()

	/** 지역 지도. 한 장이면 그 레벨의 지도이고, 여러 장이면 전체 지도에서 지역을 고른다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map", meta = (TitleProperty = "Region"))
	TArray<FLevelMapPage> Pages;

	/** 지역 지도가 둘 이상일 때 지역을 고르는 전체 지도. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "!Map")
	FLevelMapAreaView AreaView;

#if WITH_EDITORONLY_DATA
	/** 맵 썸네일 카메라가 놀이 공간을 바라보는 방향각(도). 지도 찍기 도구만 쓴다. */
	UPROPERTY(EditAnywhere, Category = "!Map|Capture")
	float ThumbnailYaw = 45.0f;
#endif

	bool HasMap() const { return !Pages.IsEmpty(); }
	bool HasAreaView() const { return Pages.Num() > 1 && !AreaView.Texture.IsNull(); }
	int32 FindPageIndex(EPlayerMapRegion Region) const;
	void CollectAssetPaths(TArray<FSoftObjectPath>& OutPaths) const;
};
