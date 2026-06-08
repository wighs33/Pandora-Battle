#include "UI/Widget/LeftProfileWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdHUD.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LeftProfileWidget)

DEFINE_LOG_CATEGORY_STATIC(LogLeftProfileWidget, Log, All);

void ULeftProfileWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindAchievementButtons();
}

void ULeftProfileWidget::NativeDestruct()
{
	UnbindAchievementButtons();
	Super::NativeDestruct();
}

void ULeftProfileWidget::BindAchievementButtons()
{
	if (AchievementButton)
	{
		AchievementButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked);
	}
	if (AchievementButton_1)
	{
		AchievementButton_1->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_1);
	}
	if (AchievementButton_2)
	{
		AchievementButton_2->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_2);
	}
	if (AchievementButton_3)
	{
		AchievementButton_3->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_3);
	}
	if (AchievementButton_4)
	{
		AchievementButton_4->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_4);
	}
	if (AchievementButton_5)
	{
		AchievementButton_5->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAchievementButtonClicked_5);
	}
}

void ULeftProfileWidget::UnbindAchievementButtons()
{
	if (AchievementButton)
	{
		AchievementButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked);
	}
	if (AchievementButton_1)
	{
		AchievementButton_1->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_1);
	}
	if (AchievementButton_2)
	{
		AchievementButton_2->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_2);
	}
	if (AchievementButton_3)
	{
		AchievementButton_3->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_3);
	}
	if (AchievementButton_4)
	{
		AchievementButton_4->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_4);
	}
	if (AchievementButton_5)
	{
		AchievementButton_5->OnClicked.RemoveDynamic(this, &ThisClass::HandleAchievementButtonClicked_5);
	}
}

void ULeftProfileWidget::HandleAchievementButtonClicked()
{
	ApplyAchievementIcon(0);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_1()
{
	ApplyAchievementIcon(1);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_2()
{
	ApplyAchievementIcon(2);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_3()
{
	ApplyAchievementIcon(3);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_4()
{
	ApplyAchievementIcon(4);
}

void ULeftProfileWidget::HandleAchievementButtonClicked_5()
{
	ApplyAchievementIcon(5);
}

void ULeftProfileWidget::ApplyAchievementIcon(const int32 AchievementIndex)
{
	UImage* SourceImage = GetAchievementImage(AchievementIndex);
	if (!SourceImage)
	{
		UE_LOG(LogLeftProfileWidget, Warning, TEXT("[AchievementIcon] Missing source image. widget=%s index=%d"),
			*GetNameSafe(this),
			AchievementIndex);
		return;
	}

	const FSlateBrush AchievementBrush = SourceImage->GetBrush();
	if (PlayerAchieveIcon)
	{
		PlayerAchieveIcon->SetBrush(AchievementBrush);
		PlayerAchieveIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		UE_LOG(LogLeftProfileWidget, Warning, TEXT("[AchievementIcon] PlayerAchieveIcon is not bound. widget=%s index=%d"),
			*GetNameSafe(this),
			AchievementIndex);
	}

	if (UImage* PlayerAvatarImage = FindHudPlayerAvatarImage())
	{
		PlayerAvatarImage->SetBrush(AchievementBrush);
		PlayerAvatarImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UE_LOG(LogLeftProfileWidget, Log, TEXT("[AchievementIcon] Applied achievement icon to profile and HUD. widget=%s index=%d source=%s hudAvatar=%s"),
			*GetNameSafe(this),
			AchievementIndex,
			*GetNameSafe(SourceImage),
			*GetNameSafe(PlayerAvatarImage));
	}
	else
	{
		UE_LOG(LogLeftProfileWidget, Warning, TEXT("[AchievementIcon] HUD PlayerAvatar image was not found. widget=%s index=%d"),
			*GetNameSafe(this),
			AchievementIndex);
	}
}

UButton* ULeftProfileWidget::GetAchievementButton(const int32 AchievementIndex) const
{
	switch (AchievementIndex)
	{
	case 0:
		return AchievementButton;
	case 1:
		return AchievementButton_1;
	case 2:
		return AchievementButton_2;
	case 3:
		return AchievementButton_3;
	case 4:
		return AchievementButton_4;
	case 5:
		return AchievementButton_5;
	default:
		return nullptr;
	}
}

UImage* ULeftProfileWidget::GetAchievementImage(const int32 AchievementIndex) const
{
	switch (AchievementIndex)
	{
	case 0:
		return AchievementImage;
	case 1:
		return AchievementImage_1;
	case 2:
		return AchievementImage_2;
	case 3:
		return AchievementImage_3;
	case 4:
		return AchievementImage_4;
	case 5:
		return AchievementImage_5;
	default:
		return nullptr;
	}
}

UImage* ULeftProfileWidget::FindHudPlayerAvatarImage() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APdHUD* HUD = PlayerController ? Cast<APdHUD>(PlayerController->GetHUD()) : nullptr;
	UUserWidget* PlayerHudWidget = HUD ? HUD->GetPlayerHudWidget() : nullptr;
	return FindImageInUserWidget(PlayerHudWidget, TEXT("PlayerAvatar"));
}

UImage* ULeftProfileWidget::FindImageInUserWidget(UUserWidget* RootWidget, const FName ImageName) const
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

UImage* ULeftProfileWidget::FindImageInWidget(UWidget* RootWidget, const FName ImageName) const
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
