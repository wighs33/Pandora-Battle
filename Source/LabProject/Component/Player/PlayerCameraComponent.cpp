#include "Component/Player/PlayerCameraComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayerCameraComponent)

UPlayerCameraComponent::UPlayerCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerCameraComponent::InitializeCamera(USpringArmComponent* InCameraBoom, UCameraComponent* InFollowCamera,
	const FPlayerCameraPresentationSettings& Settings)
{
	ResetOcclusionMaterialState();

	CameraBoom = InCameraBoom;
	FollowCamera = InFollowCamera;
	PresentationSettings = Settings;

	if (PresentationSettings.OcclusionSurfaceObjectTypes.IsEmpty())
	{
		PresentationSettings.OcclusionSurfaceObjectTypes = {
			ECC_WorldStatic,
			ECC_WorldDynamic
		};
	}

	CacheCameraDefaults();
}

void UPlayerCameraComponent::ShutdownCamera()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AbilityCameraOverrideTimerHandle);
	}

	ResetOcclusionMaterialState();
	ResetOcclusionTarget();
	bWeaponAimCameraActive = false;
	bAbilityCameraOverrideActive = false;
	if (bHasCachedDefaults && FollowCamera && CameraBoom)
	{
		FollowCamera->SetFieldOfView(DefaultCameraFOV);
		CameraBoom->SocketOffset = DefaultCameraBoomSocketOffset;
		FollowCamera->SetRelativeRotation(DefaultFollowCameraRelativeRotation);
	}
}

void UPlayerCameraComponent::TickPresentation(const float DeltaSeconds)
{
	UpdateAimCamera(DeltaSeconds);
	UpdateOcclusionMaterialState();
	UpdateOcclusionTarget();
}

void UPlayerCameraComponent::SetWeaponAimActive(const bool bEnabled, const FWeaponAimCameraSettings& Settings)
{
	if (bEnabled)
	{
		ActiveWeaponAimCameraSettings = SanitizeAimCameraSettings(Settings);
	}

	bWeaponAimCameraActive = bEnabled;
}

void UPlayerCameraComponent::SetAbilityCameraOverrideActive(const bool bEnabled,
	const FWeaponAimCameraSettings& Settings)
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	if (bEnabled)
	{
		ActiveAbilityCameraOverrideSettings = SanitizeAimCameraSettings(Settings);
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AbilityCameraOverrideTimerHandle);
	}

	bAbilityCameraOverrideActive = bEnabled;
}

void UPlayerCameraComponent::SetAbilityCameraOverrideActiveForDuration(const bool bEnabled,
	const FWeaponAimCameraSettings& Settings, const float Duration)
{
	SetAbilityCameraOverrideActive(bEnabled, Settings);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(AbilityCameraOverrideTimerHandle);
	if (bAbilityCameraOverrideActive && Duration > 0.0f)
	{
		World->GetTimerManager().SetTimer(AbilityCameraOverrideTimerHandle, this,
			&ThisClass::ClearAbilityCameraOverride, Duration, false);
	}
}

bool UPlayerCameraComponent::GetAimViewPoint(FVector& OutLocation, FVector& OutDirection) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!FollowCamera)
	{
		if (const AController* Controller = OwnerPawn ? OwnerPawn->GetController() : nullptr)
		{
			FRotator ViewRotation;
			Controller->GetPlayerViewPoint(OutLocation, ViewRotation);
			OutDirection = ViewRotation.Vector();
			return !OutDirection.IsNearlyZero();
		}

		if (!OwnerPawn)
		{
			OutLocation = FVector::ZeroVector;
			OutDirection = FVector::ForwardVector;
			return false;
		}

		OutLocation = OwnerPawn->GetActorLocation();
		OutDirection = OwnerPawn->GetActorForwardVector();
		return !OutDirection.IsNearlyZero();
	}

	OutLocation = FollowCamera->GetComponentLocation();
	OutDirection = FollowCamera->GetForwardVector();
	return !OutDirection.IsNearlyZero();
}

