#include "Component/Character/CharacterHealthBarComponent.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterBase.h"
#include "Common/LabGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "GameFramework/PlayerController.h"
#include "UI/Widget/EnemyAvatarWidget.h"
#include "UI/Widget/EnemyHealthBarWidget.h"
#include "UI/Widget/EnemyShieldBarWidget.h"
#include "View/MVVMView.h"
#include "ViewModel/HealthBarViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterHealthBarComponent)

namespace
{
	constexpr float HealthBarWorldScale = 2.0f;
	constexpr bool bHideWhenCharacterNotVisible = true;
	constexpr bool bUseLineOfSightCheck = true;
	constexpr bool bShowLocalPlayerHealthBar = false;
	constexpr float VisibilityTargetZOffset = 90.0f;
	constexpr float HideGraceTime = 0.35f;
	constexpr int32 ViewModelMaxRetryAttempts = 10;
	constexpr float ViewModelRetryInterval = 0.1f;
}

UCharacterHealthBarComponent::UCharacterHealthBarComponent()
{
	SetUsingAbsoluteRotation(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
	SetRelativeScale3D(FVector(HealthBarWorldScale));
	SetVisibility(false, true);
}

void UCharacterHealthBarComponent::InitializeHealthBar()
{
	ConfigureWidget();
	LastVisibleTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	RefreshViewModel();
	SetVisibleForLocalViewer(false);
}

void UCharacterHealthBarComponent::ShutdownHealthBar()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ViewModelRetryTimerHandle);
	}

	BindViewModelToASC(nullptr);
	HealthBarViewModel = nullptr;
	ViewModelRetryCount = 0;
}

void UCharacterHealthBarComponent::RefreshViewModel()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ViewModelRetryTimerHandle);
	}
	ViewModelRetryCount = 0;
	TryRefreshViewModel();
}

void UCharacterHealthBarComponent::TryRefreshViewModel()
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character || Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!HealthBarViewModel)
	{
		HealthBarViewModel = NewObject<UHealthBarViewModel>(this);
	}

	const bool bBoundToReadyASC = BindViewModelToASC(Character->GetAbilitySystemComponent());
	const bool bAppliedToWidget = TryApplyViewModelToWidget();
	if (bBoundToReadyASC && bAppliedToWidget)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ViewModelRetryTimerHandle);
		}
		ViewModelRetryCount = 0;
		return;
	}

	QueueViewModelRefreshRetry();
}

void UCharacterHealthBarComponent::ConfigureWidget()
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetWidgetSpace(EWidgetSpace::Screen);
	SetRelativeScale3D(FVector(HealthBarWorldScale));
	SetVisibility(false, true);

	const UWidgetClassDefinition* WidgetDefinition =
		UWidgetClassDefinition::ResolveWidgetClassDefinition(Character);
	const TSubclassOf<UUserWidget> ResolvedWidgetClass =
		Character->ResolveHealthBarWidgetClass(WidgetDefinition);
	if (!ResolvedWidgetClass)
	{
		return;
	}

	UClass* CurrentWidgetClass = GetWidgetClass();
	if (Character->ShouldApplyResolvedHealthBarWidgetClass(
		CurrentWidgetClass,
		ResolvedWidgetClass))
	{
		SetWidgetClass(ResolvedWidgetClass);
	}
}

bool UCharacterHealthBarComponent::TryApplyViewModelToWidget(UUserWidget* InWidget)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!InWidget || !Character)
	{
		return false;
	}

	if (UEnemyAvatarWidget* EnemyAvatarWidget = Cast<UEnemyAvatarWidget>(InWidget))
	{
		EnemyAvatarWidget->SetOwnerActor(Character);
		return true;
	}

	if (UEnemyHealthBarWidget* EnemyHealthBarWidget = Cast<UEnemyHealthBarWidget>(InWidget))
	{
		EnemyHealthBarWidget->SetOwnerActor(Character);
		return true;
	}

	if (UEnemyShieldBarWidget* EnemyShieldBarWidget = Cast<UEnemyShieldBarWidget>(InWidget))
	{
		EnemyShieldBarWidget->SetOwnerActor(Character);
		return true;
	}

	if (!HealthBarViewModel)
	{
		return false;
	}

	UMVVMView* View = InWidget->GetExtension<UMVVMView>();
	if (!View)
	{
		return false;
	}

	if (!View->SetViewModel(UHealthBarViewModel::ViewModelName, HealthBarViewModel))
	{
		return false;
	}

	if (HealthBarViewModel->IsViewModelInitialized())
	{
		HealthBarViewModel->UpdateAllData();
	}
	return HealthBarViewModel->IsViewModelInitialized();
}

