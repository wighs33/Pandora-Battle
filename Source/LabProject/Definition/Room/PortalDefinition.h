#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/TextureRenderTarget2D.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PortalDefinition.generated.h"

class UMaterialInterface;
class UNiagaraSystem;
class UStaticMesh;

/**
 * 한 쌍의 포털이 공유하는 쿠킹 가능한 시각 에셋과 캡처 예산 설정을 정의한다.
 * 콘텐츠 이름을 안전하게 바꿀 수 있도록 에셋 참조는 네이티브 코드 밖에 둔다.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UPortalDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// Public API ------------------------------------------------------------------------------------------------------
	/** 화면 크기에 해상도 비율과 최대 크기를 적용한 렌더 타깃 크기. */
	FIntPoint GetRenderTargetSize(const FIntPoint& ViewportSize) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Assets",
		meta = (AssetBundles = "Portal"))
	TSoftObjectPtr<UStaticMesh> PortalPlaneMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Assets",
		meta = (AssetBundles = "Portal"))
	TSoftObjectPtr<UMaterialInterface> PortalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Assets",
		meta = (AssetBundles = "Portal"))
	TSoftObjectPtr<UNiagaraSystem> PortalEffect;

	/** 포털 렌더 타깃 하나가 쓰는 로컬 뷰포트 비율. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture",
		meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0"))
	float ResolutionScale = 1.0f;

	/** ResolutionScale을 적용한 뒤 렌더 타깃 가로·세로 각각의 상한. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture",
		meta = (ClampMin = "256", ClampMax = "4096", UIMin = "256", UIMax = "4096"))
	int32 MaxRenderTargetDimension = 1920;

	/** 게임 뷰포트가 없을 때 쓴다. 예: 월드 초기화 초반. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture",
		meta = (ClampMin = "16"))
	FIntPoint FallbackViewportSize = FIntPoint(1280, 720);

	/** LDR 캡처는 8비트 타깃이면 충분하다. 고정밀 포맷은 직접 켤 때만 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture")
	TEnumAsByte<ETextureRenderTargetFormat> RenderTargetFormat = RTF_RGBA8_SRGB;

	/** 0이면 갱신 빈도 제한이 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture",
		meta = (ClampMin = "0.0", ClampMax = "120.0", UIMin = "0.0", UIMax = "120.0"))
	float MaxCaptureFrameRate = 30.0f;
};
