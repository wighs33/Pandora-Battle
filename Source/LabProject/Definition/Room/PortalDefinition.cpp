#include "Definition/Room/PortalDefinition.h"

#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PortalDefinition)

FPrimaryAssetId UPortalDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("PortalDefinition"), GetFName());
}

#if WITH_EDITOR
namespace
{
	template <typename AssetType>
	void ValidatePortalAsset(
		const TSoftObjectPtr<AssetType>& Asset,
		const FText& MissingMessage,
		const FText& InvalidMessage,
		FDataValidationContext& Context,
		EDataValidationResult& Result)
	{
		if (Asset.IsNull())
		{
			Context.AddError(MissingMessage);
			Result = EDataValidationResult::Invalid;
			return;
		}

		if (!Asset.LoadSynchronous())
		{
			Context.AddError(InvalidMessage);
			Result = EDataValidationResult::Invalid;
		}
	}
}

EDataValidationResult UPortalDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidatePortalAsset(
		PortalPlaneMesh,
		NSLOCTEXT("PortalDefinition", "MissingPortalMesh", "PortalPlaneMesh must be assigned."),
		NSLOCTEXT("PortalDefinition", "InvalidPortalMesh", "PortalPlaneMesh does not resolve to a cookable asset."),
		Context,
		Result);
	ValidatePortalAsset(
		PortalMaterial,
		NSLOCTEXT("PortalDefinition", "MissingPortalMaterial", "PortalMaterial must be assigned."),
		NSLOCTEXT("PortalDefinition", "InvalidPortalMaterial", "PortalMaterial does not resolve to a cookable asset."),
		Context,
		Result);
	ValidatePortalAsset(
		PortalEffect,
		NSLOCTEXT("PortalDefinition", "MissingPortalEffect", "PortalEffect must be assigned."),
		NSLOCTEXT("PortalDefinition", "InvalidPortalEffect", "PortalEffect does not resolve to a cookable asset."),
		Context,
		Result);

	if (ResolutionScale < 0.1f || ResolutionScale > 1.0f)
	{
		Context.AddError(NSLOCTEXT(
			"PortalDefinition",
			"InvalidResolutionScale",
			"ResolutionScale must be between 0.1 and 1.0."));
		Result = EDataValidationResult::Invalid;
	}

	if (MaxRenderTargetDimension < 256 || MaxRenderTargetDimension > 4096)
	{
		Context.AddError(NSLOCTEXT(
			"PortalDefinition",
			"InvalidMaximumDimension",
			"MaxRenderTargetDimension must be between 256 and 4096."));
		Result = EDataValidationResult::Invalid;
	}

	if (FallbackViewportSize.X < 16 || FallbackViewportSize.Y < 16)
	{
		Context.AddError(NSLOCTEXT(
			"PortalDefinition",
			"InvalidFallbackViewportSize",
			"FallbackViewportSize dimensions must both be at least 16."));
		Result = EDataValidationResult::Invalid;
	}

	if (MaxCaptureFrameRate < 0.0f || MaxCaptureFrameRate > 120.0f)
	{
		Context.AddError(NSLOCTEXT(
			"PortalDefinition",
			"InvalidCaptureFrameRate",
			"MaxCaptureFrameRate must be between 0 and 120."));
		Result = EDataValidationResult::Invalid;
	}

	if (RenderTargetFormat == RTF_RGBA16f || RenderTargetFormat == RTF_RGBA32f)
	{
		Context.AddWarning(NSLOCTEXT(
			"PortalDefinition",
			"HighPrecisionRenderTarget",
			"The portal uses an LDR capture source; a floating-point render target substantially increases memory without improving LDR output."));
	}

	return Result;
}
#endif