bool UCharacterHealthBarComponent::TryApplyViewModelToWidget()
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character
		|| Character->GetNetMode() == NM_DedicatedServer
		|| !GetWidgetClass())
	{
		return false;
	}

	InitWidget();
	return TryApplyViewModelToWidget(GetUserWidgetObject());
}

bool UCharacterHealthBarComponent::BindViewModelToASC(
	UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!HealthBarViewModel)
	{
		return false;
	}

	if (!IsAttributeDataReady(AbilitySystemComponent))
	{
		if (HealthBarViewModel->IsViewModelInitialized())
		{
			HealthBarViewModel->UninitializeViewModel();
		}
		return false;
	}

	HealthBarViewModel->InitializeViewModel(AbilitySystemComponent);
	return HealthBarViewModel->IsViewModelInitialized();
}

bool UCharacterHealthBarComponent::IsAttributeDataReady(
	const UAbilitySystemComponent* AbilitySystemComponent) const
{
	return AbilitySystemComponent
		&& AbilitySystemComponent->IsRegistered()
		&& AbilitySystemComponent->GetAttributeSet(UBasicAttributeSet::StaticClass()) != nullptr;
}

void UCharacterHealthBarComponent::QueueViewModelRefreshRetry()
{
	ACharacterBase* Character = GetCharacterOwner();
	UWorld* World = GetWorld();
	if (!Character || Character->GetNetMode() == NM_DedicatedServer || !World)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(ViewModelRetryTimerHandle)
		|| ViewModelRetryCount >= ViewModelMaxRetryAttempts)
	{
		return;
	}

	++ViewModelRetryCount;
	World->GetTimerManager().SetTimer(
		ViewModelRetryTimerHandle,
		this,
		&ThisClass::RetryRefreshViewModel,
		ViewModelRetryInterval,
		false);
}

void UCharacterHealthBarComponent::RetryRefreshViewModel()
{
	ViewModelRetryTimerHandle.Invalidate();
	TryRefreshViewModel();
}

void UCharacterHealthBarComponent::SetVisibleForLocalViewer(
	const bool bRequestedVisible)
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character || Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	bool bShouldBeVisible = bRequestedVisible;
	if (bShouldBeVisible)
	{
		const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
		bShouldBeVisible = !Character->IsDeathHandled()
			&& !(ASC && ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead));
	}

	if (IsVisible() != bShouldBeVisible)
	{
		SetVisibility(bShouldBeVisible, true);
	}
}

void UCharacterHealthBarComponent::UpdateVisibilityForLocalViewer(
	APlayerController* LocalPlayerController,
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	const float MaxDistanceSquared)
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character || Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const bool bIsLocalPlayerPawn =
		Character->IsPlayerControlled() && Character->IsLocallyControlled();
	if (bIsLocalPlayerPawn && !bShowLocalPlayerHealthBar)
	{
		SetVisibleForLocalViewer(false);
		return;
	}

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (Character->IsDeathHandled()
		|| (ASC && ASC->HasMatchingGameplayTag(LabGameplayTags::State_Dead)))
	{
		SetVisibleForLocalViewer(false);
		return;
	}

	const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (Character->IsHidden() || !CharacterMesh || !CharacterMesh->IsVisible())
	{
		SetVisibleForLocalViewer(false);
		return;
	}

	const FVector ViewerLocation = GetLineOfSightStartLocation(
		LocalPlayerController,
		CameraLocation);
	if (MaxDistanceSquared > 0.0f
		&& FVector::DistSquared(ViewerLocation, GetVisibilityTargetLocation())
			> MaxDistanceSquared)
	{
		SetVisibleForLocalViewer(false);
		return;
	}

	UWorld* World = GetWorld();
	const double NowSeconds = World ? World->GetTimeSeconds() : 0.0;
	const bool bVisibilityTestPassed = ShouldShowForLocalViewer(
		LocalPlayerController,
		CameraLocation,
		CameraRotation);
	if (bVisibilityTestPassed)
	{
		LastVisibleTimeSeconds = NowSeconds;
	}

	const bool bWithinHideGraceTime = IsVisible()
		&& HideGraceTime > 0.0f
		&& NowSeconds - LastVisibleTimeSeconds
			<= static_cast<double>(HideGraceTime);
	const bool bShouldShow = bVisibilityTestPassed || bWithinHideGraceTime;
	SetVisibleForLocalViewer(bShouldShow);
	if (bShouldShow)
	{
		UpdateFacing();
	}
}

