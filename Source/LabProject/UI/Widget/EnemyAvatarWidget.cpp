#include "UI/Widget/EnemyAvatarWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Character/PdPlayer.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/PdHUD.h"
#include "UI/Widget/EnemyHealthBarWidget.h"
#include "UI/Widget/EnemyShieldBarWidget.h"
#include "UI/Widget/InfoWidget.h"
#include "UI/Widget/StatusEffectsBarWidget.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyAvatarWidget)

namespace
{
	void SetObjectPropertyValue(UObject* Object, const FName PropertyName, UObject* Value)
	{
		if (!IsValid(Object))
		{
			return;
		}

		if (FObjectPropertyBase* ObjectProperty = FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName))
		{
			ObjectProperty->SetObjectPropertyValue_InContainer(Object, Value);
		}
	}
}

void UEnemyAvatarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OriginalWidgetVisibilities.Reset();
	bLocalPlayerPresentationInitialized = false;
	PropagateOwnerActorToChildren();
	ApplyLocalPlayerPresentation();
	HandleAvatarUpdateTick();
	StartAvatarUpdateTimer();
}

void UEnemyAvatarWidget::NativeDestruct()
{
	StopAvatarUpdateTimer();
	RestoreOriginalWidgetVisibilities();
	Super::NativeDestruct();
}

void UEnemyAvatarWidget::StartAvatarUpdateTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AvatarUpdateTimerHandle,
		this,
		&ThisClass::HandleAvatarUpdateTick,
		FMath::Max(AvatarUpdateInterval, 0.02f),
		true);
}

void UEnemyAvatarWidget::StopAvatarUpdateTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AvatarUpdateTimerHandle);
	}
	AvatarUpdateTimerHandle.Invalidate();
}

void UEnemyAvatarWidget::HandleAvatarUpdateTick()
{
	ApplyLocalPlayerPresentation();
	RefreshAvatarImage();
}

void UEnemyAvatarWidget::SetOwnerActor(AActor* InOwnerActor)
{
	if (OwnerActor.Get() == InOwnerActor)
	{
		PropagateOwnerActorToChildren();
		ApplyLocalPlayerPresentation();
		RefreshAvatarImage();
		return;
	}

	OwnerActor = InOwnerActor;
	CachedAchievementSourceImage.Reset();
	bLocalPlayerPresentationInitialized = false;
	PropagateOwnerActorToChildren();
	ApplyLocalPlayerPresentation();
	RefreshAvatarImage();
}

void UEnemyAvatarWidget::PropagateOwnerActorToChildren()
{
	bool bPropagatedHealthBar = false;
	bool bPropagatedShieldBar = false;
	bool bPropagatedStatusEffectsBar = false;

	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this, &bPropagatedHealthBar, &bPropagatedShieldBar, &bPropagatedStatusEffectsBar](UWidget* Widget)
		{
			if (!Widget)
			{
				return;
			}

			if (UEnemyHealthBarWidget* EnemyHealthBar = Cast<UEnemyHealthBarWidget>(Widget))
			{
				EnemyHealthBar->SetOwnerActor(OwnerActor.Get());
				bPropagatedHealthBar = true;
				return;
			}

			if (UEnemyShieldBarWidget* EnemyShieldBar = Cast<UEnemyShieldBarWidget>(Widget))
			{
				EnemyShieldBar->SetOwnerActor(OwnerActor.Get());
				bPropagatedShieldBar = true;
				return;
			}

			if (UStatusEffectsBarWidget* StatusEffectsBar = Cast<UStatusEffectsBarWidget>(Widget))
			{
				StatusEffectsBar->SetOwnerActor(OwnerActor.Get());
				bPropagatedStatusEffectsBar = true;
				return;
			}

			if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
			{
				SetObjectPropertyValue(UserWidget, TEXT("OwnerActor"), OwnerActor.Get());
			}
		});
	}

	if (!bPropagatedHealthBar)
	{
		if (UUserWidget* HealthBarWidget = FindFirstChildUserWidget({
			TEXT("EnemyHealthBar"),
			TEXT("W_EnemyHealthBar"),
			TEXT("WBP_EnemyHealthBar")
		}))
		{
			if (UEnemyHealthBarWidget* EnemyHealthBar = Cast<UEnemyHealthBarWidget>(HealthBarWidget))
			{
				EnemyHealthBar->SetOwnerActor(OwnerActor.Get());
			}
			else
			{
				SetObjectPropertyValue(HealthBarWidget, TEXT("OwnerActor"), OwnerActor.Get());
			}
		}
	}

	if (!bPropagatedShieldBar)
	{
		if (UUserWidget* ShieldBarWidget = FindFirstChildUserWidget({
			TEXT("EnemyShieldBar"),
			TEXT("W_EnemyShieldBar"),
			TEXT("WBP_EnemyShieldBar")
		}))
		{
			if (UEnemyShieldBarWidget* EnemyShieldBar = Cast<UEnemyShieldBarWidget>(ShieldBarWidget))
			{
				EnemyShieldBar->SetOwnerActor(OwnerActor.Get());
			}
			else
			{
				SetObjectPropertyValue(ShieldBarWidget, TEXT("OwnerActor"), OwnerActor.Get());
			}
		}
	}

	if (!bPropagatedStatusEffectsBar)
	{
		if (UUserWidget* ResolvedStatusEffectsBar = FindFirstChildUserWidget({
			TEXT("StatusEffectsBar"),
			TEXT("W_StatusEffectsBar"),
			TEXT("WBP_StatusEffectsBar")
		}))
		{
			if (UStatusEffectsBarWidget* NativeStatusEffectsBar = Cast<UStatusEffectsBarWidget>(ResolvedStatusEffectsBar))
			{
				NativeStatusEffectsBar->SetOwnerActor(OwnerActor.Get());
			}
			else
			{
				SetObjectPropertyValue(ResolvedStatusEffectsBar, TEXT("OwnerActor"), OwnerActor.Get());
			}
		}
	}
}

