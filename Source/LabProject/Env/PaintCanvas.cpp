#include "Env/PaintCanvas.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PaintCanvas)

namespace
{
const FName CanvasComponentName(TEXT("Canvas"));

double GetVectorAxisValue(const FVector& Vector, const int32 AxisIndex)
{
	switch (AxisIndex)
	{
	case 0:
		return Vector.X;
	case 1:
		return Vector.Y;
	default:
		return Vector.Z;
	}
}

void FindSurfaceAxes(const FVector& BoundsSize, int32& OutUAxis, int32& OutVAxis)
{
	int32 ThinAxis = 0;
	double ThinSize = BoundsSize.X;

	if (BoundsSize.Y < ThinSize)
	{
		ThinAxis = 1;
		ThinSize = BoundsSize.Y;
	}

	if (BoundsSize.Z < ThinSize)
	{
		ThinAxis = 2;
	}

	OutUAxis = ThinAxis == 0 ? 1 : 0;
	OutVAxis = ThinAxis == 2 ? 1 : 2;
}

bool IsOwnedInstanceSceneComponent(const USceneComponent* Component, const AActor* Owner)
{
	return Component && Component->GetOwner() == Owner && !Component->IsTemplate();
}

USceneComponent* FindOwnedInstanceSceneComponentMatching(
	const TArray<USceneComponent*>& Components,
	const USceneComponent* TemplateComponent,
	const AActor* Owner)
{
	if (!TemplateComponent)
	{
		return nullptr;
	}

	for (USceneComponent* Component : Components)
	{
		if (IsOwnedInstanceSceneComponent(Component, Owner)
			&& Component->GetFName() == TemplateComponent->GetFName())
		{
			return Component;
		}
	}

	const FString TemplateName = TemplateComponent->GetName();
	for (USceneComponent* Component : Components)
	{
		if (IsOwnedInstanceSceneComponent(Component, Owner)
			&& Component->GetName().StartsWith(TemplateName))
		{
			return Component;
		}
	}

	return nullptr;
}

void AttachSceneComponentKeepingSetup(
	USceneComponent* Component,
	USceneComponent* Parent,
	const FName SocketName)
{
	if (!Component || !Parent || Component == Parent)
	{
		return;
	}

	if (Component->IsRegistered())
	{
		Component->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, SocketName);
	}
	else
	{
		Component->SetupAttachment(Parent, SocketName);
	}
}
}

APaintCanvas::APaintCanvas(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRootComponent);

	CanvasComponent = CreateDefaultSubobject<UStaticMeshComponent>(CanvasComponentName);
	CanvasComponent->SetupAttachment(SceneRootComponent);
	ApplyCollisionSettings();

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CanvasMaterialAsset(TEXT("/Game/Materials/M_Canvas.M_Canvas"));
	if (CanvasMaterialAsset.Succeeded())
	{
		CanvasMaterialParent = CanvasMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BrushMaterialAsset(TEXT("/Game/Materials/M_Brush.M_Brush"));
	if (BrushMaterialAsset.Succeeded())
	{
		BrushMaterialParent = BrushMaterialAsset.Object;
	}
}

void APaintCanvas::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();
	RepairTemplateMismatchAttachments();
}

void APaintCanvas::BeginPlay()
{
	Super::BeginPlay();
	ApplyCollisionSettings();
	NativeInitializeCanvas();
}

