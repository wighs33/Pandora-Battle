#include "UI/Controller/InfoCharacterPreviewController.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/Widget/InfoWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InfoCharacterPreviewController)

namespace
{
void CutToViewTarget(APlayerController* PlayerController, AActor* ViewTarget)
{
	if (!PlayerController || !IsValid(ViewTarget))
	{
		return;
	}
	PlayerController->SetViewTarget(ViewTarget);
	if (PlayerController->PlayerCameraManager)
	{
		PlayerController->PlayerCameraManager->UpdateCamera(0.0f);
		PlayerController->PlayerCameraManager->SetGameCameraCutThisFrame();
	}
}

void ConfigurePreviewCamera(AActor* ViewTarget)
{
	if (!IsValid(ViewTarget))
	{
		return;
	}
	TArray<UCameraComponent*> CameraComponents;
	ViewTarget->GetComponents<UCameraComponent>(CameraComponents);
	for (UCameraComponent* CameraComponent : CameraComponents)
	{
		if (IsValid(CameraComponent))
		{
			CameraComponent->SetConstraintAspectRatio(false);
			CameraComponent->bOverrideAspectRatioAxisConstraint = false;
		}
	}
}
}

void UInfoCharacterPreviewController::Initialize(
	UInfoWidget* InOwnerWidget,
	const bool bInUsePreviewCamera,
	TSubclassOf<AActor> InPreviewClass)
{
	OwnerWidget = InOwnerWidget;
	bUsePreviewCamera = bInUsePreviewCamera;
	if (InPreviewClass)
	{
		PreviewClass = InPreviewClass;
	}
}

void UInfoCharacterPreviewController::ShowPreview()
{
	if (!OwnerWidget || !bUsePreviewCamera)
	{
		return;
	}
	APawn* OwningPawn = OwnerWidget->GetOwningPlayerPawn();
	USkeletalMeshComponent* MeshComponent = OwningPawn
		? OwningPawn->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;
	if (IsValid(SpawnedPreview))
	{
		if (!MeshComponent)
		{
			return;
		}
		ConfigurePreviewCamera(SpawnedPreview);
		CutToViewTarget(OwnerWidget->GetOwningPlayer(), SpawnedPreview);
		return;
	}

	ResolvePreviewClass();
	UWorld* World = OwnerWidget->GetWorld();
	if (!World || !PreviewClass || !MeshComponent)
	{
		return;
	}
	SpawnedPreview = World->SpawnActor<AActor>(PreviewClass, FTransform::Identity);
	if (!SpawnedPreview)
	{
		return;
	}
	SpawnedPreview->AttachToComponent(
		MeshComponent,
		FAttachmentTransformRules(
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepRelative,
			true));
	ConfigurePreviewCamera(SpawnedPreview);
	CutToViewTarget(OwnerWidget->GetOwningPlayer(), SpawnedPreview);
}

void UInfoCharacterPreviewController::ReturnCameraToPawn() const
{
	if (!OwnerWidget)
	{
		return;
	}
	APlayerController* PlayerController = OwnerWidget->GetOwningPlayer();
	APawn* OwningPawn = OwnerWidget->GetOwningPlayerPawn();
	if (PlayerController && OwningPawn)
	{
		CutToViewTarget(PlayerController, OwningPawn);
	}
}

void UInfoCharacterPreviewController::Shutdown(const bool bReturnCamera)
{
	if (bReturnCamera)
	{
		ReturnCameraToPawn();
	}
	if (IsValid(SpawnedPreview))
	{
		SpawnedPreview->Destroy();
	}
	SpawnedPreview = nullptr;
	OwnerWidget = nullptr;
}

void UInfoCharacterPreviewController::ResolvePreviewClass()
{
	if (PreviewClass || !OwnerWidget)
	{
		return;
	}
	if (const UWidgetClassDefinition* WidgetDefinition =
		UWidgetClassDefinition::ResolveWidgetClassDefinition(OwnerWidget))
	{
		PreviewClass = WidgetDefinition->GetCharacterPreviewClass();
	}
}
