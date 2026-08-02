#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "PaintCanvas.generated.h"

struct FHitResult;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;

UCLASS(Blueprintable)
class LABPROJECT_API APaintCanvas : public AActor
{
	GENERATED_BODY()

public:
	APaintCanvas(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PreRegisterAllComponents() override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Paint|Canvas")
	void NativeInitializeCanvas();

	UFUNCTION(BlueprintCallable, Category = "Paint|Canvas")
	void DrawBrush(UTexture2D* BrushTexture, double BrushSize, FVector2D DrawLocation);

	UFUNCTION(BlueprintCallable, Category = "Paint|Collision")
	void ConfigureCollisionForPaintTrace(ETraceTypeQuery InPaintTraceChannel);

	bool TryGetDrawLocationFromHitResult(const FHitResult& HitResult, FVector2D& OutDrawLocation) const;

	UFUNCTION(BlueprintPure, Category = "Paint|Canvas")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget.Get(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRootComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Paint|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> CanvasComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Render Target", meta = (ClampMin = "1"))
	int32 RenderTargetWidth = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Render Target", meta = (ClampMin = "1"))
	int32 RenderTargetHeight = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Render Target")
	FLinearColor ClearColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Material")
	TObjectPtr<UMaterialInterface> CanvasMaterialParent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Material")
	TObjectPtr<UMaterialInterface> BrushMaterialParent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Material")
	FName RenderTargetParameterName = TEXT("RenderTarget");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Material")
	FName BrushTextureParameterName = TEXT("BrushTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Material")
	FName BrushColorParameterName = TEXT("BrushColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Material")
	FLinearColor BrushColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Input")
	bool bFlipDrawLocationX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Input")
	bool bFlipDrawLocationY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Paint|Collision")
	TEnumAsByte<ETraceTypeQuery> PaintTraceChannel = TraceTypeQuery1;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Paint|Runtime")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Paint|Runtime")
	TObjectPtr<UMaterialInstanceDynamic> CanvasMaterial;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Paint|Runtime")
	TObjectPtr<UMaterialInstanceDynamic> BrushMaterial;

private:
	void RepairTemplateMismatchAttachments();
	void ApplyCollisionSettings() const;
	void ApplyBrushMaterialParameters() const;
	UStaticMeshComponent* GetResolvedCanvasComponent() const;
};
