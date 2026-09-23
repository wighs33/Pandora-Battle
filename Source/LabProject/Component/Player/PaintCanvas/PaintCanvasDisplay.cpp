#include "Component/Player/PaintCanvas/PaintCanvasDisplay.h"

#include "Character/PdPlayer.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogPaintCanvasPresentation, Log, All);

void UPaintCanvasDisplay::Initialize(APdPlayer* InPlayerOwner)
{
    PlayerOwner = InPlayerOwner;
}

void UPaintCanvasDisplay::SetSpeechBubbleComponent(
    UPrimitiveComponent* InSpeechBubbleComponent)
{
    SpeechBubbleComponent = InSpeechBubbleComponent;
}

UPrimitiveComponent* UPaintCanvasDisplay::FindSpeechBubbleComponent(
    const FName ComponentName) const
{
    if (IsValid(SpeechBubbleComponent))
    {
        return SpeechBubbleComponent.Get();
    }

    const APdPlayer* Player = PlayerOwner.Get();
    if (!Player)
    {
        return nullptr;
    }

    TArray<UPrimitiveComponent*> PrimitiveComponents;
    Player->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

    for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
    {
        if (!PrimitiveComponent)
        {
            continue;
        }

        const bool bMatchesConfiguredName =
            PrimitiveComponent->GetFName() == ComponentName
            || PrimitiveComponent->GetName().StartsWith(ComponentName.ToString());
        if (bMatchesConfiguredName)
        {
            return PrimitiveComponent;
        }
    }

    return nullptr;
}

bool UPaintCanvasDisplay::ShowSpeechBubble(
    UTextureRenderTarget2D* RenderTarget,
    const FName SpeechBubbleComponentName,
    const int32 MaterialIndex,
    const FName TextureParameterName)
{
    if (!RenderTarget)
    {
        return false;
    }

    UPrimitiveComponent* ResolvedSpeechBubbleComponent =
        FindSpeechBubbleComponent(SpeechBubbleComponentName);
    if (!ResolvedSpeechBubbleComponent)
    {
        UE_LOG(
            LogPaintCanvasPresentation,
            Warning,
            TEXT("Failed to show paint speech bubble. Component '%s' was not found."),
            *SpeechBubbleComponentName.ToString());
        return false;
    }

    UMaterialInterface* ExistingMaterial =
        ResolvedSpeechBubbleComponent->GetMaterial(MaterialIndex);
    if (!ExistingMaterial)
    {
        UE_LOG(
            LogPaintCanvasPresentation,
            Warning,
            TEXT("Failed to show paint speech bubble. %s has no material at index %d."),
            *ResolvedSpeechBubbleComponent->GetName(),
            MaterialIndex);
        return false;
    }

    ActiveSpeechBubbleComponent = ResolvedSpeechBubbleComponent;
    ActiveSpeechBubbleMaterial =
        Cast<UMaterialInstanceDynamic>(ExistingMaterial);
    if (!ActiveSpeechBubbleMaterial)
    {
        ActiveSpeechBubbleMaterial =
            ResolvedSpeechBubbleComponent->CreateDynamicMaterialInstance(
                MaterialIndex,
                ExistingMaterial);
    }

    if (!ActiveSpeechBubbleMaterial)
    {
        return false;
    }

    ActiveSpeechBubbleRenderTarget = RenderTarget;
    ActiveSpeechBubbleMaterial->SetTextureParameterValue(
        TextureParameterName,
        ActiveSpeechBubbleRenderTarget.Get());

    ResolvedSpeechBubbleComponent->SetVisibility(true, true);
    ResolvedSpeechBubbleComponent->SetHiddenInGame(false, true);
    ResolvedSpeechBubbleComponent->SetCollisionEnabled(
        ECollisionEnabled::NoCollision);
    ResolvedSpeechBubbleComponent->SetGenerateOverlapEvents(false);
    return true;
}

void UPaintCanvasDisplay::HideSpeechBubble(
    const FName SpeechBubbleComponentName)
{
    UPrimitiveComponent* ResolvedSpeechBubbleComponent =
        ActiveSpeechBubbleComponent.Get();
    if (!ResolvedSpeechBubbleComponent)
    {
        ResolvedSpeechBubbleComponent =
            FindSpeechBubbleComponent(SpeechBubbleComponentName);
    }

    if (ResolvedSpeechBubbleComponent)
    {
        ResolvedSpeechBubbleComponent->SetHiddenInGame(true, true);
        ResolvedSpeechBubbleComponent->SetVisibility(false, true);
        ResolvedSpeechBubbleComponent->SetCollisionEnabled(
            ECollisionEnabled::NoCollision);
    }

    ActiveSpeechBubbleComponent = nullptr;
    ActiveSpeechBubbleMaterial = nullptr;
    ActiveSpeechBubbleRenderTarget = nullptr;
}