void APaintCanvas::RepairTemplateMismatchAttachments()
{
	if (IsTemplate())
	{
		return;
	}

	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);

	USceneComponent* InstanceRoot = GetRootComponent();
	if (!IsOwnedInstanceSceneComponent(InstanceRoot, this))
	{
		InstanceRoot = FindOwnedInstanceSceneComponentMatching(SceneComponents, InstanceRoot, this);
	}

	if (!InstanceRoot && IsOwnedInstanceSceneComponent(SceneRootComponent, this))
	{
		InstanceRoot = SceneRootComponent.Get();
	}

	if (!InstanceRoot)
	{
		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (IsOwnedInstanceSceneComponent(SceneComponent, this)
				&& SceneComponent->GetFName() == TEXT("SceneRoot"))
			{
				InstanceRoot = SceneComponent;
				break;
			}
		}
	}

	if (InstanceRoot && GetRootComponent() != InstanceRoot)
	{
		SetRootComponent(InstanceRoot);
	}

	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (!IsOwnedInstanceSceneComponent(SceneComponent, this)
			|| SceneComponent == InstanceRoot)
		{
			continue;
		}

		USceneComponent* AttachParent = SceneComponent->GetAttachParent();
		if (!AttachParent || IsOwnedInstanceSceneComponent(AttachParent, this))
		{
			continue;
		}

		USceneComponent* ReplacementParent =
			FindOwnedInstanceSceneComponentMatching(SceneComponents, AttachParent, this);
		if (!ReplacementParent || ReplacementParent == SceneComponent)
		{
			ReplacementParent = InstanceRoot;
		}

		AttachSceneComponentKeepingSetup(
			SceneComponent,
			ReplacementParent,
			SceneComponent->GetAttachSocketName());
	}
}

void APaintCanvas::ConfigureCollisionForPaintTrace(const ETraceTypeQuery InPaintTraceChannel)
{
	PaintTraceChannel = InPaintTraceChannel;
	ApplyCollisionSettings();
}

void APaintCanvas::NativeInitializeCanvas()
{
	if (!RenderTarget)
	{
		RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
			this,
			FMath::Max(RenderTargetWidth, 1),
			FMath::Max(RenderTargetHeight, 1),
			RTF_RGBA16f,
			FLinearColor::Black,
			false,
			false);
	}

	if (RenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTarget, ClearColor);
	}

	if (!CanvasMaterial && CanvasMaterialParent)
	{
		CanvasMaterial = UMaterialInstanceDynamic::Create(CanvasMaterialParent, this);
	}

	if (CanvasMaterial && RenderTarget)
	{
		CanvasMaterial->SetTextureParameterValue(RenderTargetParameterName, RenderTarget);

		if (UStaticMeshComponent* ResolvedCanvasComponent = GetResolvedCanvasComponent())
		{
			ResolvedCanvasComponent->SetMaterial(0, CanvasMaterial);
		}
	}

	if (!BrushMaterial && BrushMaterialParent)
	{
		BrushMaterial = UMaterialInstanceDynamic::Create(BrushMaterialParent, this);
	}

	ApplyBrushMaterialParameters();
}

void APaintCanvas::DrawBrush(UTexture2D* BrushTexture, const double BrushSize, const FVector2D DrawLocation)
{
	if (!RenderTarget || !BrushMaterial)
	{
		NativeInitializeCanvas();
	}

	if (!RenderTarget || !BrushMaterial || BrushSize <= 0.0)
	{
		return;
	}

	BrushMaterial->SetTextureParameterValue(BrushTextureParameterName, BrushTexture);
	ApplyBrushMaterialParameters();

	UCanvas* DrawCanvas = nullptr;
	FVector2D RenderTargetSize = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, RenderTarget, DrawCanvas, RenderTargetSize, Context);

	if (DrawCanvas)
	{
		const FVector2D BrushScreenSize(BrushSize, BrushSize);
		const FVector2D BrushScreenPosition = RenderTargetSize * DrawLocation - BrushScreenSize * 0.5;
		DrawCanvas->K2_DrawMaterial(
			BrushMaterial,
			BrushScreenPosition,
			BrushScreenSize,
			FVector2D::ZeroVector,
			FVector2D(1.0, 1.0),
			0.0f,
			FVector2D(0.5, 0.5));
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
}

void APaintCanvas::ApplyCollisionSettings() const
{
	const ECollisionChannel PaintCollisionChannel = UEngineTypes::ConvertToCollisionChannel(PaintTraceChannel.GetValue());

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PrimitiveComponent->SetCollisionObjectType(ECC_WorldDynamic);
		PrimitiveComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		PrimitiveComponent->SetCollisionResponseToChannel(PaintCollisionChannel, ECR_Block);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
	}
}

