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
 * Cookable visual assets and capture-budget settings shared by a portal pair.
 * Asset references stay outside native code so content can be renamed safely.
 */
UCLASS(BlueprintType, Const)
class LABPROJECT_API UPortalDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Assets",
		meta = (AssetBundles = "Portal"))
	TSoftObjectPtr<UStaticMesh> PortalPlaneMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Assets",
		meta = (AssetBundles = "Portal"))
	TSoftObjectPtr<UMaterialInterface> PortalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Assets",
		meta = (AssetBundles = "Portal"))
	TSoftObjectPtr<UNiagaraSystem> PortalEffect;

	/** Fraction of the local viewport used by each portal render target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture",
		meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0"))
	float ResolutionScale = 1.0f;

	/** Upper bound for either render-target dimension after applying ResolutionScale. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture",
		meta = (ClampMin = "256", ClampMax = "4096", UIMin = "256", UIMax = "4096"))
	int32 MaxRenderTargetDimension = 1920;

	/** Used when no game viewport exists, for example during early world initialization. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture",
		meta = (ClampMin = "16"))
	FIntPoint FallbackViewportSize = FIntPoint(1280, 720);

	/** LDR capture only needs an 8-bit target; high-precision formats should be opt-in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture")
	TEnumAsByte<ETextureRenderTargetFormat> RenderTargetFormat = RTF_RGBA8_SRGB;

	/** Zero removes the rate limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal|Capture",
		meta = (ClampMin = "0.0", ClampMax = "120.0", UIMin = "0.0", UIMax = "120.0"))
	float MaxCaptureFrameRate = 30.0f;
};