void UEnemyAvatarWidget::ApplyLocalPlayerPresentation()
{
	const bool bIsLocalPlayerOwner = IsLocalPlayerOwner();
	if (bLocalPlayerPresentationInitialized
		&& bLastLocalPlayerOwner == bIsLocalPlayerOwner)
	{
		return;
	}

	bLocalPlayerPresentationInitialized = true;
	bLastLocalPlayerOwner = bIsLocalPlayerOwner;

	if (!WidgetTree || !StatusEffectsBar)
	{
		return;
	}

	if (OriginalWidgetVisibilities.IsEmpty())
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			if (Widget)
			{
				OriginalWidgetVisibilities.Add(Widget, Widget->GetVisibility());
			}
		});
	}

	TSet<const UWidget*> StatusEffectsPath;
	for (const UWidget* Widget = StatusEffectsBar.Get(); Widget; Widget = Widget->GetParent())
	{
		StatusEffectsPath.Add(Widget);
	}

	WidgetTree->ForEachWidget([this, bIsLocalPlayerOwner, &StatusEffectsPath](UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}

		if (bIsLocalPlayerOwner && !StatusEffectsPath.Contains(Widget))
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}

		if (const ESlateVisibility* OriginalVisibility = OriginalWidgetVisibilities.Find(Widget))
		{
			Widget->SetVisibility(*OriginalVisibility);
		}
	});

	if (bIsLocalPlayerOwner)
	{
		StatusEffectsBar->CenterHorizontalBox();
	}
}

void UEnemyAvatarWidget::RestoreOriginalWidgetVisibilities()
{
	for (const TPair<TWeakObjectPtr<UWidget>, ESlateVisibility>& Pair : OriginalWidgetVisibilities)
	{
		if (UWidget* Widget = Pair.Key.Get())
		{
			Widget->SetVisibility(Pair.Value);
		}
	}

	OriginalWidgetVisibilities.Reset();
	bLocalPlayerPresentationInitialized = false;
}