bool UPaintCanvasDisplay::ApplyFaceDecal(
    UTextureRenderTarget2D* PaintSnapshot,
    UMaterialInterface* FaceDecalMaterial,
    FName AttachSocketName,
    const FTransform& FaceDecalTransformOffset,
    FVector FaceDecalSize,
    FName TextureParameterName)
{
    APdPlayer* Player = PlayerOwner.Get();
    USkeletalMeshComponent* CharacterMesh =
        Player ? Player->GetMesh() : nullptr;
    if (!Player || !PaintSnapshot || !FaceDecalMaterial || !CharacterMesh)
    {
        return false;
    }

    if (!AttachSocketName.IsNone()
        && !CharacterMesh->DoesSocketExist(AttachSocketName)
        && CharacterMesh->GetBoneIndex(AttachSocketName) == INDEX_NONE)
    {
        AttachSocketName = NAME_None;
    }

    ClearFaceDecal();

    ActiveFaceDecalSnapshot = PaintSnapshot;
    TextureParameterName = TextureParameterName.IsNone()
        ? FName(TEXT("RenderTarget"))
        : TextureParameterName;

    ActiveFaceDecalMaterial =
        UMaterialInstanceDynamic::Create(FaceDecalMaterial, this);
    if (!ActiveFaceDecalMaterial)
    {
        ActiveFaceDecalSnapshot = nullptr;
        return false;
    }

    ActiveFaceDecalMaterial->SetTextureParameterValue(
        TextureParameterName,
        ActiveFaceDecalSnapshot.Get());

    FaceDecalSize.X = FMath::Max(FaceDecalSize.X, 1.0);
    FaceDecalSize.Y = FMath::Max(FaceDecalSize.Y, 1.0);
    FaceDecalSize.Z = FMath::Max(FaceDecalSize.Z, 1.0);

    const bool bHasAttachSocket = !AttachSocketName.IsNone();
    const FTransform FaceDecalBaseTransform(
        Player->GetActorRotation(),
        bHasAttachSocket
            ? CharacterMesh->GetSocketLocation(AttachSocketName)
            : Player->GetActorLocation(),
        FVector::OneVector);

    FTransform FaceDecalWorldTransform =
        FaceDecalTransformOffset * FaceDecalBaseTransform;
    FaceDecalWorldTransform.NormalizeRotation();
    FaceDecalWorldTransform.AddToTranslation(
        FaceDecalWorldTransform.GetUnitAxis(EAxis::X)
        * (FaceDecalSize.X * 0.5));

    ActiveFaceDecalComponent = UGameplayStatics::SpawnDecalAttached(
        ActiveFaceDecalMaterial.Get(),
        FaceDecalSize,
        CharacterMesh,
        AttachSocketName,
        FaceDecalWorldTransform.GetLocation(),
        FaceDecalWorldTransform.GetRotation().Rotator(),
        EAttachLocation::KeepWorldPosition,
        0.0f);

    if (!IsValid(ActiveFaceDecalComponent))
    {
        ActiveFaceDecalMaterial = nullptr;
        ActiveFaceDecalSnapshot = nullptr;
        return false;
    }

    ActiveFaceDecalComponent->SetWorldScale3D(
        FaceDecalWorldTransform.GetScale3D());
    ActiveFaceDecalComponent->SetFadeScreenSize(0.0f);
    return true;
}

void UPaintCanvasDisplay::ClearFaceDecal()
{
    if (IsValid(ActiveFaceDecalComponent))
    {
        ActiveFaceDecalComponent->DestroyComponent();
    }

    ActiveFaceDecalComponent = nullptr;
    ActiveFaceDecalMaterial = nullptr;
    ActiveFaceDecalSnapshot = nullptr;
}

void UPaintCanvasDisplay::Reset(const FName SpeechBubbleComponentName)
{
    HideSpeechBubble(SpeechBubbleComponentName);
    ClearFaceDecal();
    PlayerOwner = nullptr;
}