FWeaponAimCameraSettings UPlayerCameraComponent::SanitizeAimCameraSettings(FWeaponAimCameraSettings Settings)
{
	Settings.TargetFOV = FMath::Clamp(Settings.TargetFOV, 5.0f, 170.0f);
	Settings.TargetBoomSocketOffset = Settings.TargetBoomSocketOffset.ContainsNaN()
		? FVector::ZeroVector
		: Settings.TargetBoomSocketOffset.BoundToCube(1000.0);

	if (Settings.TargetCameraRotation.ContainsNaN())
	{
		Settings.TargetCameraRotation = FRotator::ZeroRotator;
	}
	else
	{
		Settings.TargetCameraRotation.Normalize();
		Settings.TargetCameraRotation.Pitch = FMath::Clamp(Settings.TargetCameraRotation.Pitch, -89.0f, 89.0f);
		Settings.TargetCameraRotation.Roll = FMath::Clamp(Settings.TargetCameraRotation.Roll, -89.0f, 89.0f);
	}

	Settings.InterpSpeed = FMath::Clamp(Settings.InterpSpeed, 0.0f, 100.0f);
	return Settings;
}

void UPlayerCameraComponent::CacheCameraDefaults()
{
	bHasCachedDefaults = FollowCamera && CameraBoom;
	if (!bHasCachedDefaults)
	{
		return;
	}

	DefaultCameraFOV = FollowCamera->FieldOfView;
	DefaultCameraBoomSocketOffset = CameraBoom->SocketOffset;
	DefaultFollowCameraRelativeRotation = FollowCamera->GetRelativeRotation();
}

void UPlayerCameraComponent::ClearAbilityCameraOverride()
{
	SetAbilityCameraOverrideActive(false, FWeaponAimCameraSettings());
}

void UPlayerCameraComponent::UpdateAimCamera(const float DeltaSeconds)
{
	if (!bHasCachedDefaults || !FollowCamera || !CameraBoom)
	{
		return;
	}

	const FWeaponAimCameraSettings* DesiredCameraSettings = nullptr;
	if (bAbilityCameraOverrideActive)
	{
		DesiredCameraSettings = &ActiveAbilityCameraOverrideSettings;
	}
	else if (bWeaponAimCameraActive)
	{
		DesiredCameraSettings = &ActiveWeaponAimCameraSettings;
	}

	const float TargetFOV = DesiredCameraSettings ? DesiredCameraSettings->TargetFOV : DefaultCameraFOV;
	const FVector TargetSocketOffset = DesiredCameraSettings
		? DesiredCameraSettings->TargetBoomSocketOffset
		: DefaultCameraBoomSocketOffset;
	const FRotator TargetCameraRotation = DesiredCameraSettings
		? DesiredCameraSettings->TargetCameraRotation
		: DefaultFollowCameraRelativeRotation;
	const float CameraTransitionSpeed = DesiredCameraSettings
		? DesiredCameraSettings->InterpSpeed
		: ActiveWeaponAimCameraSettings.InterpSpeed;

	if (CameraTransitionSpeed <= 0.0f)
	{
		FollowCamera->SetFieldOfView(TargetFOV);
		CameraBoom->SocketOffset = TargetSocketOffset;
		FollowCamera->SetRelativeRotation(TargetCameraRotation);
		return;
	}

	FollowCamera->SetFieldOfView(
		FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaSeconds, CameraTransitionSpeed));
	CameraBoom->SocketOffset =
		FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocketOffset, DeltaSeconds, CameraTransitionSpeed);
	FollowCamera->SetRelativeRotation(FMath::RInterpTo(FollowCamera->GetRelativeRotation(), TargetCameraRotation,
		DeltaSeconds, CameraTransitionSpeed));
}