void UEnemyAvatarWidget::RefreshAvatarImage()
{
	UImage* TargetAvatarImage = ResolveAvatarImage();
	if (!TargetAvatarImage)
	{
		return;
	}

	if (IsLocalPlayerOwner())
	{
		TargetAvatarImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (!IsPlayerOwner())
	{
		TargetAvatarImage->SetRenderOpacity(1.0f);
		TargetAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	FSlateBrush AchievementBrush;
	if (!FindPlayerAchievementBrush(AchievementBrush))
	{
		TargetAvatarImage->SetRenderOpacity(1.0f);
		TargetAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	TargetAvatarImage->SetBrush(AchievementBrush);
	TargetAvatarImage->SetRenderOpacity(1.0f);
	TargetAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

bool UEnemyAvatarWidget::IsPlayerOwner() const
{
	return OwnerActor.Get() && OwnerActor->IsA<APdPlayer>();
}

bool UEnemyAvatarWidget::IsLocalPlayerOwner() const
{
	const APdPlayer* PlayerOwner = Cast<APdPlayer>(OwnerActor.Get());
	return PlayerOwner && PlayerOwner->IsLocallyControlled();
}

bool UEnemyAvatarWidget::FindPlayerAchievementBrush(FSlateBrush& OutBrush) const
{
	if (UImage* SourceImage = ResolveAchievementSourceImage())
	{
		OutBrush = SourceImage->GetBrush();
		return true;
	}

	return false;
}

UImage* UEnemyAvatarWidget::ResolveAvatarImage() const
{
	if (CachedAvatarImage)
	{
		return CachedAvatarImage.Get();
	}

	if (AvatarImage)
	{
		const_cast<UEnemyAvatarWidget*>(this)->CachedAvatarImage = AvatarImage;
		return AvatarImage.Get();
	}

	UImage* ResolvedImage = FindImageInUserWidget(const_cast<UEnemyAvatarWidget*>(this), TEXT("AvatarImage"));
	const_cast<UEnemyAvatarWidget*>(this)->CachedAvatarImage = ResolvedImage;
	return ResolvedImage;
}

UImage* UEnemyAvatarWidget::ResolveAchievementSourceImage() const
{
	if (CachedAchievementSourceImage.IsValid())
	{
		return CachedAchievementSourceImage.Get();
	}

	const APlayerController* PlayerController = GetOwningPlayer()
		? GetOwningPlayer()
		: UGameplayStatics::GetPlayerController(this, 0);
	const APdHUD* HUD = PlayerController ? Cast<APdHUD>(PlayerController->GetHUD()) : nullptr;
	if (!HUD)
	{
		return nullptr;
	}

	if (UUserWidget* InfoWidget = HUD->GetInfoWidget())
	{
		if (UImage* PlayerAchieveIcon = FindImageInUserWidget(InfoWidget, TEXT("PlayerAchieveIcon")))
		{
			const_cast<UEnemyAvatarWidget*>(this)->CachedAchievementSourceImage = PlayerAchieveIcon;
			return PlayerAchieveIcon;
		}
	}

	if (UUserWidget* PlayerHudWidget = HUD->GetPlayerHudWidget())
	{
		if (UImage* PlayerAvatarImage = FindImageInUserWidget(PlayerHudWidget, TEXT("PlayerAvatar")))
		{
			const_cast<UEnemyAvatarWidget*>(this)->CachedAchievementSourceImage = PlayerAvatarImage;
			return PlayerAvatarImage;
		}
	}

	return nullptr;
}

UUserWidget* UEnemyAvatarWidget::FindChildUserWidget(const FName WidgetName) const
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	return Cast<UUserWidget>(WidgetTree->FindWidget(WidgetName));
}

UUserWidget* UEnemyAvatarWidget::FindFirstChildUserWidget(std::initializer_list<FName> WidgetNames) const
{
	for (const FName WidgetName : WidgetNames)
	{
		if (UUserWidget* FoundWidget = FindChildUserWidget(WidgetName))
		{
			return FoundWidget;
		}
	}

	return nullptr;
}

UImage* UEnemyAvatarWidget::FindImageInUserWidget(UUserWidget* RootWidget, const FName ImageName) const
{
	if (!RootWidget || !RootWidget->WidgetTree)
	{
		return nullptr;
	}

	if (UImage* FoundImage = Cast<UImage>(RootWidget->WidgetTree->FindWidget(ImageName)))
	{
		return FoundImage;
	}

	return FindImageInWidget(RootWidget->WidgetTree->RootWidget, ImageName);
}

UImage* UEnemyAvatarWidget::FindImageInWidget(UWidget* RootWidget, const FName ImageName) const
{
	if (!RootWidget)
	{
		return nullptr;
	}

	if (RootWidget->GetFName() == ImageName)
	{
		if (UImage* Image = Cast<UImage>(RootWidget))
		{
			return Image;
		}
	}

	if (UUserWidget* ChildUserWidget = Cast<UUserWidget>(RootWidget))
	{
		if (UImage* FoundImage = FindImageInUserWidget(ChildUserWidget, ImageName))
		{
			return FoundImage;
		}
	}

	if (const UPanelWidget* PanelWidget = Cast<UPanelWidget>(RootWidget))
	{
		const int32 ChildrenCount = PanelWidget->GetChildrenCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildrenCount; ++ChildIndex)
		{
			if (UImage* FoundImage = FindImageInWidget(PanelWidget->GetChildAt(ChildIndex), ImageName))
			{
				return FoundImage;
			}
		}
	}

	if (const UContentWidget* ContentWidget = Cast<UContentWidget>(RootWidget))
	{
		if (UImage* FoundImage = FindImageInWidget(ContentWidget->GetContent(), ImageName))
		{
			return FoundImage;
		}
	}

	return nullptr;
}
