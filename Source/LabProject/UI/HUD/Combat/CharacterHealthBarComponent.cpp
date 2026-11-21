#include "UI/HUD/Combat/CharacterHealthBarComponent.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Character/CharacterBase.h"
#include "Character/EnemyBase.h"
#include "Common/PdGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "UI/Core/WidgetClassDefinition.h"
#include "GameFramework/PlayerController.h"
#include "UI/HUD/Combat/CombatUiPool.h"
#include "UI/HUD/Combat/EnemyAvatarWidget.h"
#include "UI/HUD/Combat/EnemyHealthBarWidget.h"
#include "UI/HUD/Combat/EnemyShieldBarWidget.h"
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
	ReleaseWidget();
	BindViewModelToASC(nullptr);
	HealthBarViewModel = nullptr;
}

// 엔진은 BeginPlay에서 위젯을 만든다. 이 컴포넌트는 막대가 처음 보일 때 풀에서 꺼내므로(AcquireWidget) 여기서는 만들지 않는다.
// 체력이 가득하면 막대를 숨기는 졸개는 맞기 전까지 위젯이 없다.
void UCharacterHealthBarComponent::InitWidget()
{
	if (IsRunningDedicatedServer())
	{
		Super::InitWidget();
	}
}

// ASC가 연결돼 있으면 값을 연결하고, 위젯이 이미 있으면 그 위젯에도 붙인다. ASC가 아직이면 연결 완료 시 AbilityStateComponent가 다시 호출한다.
void UCharacterHealthBarComponent::RefreshViewModel()
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

	BindViewModelToASC(Character->GetAbilitySystemComponent());
	TryApplyViewModelToWidget(GetWidget());
}

void UCharacterHealthBarComponent::AcquireWidget()
{
	UCombatUiPool* Pool = UWorld::GetSubsystem<UCombatUiPool>(GetWorld());
	if (GetWidget() || !GetWidgetClass() || !Pool)
	{
		return;
	}

	SetWidget(Pool->AcquireOverheadWidget(GetWidgetClass()));
	TryApplyViewModelToWidget(GetWidget());
}