void UPlayerCameraComponent::UpdateOcclusionMaterialState()
{
	if (!FollowCamera || PresentationSettings.OcclusionEnabledParameterName.IsNone())
	{
		return;
	}

	const float DisableDistance = FMath::Max(0.0f, PresentationSettings.OcclusionDisableDistance);
	const float ReenableDistance = FMath::Max(DisableDistance, PresentationSettings.OcclusionReenableDistance);

	TSet<TWeakObjectPtr<UMeshComponent>> NearMeshComponents;
	AppendOcclusionMeshComponents(DisableDistance, NearMeshComponents);

	TSet<TWeakObjectPtr<UMeshComponent>> RetainedMeshComponents = NearMeshComponents;
	if (!OcclusionDisabledMeshComponents.IsEmpty() && ReenableDistance > DisableDistance)
	{
		AppendOcclusionMeshComponents(ReenableDistance, RetainedMeshComponents);
	}

	TSet<TWeakObjectPtr<UMeshComponent>> NextDisabledMeshComponents;
	for (const TWeakObjectPtr<UMeshComponent>& MeshComponentPtr : NearMeshComponents)
	{
		UMeshComponent* MeshComponent = MeshComponentPtr.Get();
		if (MeshComponent && (OcclusionDisabledMeshComponents.Contains(MeshComponentPtr)
			|| SetOcclusionEnabledForMesh(MeshComponent, false)))
		{
			NextDisabledMeshComponents.Add(MeshComponentPtr);
		}
	}

	for (const TWeakObjectPtr<UMeshComponent>& MeshComponentPtr : OcclusionDisabledMeshComponents)
	{
		if (NextDisabledMeshComponents.Contains(MeshComponentPtr))
		{
			continue;
		}

		if (RetainedMeshComponents.Contains(MeshComponentPtr))
		{
			NextDisabledMeshComponents.Add(MeshComponentPtr);
		}
		else if (UMeshComponent* MeshComponent = MeshComponentPtr.Get())
		{
			SetOcclusionEnabledForMesh(MeshComponent, true);
		}
	}

	OcclusionDisabledMeshComponents = MoveTemp(NextDisabledMeshComponents);
}

void UPlayerCameraComponent::AppendOcclusionMeshComponents(const float ProbeRadius,
	TSet<TWeakObjectPtr<UMeshComponent>>& OutMeshComponents) const
{
	const UWorld* World = GetWorld();
	const AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor || !FollowCamera || ProbeRadius <= 0.0f)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	for (const TEnumAsByte<ECollisionChannel> ObjectType : PresentationSettings.OcclusionSurfaceObjectTypes)
	{
		ObjectQueryParams.AddObjectTypesToQuery(ObjectType);
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerCameraOcclusionSurfaceProximity), false, OwnerActor);
	QueryParams.AddIgnoredActor(OwnerActor);
	QueryParams.bTraceComplex = true;

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(OverlapResults, GetOcclusionViewLocation(), FQuat::Identity, ObjectQueryParams,
		FCollisionShape::MakeSphere(ProbeRadius), QueryParams);

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		if (UMeshComponent* OverlappedMesh = Cast<UMeshComponent>(OverlapResult.Component.Get()))
		{
			OutMeshComponents.Add(OverlappedMesh);
		}

		AActor* OverlappedActor = OverlapResult.GetActor();
		if (!OverlappedActor)
		{
			continue;
		}

		TInlineComponentArray<UMeshComponent*> ActorMeshComponents;
		OverlappedActor->GetComponents(ActorMeshComponents);
		for (UMeshComponent* MeshComponent : ActorMeshComponents)
		{
			if (MeshComponent)
			{
				OutMeshComponents.Add(MeshComponent);
			}
		}
	}
}

bool UPlayerCameraComponent::SetOcclusionEnabledForMesh(UMeshComponent* MeshComponent, const bool bEnabled)
{
	if (!MeshComponent || PresentationSettings.OcclusionEnabledParameterName.IsNone())
	{
		return false;
	}

	const TWeakObjectPtr<UMeshComponent> MeshComponentKey(MeshComponent);
	TArray<TWeakObjectPtr<UMaterialInstanceDynamic>>* DynamicMaterials =
		OcclusionMaterialInstances.Find(MeshComponentKey);

	if (!DynamicMaterials)
	{
		TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> NewDynamicMaterials;
		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		NewDynamicMaterials.Reserve(MaterialCount);

		const FMaterialParameterInfo ParameterInfo(PresentationSettings.OcclusionEnabledParameterName);
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);
			float ParameterValue = 0.0f;
			if (!Material || !Material->GetScalarParameterValue(ParameterInfo, ParameterValue))
			{
				continue;
			}

			UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);
			if (!DynamicMaterial)
			{
				DynamicMaterial = MeshComponent->CreateDynamicMaterialInstance(MaterialIndex, Material);
			}

			if (DynamicMaterial)
			{
				NewDynamicMaterials.Add(DynamicMaterial);
			}
		}

		DynamicMaterials = &OcclusionMaterialInstances.Add(MeshComponentKey, MoveTemp(NewDynamicMaterials));
	}

	bool bFoundOcclusionMaterial = false;
	for (const TWeakObjectPtr<UMaterialInstanceDynamic>& DynamicMaterialPtr : *DynamicMaterials)
	{
		if (UMaterialInstanceDynamic* DynamicMaterial = DynamicMaterialPtr.Get())
		{
			DynamicMaterial->SetScalarParameterValue(PresentationSettings.OcclusionEnabledParameterName,
				bEnabled ? 1.0f : 0.0f);
			bFoundOcclusionMaterial = true;
		}
	}

	return bFoundOcclusionMaterial;
}