void UCharacterHealthBarComponent::UpdateFacing()
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character
		|| Character->GetNetMode() == NM_DedicatedServer
		|| !IsVisible()
		|| GetWidgetSpace() != EWidgetSpace::World)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* LocalPlayerController =
		World ? World->GetFirstPlayerController() : nullptr;
	if (!LocalPlayerController || !LocalPlayerController->IsLocalController())
	{
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	LocalPlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	SetWorldRotation((CameraLocation - GetComponentLocation()).Rotation());
}

bool UCharacterHealthBarComponent::ShouldShowForLocalViewer(
	APlayerController* LocalPlayerController,
	const FVector& CameraLocation,
	const FRotator& CameraRotation) const
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character
		|| !LocalPlayerController
		|| !LocalPlayerController->IsLocalController())
	{
		return false;
	}

	const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (Character->IsHidden() || !CharacterMesh || !CharacterMesh->IsVisible())
	{
		return false;
	}

	const FVector TargetLocation = GetVisibilityTargetLocation();
	const FVector ViewerCharacterLocation = GetLineOfSightStartLocation(
		LocalPlayerController,
		CameraLocation);

	if (!bHideWhenCharacterNotVisible)
	{
		return true;
	}

	const FVector ToTarget = TargetLocation - CameraLocation;
	if (FVector::DotProduct(CameraRotation.Vector(), ToTarget.GetSafeNormal()) <= 0.0f)
	{
		return false;
	}

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	if (!LocalPlayerController->ProjectWorldLocationToScreen(
		TargetLocation,
		ScreenPosition,
		true))
	{
		return false;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	LocalPlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX > 0
		&& ViewportSizeY > 0
		&& (ScreenPosition.X < 0.0f
			|| ScreenPosition.Y < 0.0f
			|| ScreenPosition.X > static_cast<float>(ViewportSizeX)
			|| ScreenPosition.Y > static_cast<float>(ViewportSizeY)))
	{
		return false;
	}

	return !bUseLineOfSightCheck
		|| HasLineOfSight(ViewerCharacterLocation);
}

bool UCharacterHealthBarComponent::HasLineOfSight(
	const FVector& TraceStartLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PdHealthBarVisibility), false);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FHitResult HitResult;
	return !World->LineTraceSingleByObjectType(
		HitResult,
		TraceStartLocation,
		GetVisibilityTargetLocation(),
		ObjectQueryParams,
		QueryParams);
}

FVector UCharacterHealthBarComponent::GetLineOfSightStartLocation(
	const APlayerController* LocalPlayerController,
	const FVector& FallbackCameraLocation) const
{
	const APawn* LocalPawn =
		LocalPlayerController ? LocalPlayerController->GetPawn() : nullptr;
	return LocalPawn ? LocalPawn->GetActorLocation() : FallbackCameraLocation;
}

FVector UCharacterHealthBarComponent::GetVisibilityTargetLocation() const
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character)
	{
		return GetComponentLocation();
	}

	const FVector VisibilityOffset(
		0.0f,
		0.0f,
		VisibilityTargetZOffset);
	if (const USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
	{
		return CharacterMesh->Bounds.Origin + VisibilityOffset;
	}
	return Character->GetActorLocation() + VisibilityOffset;
}

ACharacterBase* UCharacterHealthBarComponent::GetCharacterOwner() const
{
	return Cast<ACharacterBase>(GetOwner());
}

const ACharacterBase* UCharacterHealthBarComponent::GetCharacterOwnerConst() const
{
	return Cast<ACharacterBase>(GetOwner());
}