void APaintCanvas::ApplyBrushMaterialParameters() const
{
	if (!BrushMaterial)
	{
		return;
	}

	BrushMaterial->SetVectorParameterValue(BrushColorParameterName, BrushColor);
	BrushMaterial->SetVectorParameterValue(TEXT("BaseColor"), BrushColor);
}

bool APaintCanvas::TryGetDrawLocationFromHitResult(const FHitResult& HitResult, FVector2D& OutDrawLocation) const
{
	const UStaticMeshComponent* HitStaticMeshComponent = Cast<UStaticMeshComponent>(HitResult.GetComponent());
	const UStaticMeshComponent* TargetCanvasComponent = HitStaticMeshComponent && HitStaticMeshComponent->GetOwner() == this
		? HitStaticMeshComponent
		: GetResolvedCanvasComponent();
	if (!TargetCanvasComponent || !TargetCanvasComponent->GetStaticMesh())
	{
		return false;
	}

	const FBox LocalBounds = TargetCanvasComponent->GetStaticMesh()->GetBoundingBox();
	if (!LocalBounds.IsValid)
	{
		return false;
	}

	const FVector BoundsSize = LocalBounds.GetSize();
	int32 UAxis = 0;
	int32 VAxis = 1;
	FindSurfaceAxes(BoundsSize, UAxis, VAxis);

	const double USize = GetVectorAxisValue(BoundsSize, UAxis);
	const double VSize = GetVectorAxisValue(BoundsSize, VAxis);
	if (FMath::IsNearlyZero(USize) || FMath::IsNearlyZero(VSize))
	{
		return false;
	}

	const FVector LocalImpactPoint = TargetCanvasComponent->GetComponentTransform().InverseTransformPosition(
		HitResult.ImpactPoint.IsNearlyZero() ? HitResult.Location : HitResult.ImpactPoint);
	const double LocalMinU = GetVectorAxisValue(LocalBounds.Min, UAxis);
	const double LocalMinV = GetVectorAxisValue(LocalBounds.Min, VAxis);

	double U = (GetVectorAxisValue(LocalImpactPoint, UAxis) - LocalMinU) / USize;
	double V = (GetVectorAxisValue(LocalImpactPoint, VAxis) - LocalMinV) / VSize;

	if (bFlipDrawLocationX)
	{
		U = 1.0 - U;
	}

	if (bFlipDrawLocationY)
	{
		V = 1.0 - V;
	}

	OutDrawLocation = FVector2D(
		FMath::Clamp(U, 0.0, 1.0),
		FMath::Clamp(V, 0.0, 1.0));
	return true;
}

UStaticMeshComponent* APaintCanvas::GetResolvedCanvasComponent() const
{
	UStaticMeshComponent* NamedCanvasComponent = nullptr;
	UStaticMeshComponent* FirstMeshComponent = nullptr;
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents<UStaticMeshComponent>(StaticMeshComponents);

	for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (!StaticMeshComponent)
		{
			continue;
		}

		const bool bLooksLikeCanvasComponent =
			StaticMeshComponent->GetFName() == CanvasComponentName
			|| StaticMeshComponent->GetName().StartsWith(CanvasComponentName.ToString());

		if (StaticMeshComponent->GetStaticMesh())
		{
			if (bLooksLikeCanvasComponent)
			{
				return StaticMeshComponent;
			}

			if (!FirstMeshComponent)
			{
				FirstMeshComponent = StaticMeshComponent;
			}
		}

		if (!NamedCanvasComponent && bLooksLikeCanvasComponent)
		{
			NamedCanvasComponent = StaticMeshComponent;
		}
	}

	if (FirstMeshComponent)
	{
		return FirstMeshComponent;
	}

	return NamedCanvasComponent ? NamedCanvasComponent : CanvasComponent.Get();
}