void UPlayerCameraComponent::ResetOcclusionMaterialState()
{
	for (const TWeakObjectPtr<UMeshComponent>& MeshComponentPtr : OcclusionDisabledMeshComponents)
	{
		if (UMeshComponent* MeshComponent = MeshComponentPtr.Get())
		{
			SetOcclusionEnabledForMesh(MeshComponent, true);
		}
	}

	OcclusionDisabledMeshComponents.Reset();
	OcclusionMaterialInstances.Reset();
}

// 가림 처리는 플레이어 컨트롤러의 시점(카메라 매니저가 최종으로 정한 위치)을 기준으로 한다.
FVector UPlayerCameraComponent::GetOcclusionViewLocation() const
{
	FVector ViewLocation = FollowCamera ? FollowCamera->GetComponentLocation() : FVector::ZeroVector;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const APlayerController* PlayerController = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			FRotator ViewRotation;
			PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}
	return ViewLocation;
}

UMaterialParameterCollectionInstance* UPlayerCameraComponent::FindOcclusionCollectionInstance() const
{
	UWorld* World = GetWorld();
	return World && PresentationSettings.OcclusionParameterCollection
		? World->GetParameterCollectionInstance(PresentationSettings.OcclusionParameterCollection)
		: nullptr;
}

// 화면 가운데를 가리는 면은 플레이어를 가릴 수 있는 것만 지운다: 카메라와 플레이어 사이에 있고 발보다 높은 면.
// 내려다볼 때 조준선 둘레에 걸리는 바닥은 그 밖이라 남는다.
void UPlayerCameraComponent::UpdateOcclusionTarget()
{
	UMaterialParameterCollectionInstance* Collection = FindOcclusionCollectionInstance();
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* Capsule = OwnerCharacter ? OwnerCharacter->GetCapsuleComponent() : nullptr;
	if (!Collection || !Capsule || !FollowCamera)
	{
		return;
	}

	const FVector PlayerLocation = OwnerCharacter->GetActorLocation();
	Collection->SetScalarParameterValue(PresentationSettings.OcclusionDistanceParameterName,
		FVector::Dist(GetOcclusionViewLocation(), PlayerLocation));
	Collection->SetScalarParameterValue(PresentationSettings.OcclusionFloorHeightParameterName,
		PlayerLocation.Z - Capsule->GetScaledCapsuleHalfHeight() + PresentationSettings.OcclusionFloorClearance);
}

// 카메라를 내려놓을 때 컬렉션 값을 기본값(예전처럼 거리 · 높이 제한 없음)으로 되돌린다.
void UPlayerCameraComponent::ResetOcclusionTarget()
{
	UMaterialParameterCollectionInstance* Collection = FindOcclusionCollectionInstance();
	if (!Collection)
	{
		return;
	}

	for (const FName ParameterName : {PresentationSettings.OcclusionDistanceParameterName,
		PresentationSettings.OcclusionFloorHeightParameterName})
	{
		if (const FCollectionScalarParameter* Parameter =
			PresentationSettings.OcclusionParameterCollection->GetScalarParameterByName(ParameterName))
		{
			Collection->SetScalarParameterValue(Parameter->ParameterName, Parameter->DefaultValue);
		}
	}
}