// 위젯을 떼어 이 캐릭터를 더 보지 않게 한 뒤 풀에 돌려준다. 화면에서 빠지면서 위젯의 NativeDestruct가 구독과 타이머를 정리한다.
void UCharacterHealthBarComponent::ReleaseWidget()
{
	UUserWidget* PooledWidget = GetWidget();
	if (!PooledWidget)
	{
		return;
	}

	SetWidget(nullptr);
	ClearWidgetOwner(*PooledWidget);
	if (UCombatUiPool* Pool = UWorld::GetSubsystem<UCombatUiPool>(GetWorld()))
	{
		Pool->ReleaseOverheadWidget(*PooledWidget);
	}
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

	// 적은 위젯 정의의 아바타 위젯을 항상 쓰고, 그 밖의 캐릭터는 BP가 지정한 위젯이 없을 때만 정의의 체력바를 쓴다.
	const bool bEnemy = Character->IsA<AEnemyBase>();
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(Character);
	const TSubclassOf<UUserWidget> ResolvedWidgetClass = !WidgetDefinition
		? nullptr
		: bEnemy
			? WidgetDefinition->GetEnemyAvatarWidgetClass()
			: WidgetDefinition->GetHealthBarWidgetClass();
	if (!ResolvedWidgetClass)
	{
		return;
	}

	UClass* CurrentWidgetClass = GetWidgetClass();
	const bool bApplyResolvedWidgetClass = bEnemy
		? CurrentWidgetClass != ResolvedWidgetClass.Get()
		: !CurrentWidgetClass;
	if (bApplyResolvedWidgetClass)
	{
		// SetWidgetClass는 BeginPlay 뒤에 부르면 위젯을 바로 만든다. 위젯은 보일 때 꺼내므로 클래스만 바꾸고 지난 클래스의 위젯은 돌려준다.
		ReleaseWidget();
		WidgetClass = ResolvedWidgetClass;
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

// 풀로 돌아가는 위젯이 떠난 캐릭터의 ASC와 뷰 모델을 붙잡지 않게 한다. 다음 캐릭터가 꺼내면 TryApplyViewModelToWidget이 다시 붙인다.
void UCharacterHealthBarComponent::ClearWidgetOwner(UUserWidget& PooledWidget) const
{
	if (UEnemyAvatarWidget* EnemyAvatarWidget = Cast<UEnemyAvatarWidget>(&PooledWidget))
	{
		EnemyAvatarWidget->SetOwnerActor(nullptr);
		return;
	}

	if (UEnemyHealthBarWidget* EnemyHealthBarWidget = Cast<UEnemyHealthBarWidget>(&PooledWidget))
	{
		EnemyHealthBarWidget->SetOwnerActor(nullptr);
		return;
	}

	if (UEnemyShieldBarWidget* EnemyShieldBarWidget = Cast<UEnemyShieldBarWidget>(&PooledWidget))
	{
		EnemyShieldBarWidget->SetOwnerActor(nullptr);
		return;
	}

	if (UMVVMView* View = PooledWidget.GetExtension<UMVVMView>())
	{
		View->SetViewModel(UHealthBarViewModel::ViewModelName, nullptr);
	}
}

bool UCharacterHealthBarComponent::BindViewModelToASC(UAbilitySystemComponent* AbilitySystemComponent)
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

bool UCharacterHealthBarComponent::IsAttributeDataReady(const UAbilitySystemComponent* AbilitySystemComponent) const
{
	return AbilitySystemComponent && AbilitySystemComponent->IsRegistered()
		&& AbilitySystemComponent->GetAttributeSet(UBasicAttributeSet::StaticClass()) != nullptr;
}

void UCharacterHealthBarComponent::SetVisibleForLocalViewer(const bool bRequestedVisible)
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (!Character || Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	bool bShouldBeVisible = bRequestedVisible;
	if (bShouldBeVisible)
	{
		const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
		bShouldBeVisible = !Character->IsDeathHandled()
			&& !(ASC && ASC->HasMatchingGameplayTag(PdGameplayTags::State_Dead));
	}

	if (bShouldBeVisible)
	{
		AcquireWidget();
	}

	if (IsVisible() != bShouldBeVisible)
	{
		SetVisibility(bShouldBeVisible, true);
	}
}

void UCharacterHealthBarComponent::UpdateVisibilityForLocalViewer(APlayerController* LocalPlayerController,
	const FVector& CameraLocation, const FRotator& CameraRotation, const float MaxDistanceSquared)
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (!Character || Character->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const bool bIsLocalPlayerPawn = Character->IsPlayerControlled() && Character->IsLocallyControlled();
	if (bIsLocalPlayerPawn && !bShowLocalPlayerHealthBar)
	{
		SetVisibleForLocalViewer(false);
		return;
	}

	const UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (Character->IsDeathHandled() || (ASC && ASC->HasMatchingGameplayTag(PdGameplayTags::State_Dead)))
	{
		SetVisibleForLocalViewer(false);
		return;
	}

	if (!Character->ShouldShowHealthBarAtFullHealth() && ASC
		&& ASC->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) >= ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxHealthAttribute()))
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

	const FVector ViewerLocation = GetLineOfSightStartLocation(LocalPlayerController, CameraLocation);
	if (MaxDistanceSquared > 0.0f && FVector::DistSquared(ViewerLocation, GetVisibilityTargetLocation())
			> MaxDistanceSquared)
	{
		SetVisibleForLocalViewer(false);
		return;
	}

	UWorld* World = GetWorld();
	const double NowSeconds = World ? World->GetTimeSeconds() : 0.0;
	const bool bVisibilityTestPassed = ShouldShowForLocalViewer(LocalPlayerController, CameraLocation, CameraRotation);
	if (bVisibilityTestPassed)
	{
		LastVisibleTimeSeconds = NowSeconds;
	}

	const bool bWithinHideGraceTime = IsVisible() && HideGraceTime > 0.0f && NowSeconds - LastVisibleTimeSeconds
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
	const ACharacterBase* Character = GetCharacterOwner();
	if (!Character || Character->GetNetMode() == NM_DedicatedServer || !IsVisible()
		|| GetWidgetSpace() != EWidgetSpace::World)
	{
		return;
	}

	const APlayerController* LocalPlayerController = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	if (!LocalPlayerController)
	{
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	LocalPlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
	SetWorldRotation((CameraLocation - GetComponentLocation()).Rotation());
}

bool UCharacterHealthBarComponent::ShouldShowForLocalViewer(APlayerController* LocalPlayerController,
	const FVector& CameraLocation, const FRotator& CameraRotation) const
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (!Character || !LocalPlayerController || !LocalPlayerController->IsLocalController())
	{
		return false;
	}

	const USkeletalMeshComponent* CharacterMesh = Character->GetMesh();
	if (Character->IsHidden() || !CharacterMesh || !CharacterMesh->IsVisible())
	{
		return false;
	}

	const FVector TargetLocation = GetVisibilityTargetLocation();
	const FVector ViewerCharacterLocation = GetLineOfSightStartLocation(LocalPlayerController, CameraLocation);

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
	if (!LocalPlayerController->ProjectWorldLocationToScreen(TargetLocation, ScreenPosition, true))
	{
		return false;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	LocalPlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX > 0 && ViewportSizeY > 0 && (ScreenPosition.X < 0.0f || ScreenPosition.Y < 0.0f
		|| ScreenPosition.X > static_cast<float>(ViewportSizeX)
		|| ScreenPosition.Y > static_cast<float>(ViewportSizeY)))
	{
		return false;
	}

	return !bUseLineOfSightCheck || HasLineOfSight(ViewerCharacterLocation);
}

bool UCharacterHealthBarComponent::HasLineOfSight(const FVector& TraceStartLocation) const
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
	return !World->LineTraceSingleByObjectType(HitResult, TraceStartLocation, GetVisibilityTargetLocation(),
		ObjectQueryParams, QueryParams);
}

FVector UCharacterHealthBarComponent::GetLineOfSightStartLocation(const APlayerController* LocalPlayerController,
	const FVector& FallbackCameraLocation) const
{
	const APawn* LocalPawn = LocalPlayerController ? LocalPlayerController->GetPawn() : nullptr;
	return LocalPawn ? LocalPawn->GetActorLocation() : FallbackCameraLocation;
}

FVector UCharacterHealthBarComponent::GetVisibilityTargetLocation() const
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return GetComponentLocation();
	}

	const FVector VisibilityOffset(0.0f, 0.0f, VisibilityTargetZOffset);
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