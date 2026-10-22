#include "UI/Core/GameplayScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Engine/LocalPlayer.h"
#include "UI/Core/PdUIActionRouter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayScreen)

UGameplayScreen::UGameplayScreen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = true;
}

UGameplayScreen* UGameplayScreen::Create(APlayerController* OwningPlayer, FSimpleDelegate BackAction)
{
	UGameplayScreen* Screen = CreateWidget<UGameplayScreen>(OwningPlayer);
	if (Screen)
	{
		Screen->OnBack = MoveTemp(BackAction);
		// 그리는 것은 없어도 보이는 상태여야 CommonUI가 입력을 받을 화면으로 고른다. 클릭은 아래 HUD로 통과시킨다.
		UOverlay* Content = Screen->WidgetTree->ConstructWidget<UOverlay>();
		Content->SetVisibility(ESlateVisibility::HitTestInvisible);
		Screen->WidgetTree->RootWidget = Content;
	}
	return Screen;
}

TOptional<FUIInputConfig> UGameplayScreen::GetDesiredInputConfig() const
{
	const UPdUIActionRouter* Router = ULocalPlayer::GetSubsystem<UPdUIActionRouter>(GetOwningLocalPlayer());
	return Router ? Router->MakeDefaultInputConfig() : TOptional<FUIInputConfig>();
}

bool UGameplayScreen::NativeOnHandleBackAction()
{
	OnBack.ExecuteIfBound();
	return true;
}
