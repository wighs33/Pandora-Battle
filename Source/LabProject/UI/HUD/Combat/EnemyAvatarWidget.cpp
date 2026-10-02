#include "UI/HUD/Combat/EnemyAvatarWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Character/PdPlayer.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Definition/Online/AchievementDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Online/AchievementSubsystem.h"
#include "UI/HUD/Combat/EnemyHealthBarWidget.h"
#include "UI/HUD/Combat/EnemyShieldBarWidget.h"
#include "UI/HUD/Status/StatusEffectsBarWidget.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnemyAvatarWidget)

void UEnemyAvatarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	OriginalWidgetVisibilities.Reset();
	bLocalPlayerPresentationInitialized = false;
	if (!bHasDefaultAvatarBrush)
	{
		if (AvatarImage)
		{
			DefaultAvatarBrush = AvatarImage->GetBrush();
			bHasDefaultAvatarBrush = true;
		}
	}
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
	RestoreDefaultAvatarBrush(AvatarImage);
	bLocalPlayerPresentationInitialized = false;
	PropagateOwnerActorToChildren();
	ApplyLocalPlayerPresentation();
	RefreshAvatarImage();
}

void UEnemyAvatarWidget::PropagateOwnerActorToChildren()
{
	if (!WidgetTree)
	{
		return;
	}

	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (UEnemyHealthBarWidget* EnemyHealthBar = Cast<UEnemyHealthBarWidget>(Widget))
		{
			EnemyHealthBar->SetOwnerActor(OwnerActor.Get());
		}
		else if (UEnemyShieldBarWidget* EnemyShieldBar = Cast<UEnemyShieldBarWidget>(Widget))
		{
			EnemyShieldBar->SetOwnerActor(OwnerActor.Get());
		}
		else if (UStatusEffectsBarWidget* ChildStatusEffectsBar = Cast<UStatusEffectsBarWidget>(Widget))
		{
			ChildStatusEffectsBar->SetOwnerActor(OwnerActor.Get());
		}
	});
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
	UImage* TargetAvatarImage = AvatarImage;
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
		RestoreDefaultAvatarBrush(TargetAvatarImage);
		TargetAvatarImage->SetRenderOpacity(1.0f);
		TargetAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	UTexture2D* AchievementTexture = ResolvePlayerAchievementTexture();
	if (!AchievementTexture)
	{
		RestoreDefaultAvatarBrush(TargetAvatarImage);
		TargetAvatarImage->SetRenderOpacity(1.0f);
		TargetAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	TargetAvatarImage->SetBrushFromTexture(AchievementTexture, true);
	TargetAvatarImage->SetRenderOpacity(1.0f);
	TargetAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

bool UEnemyAvatarWidget::IsPlayerOwner() const
{
	return OwnerActor.Get() && OwnerActor->IsA<APdPlayer>();
}

void UEnemyAvatarWidget::RestoreDefaultAvatarBrush(UImage* TargetAvatarImage)
{
	if (TargetAvatarImage && bHasDefaultAvatarBrush)
	{
		TargetAvatarImage->SetBrush(DefaultAvatarBrush);
	}
}

bool UEnemyAvatarWidget::IsLocalPlayerOwner() const
{
	const APdPlayer* PlayerOwner = Cast<APdPlayer>(OwnerActor.Get());
	return PlayerOwner && PlayerOwner->IsLocallyControlled();
}

UTexture2D* UEnemyAvatarWidget::ResolvePlayerAchievementTexture() const
{
	const APdPlayer* PlayerOwner = Cast<APdPlayer>(OwnerActor.Get());
	const APdPlayerState* PlayerState =
		PlayerOwner ? PlayerOwner->GetPlayerState<APdPlayerState>() : nullptr;
	const UPlayerMatchComponent* PlayerMatchComponent =
		PlayerState ? PlayerState->GetPlayerMatchComponent() : nullptr;
	const FName AchievementId = PlayerMatchComponent
		? PlayerMatchComponent->GetSelectedAchievementId()
		: NAME_None;
	if (AchievementId.IsNone())
	{
		return nullptr;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UAchievementSubsystem* AchievementSubsystem = GameInstance
		? GameInstance->GetSubsystem<UAchievementSubsystem>()
		: nullptr;
	const UAchievementDefinition* AchievementDefinition = AchievementSubsystem
		? AchievementSubsystem->GetAchievementDefinition()
		: nullptr;
	if (!AchievementDefinition)
	{
		return nullptr;
	}

	for (const FAchievementEntry& Achievement : AchievementDefinition->Achievements)
	{
		FString CanonicalId = Achievement.AchievementId;
		CanonicalId.TrimStartAndEndInline();
		if (Achievement.bEnabled
			&& !CanonicalId.IsEmpty()
			&& FName(*CanonicalId) == AchievementId)
		{
			return Achievement.UnlockedIcon.Get();
		}
	}

	return nullptr;
}
