#include "UI/Widget/PandoraDescriptionWidget.h"

#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Mode/PdGameInstance.h"
#include "Mode/PdPlayerState.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "UI/Widget/SkillEffectIconResolver.h"
#include "UI/Widget/PandoraWidgetViewData.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "ViewModel/PandoraDescriptionViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraDescriptionWidget)

namespace
{
	UPandoraTreeComponent* ResolvePandoraTreeComponentFromDescriptionWidget(const UUserWidget* Widget)
	{
		if (!Widget)
		{
			return nullptr;
		}

		if (APlayerController* OwningPlayer = Widget->GetOwningPlayer())
		{
			if (APdPlayerState* PlayerState = OwningPlayer->GetPlayerState<APdPlayerState>())
			{
				return PlayerState->GetPandoraTreeComponent();
			}
		}

		if (APawn* OwningPawn = Widget->GetOwningPlayerPawn())
		{
			if (APdPlayerState* PlayerState = OwningPawn->GetPlayerState<APdPlayerState>())
			{
				return PlayerState->GetPandoraTreeComponent();
			}
		}

		return nullptr;
	}
}
void UPandoraDescriptionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	GetOrCreatePandoraDescriptionViewModel();
	ApplyPandoraDescriptionViewModelToMvvmView();
	ResolvePandoraTreeComponent();
	SetDetails();
}

void UPandoraDescriptionWidget::NativeDestruct()
{
	if (PandoraDescriptionViewModel && PandoraDescriptionViewModel->IsViewModelInitialized())
	{
		PandoraDescriptionViewModel->UninitializeViewModel();
	}

	Super::NativeDestruct();
}

void UPandoraDescriptionWidget::SetPandoraDefinition(UPandoraDefinition* InPandoraDefinition)
{
	PandoraDefinition = InPandoraDefinition;

	SetDetails();
}

void UPandoraDescriptionWidget::SetPandoraTreeComponent(UPandoraTreeComponent* InPandoraTreeComponent)
{
	PandoraTreeComponent = InPandoraTreeComponent;
	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = PandoraTreeComponent->GetPandoraDefinition();
	}

	SetDetails();
}

void UPandoraDescriptionWidget::SetDetails()
{
	ApplyEffectIconResources();

	UPandoraDescriptionViewModel* ViewModel = GetOrCreatePandoraDescriptionViewModel();
	if (!ViewModel)
	{
		return;
	}

	ViewModel->SetWeaponRequirementTextColor(WeaponRequirementTextColor);

	FPandoraDescriptionViewData ViewData = FPandoraDescriptionViewDataBuilder::Build(
		PandoraDefinition.Get(),
		PandoraTreeComponent.Get());
	const bool bUnlockedInSave = IsPandoraUnlockedInSave();
	if (PandoraDefinition && !bUnlockedInSave)
	{
		ViewData.DescriptionText = NSLOCTEXT("PandoraDescriptionWidget", "UnownedPandoraDescription", "You do not own this Pandora.");
		ViewData.WeaponRequirementVisibility = ESlateVisibility::Collapsed;
		ViewData.CurrentLevelVisibility = ESlateVisibility::Collapsed;
		ViewData.NextLevelVisibility = ESlateVisibility::Collapsed;
		ViewData.PointsRequiredVisibility = ESlateVisibility::Collapsed;
		ViewData.SkillSectionVisibility = ESlateVisibility::Collapsed;
	}

	ViewModel->SetTitleText(ViewData.TitleText);
	ViewModel->SetDescriptionText(ViewData.DescriptionText);
	ViewModel->SetWeaponRequirementText(ViewData.WeaponRequirementText);
	ViewModel->SetWeaponRequirementVisibility(ViewData.WeaponRequirementVisibility);
	ViewModel->SetCurrentLevelVisibility(ViewData.CurrentLevelVisibility);
	ViewModel->SetCurrentLevelTitleText(ViewData.CurrentLevelTitleText);
	ViewModel->SetCurrentLevelDescriptionText(ViewData.CurrentLevelDescriptionText);
	ViewModel->SetNextLevelVisibility(ViewData.NextLevelVisibility);
	ViewModel->SetNextLevelTitleText(ViewData.NextLevelTitleText);
	ViewModel->SetNextLevelDescriptionText(ViewData.NextLevelDescriptionText);
	ViewModel->SetPointsRequiredVisibility(ViewData.PointsRequiredVisibility);
	ViewModel->SetPointsRequiredText(ViewData.PointsRequiredText);
	ViewModel->SetSkillSectionVisibility(ViewData.SkillSectionVisibility);

	for (int32 SkillSlotIndex = 0; SkillSlotIndex < 4; ++SkillSlotIndex)
	{
		const FPandoraSkillSlotViewData* SkillViewData = ViewData.SkillSlots.IsValidIndex(SkillSlotIndex)
			? &ViewData.SkillSlots[SkillSlotIndex]
			: nullptr;

		ViewModel->SetSkillSlot(
			SkillSlotIndex,
			SkillViewData ? SkillViewData->IconResource : nullptr,
			SkillViewData ? SkillViewData->DisplayName : FText::GetEmpty(),
			SkillViewData ? SkillViewData->Description : FText::GetEmpty());
	}

	if (!ViewData.bHasPandoraDefinition)
	{

		return;
	}

	if (ViewData.bLockedByPandoraRequirement)
	{

		return;
	}

	if (PandoraDefinition && !bUnlockedInSave)
	{

		return;
	}

}

void UPandoraDescriptionWidget::PlayShowAnimation()
{
	const APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer || !OwningPlayer->IsLocalController() || !ScaleUp)
	{
		return;
	}

	StopAnimation(ScaleUp);
	PlayAnimation(ScaleUp, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
}

void UPandoraDescriptionWidget::ShowWithoutAnimation()
{
	const APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer || !OwningPlayer->IsLocalController() || !ScaleUp)
	{
		return;
	}

	// Apply the animation's completed state immediately and stop any transition already in progress.
	StopAnimation(ScaleUp);
	PlayAnimation(ScaleUp, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	SetAnimationCurrentTime(ScaleUp, ScaleUp->GetEndTime());
	PauseAnimation(ScaleUp);
}

void UPandoraDescriptionWidget::ResolvePandoraTreeComponent()
{
	if (!PandoraTreeComponent)
	{
		PandoraTreeComponent = ResolvePandoraTreeComponentFromDescriptionWidget(this);
	}

	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = PandoraTreeComponent->GetPandoraDefinition();
	}

}

void UPandoraDescriptionWidget::ApplyEffectIconResources()
{
	const TArray<UImage*> EffectIconResources =
	{
		EffectIconResource1.Get(),
		EffectIconResource2.Get(),
		EffectIconResource3.Get()
	};

	for (int32 SkillIndex = 0; SkillIndex < EffectIconResources.Num(); ++SkillIndex)
	{
		const FSkill* Skill = PandoraDefinition && PandoraDefinition->Skill.IsValidIndex(SkillIndex)
			? &PandoraDefinition->Skill[SkillIndex]
			: nullptr;
		PdSkillEffectIconResolver::ApplySkillEffectIcon(
			this,
			Skill,
			EffectIconResources[SkillIndex],
			ESkillEffectIconSet::PandoraDescription);
	}
}

UPandoraDescriptionViewModel* UPandoraDescriptionWidget::GetOrCreatePandoraDescriptionViewModel()
{
	if (!PandoraDescriptionViewModel)
	{
		PandoraDescriptionViewModel = NewObject<UPandoraDescriptionViewModel>(this);
	}

	if (PandoraDescriptionViewModel && !PandoraDescriptionViewModel->IsViewModelInitialized())
	{
		PandoraDescriptionViewModel->InitializeViewModel(this);
	}

	return PandoraDescriptionViewModel.Get();
}

void UPandoraDescriptionWidget::ApplyPandoraDescriptionViewModelToMvvmView()
{
	if (!PandoraDescriptionViewModel)
	{
		return;
	}

	UMVVMView* ViewExtension = GetExtension<UMVVMView>();
	if (!ViewExtension)
	{
		return;
	}

	const UMVVMViewClass* ViewClass = ViewExtension->GetViewClass();
	if (!ViewClass)
	{
		return;
	}

	FName RuntimeViewModelName = NAME_None;
	for (const FMVVMViewClass_Source& Source : ViewClass->GetSources())
	{
		if (!Source.IsViewModel() || !Source.CanBeSet())
		{
			continue;
		}

		const UClass* SourceClass = Source.GetSourceClass();
		if (SourceClass && PandoraDescriptionViewModel->GetClass()->IsChildOf(SourceClass))
		{
			RuntimeViewModelName = Source.GetName();
			break;
		}
	}

	if (RuntimeViewModelName.IsNone())
	{
		return;
	}

	ViewExtension->SetViewModel(RuntimeViewModelName, PandoraDescriptionViewModel);
}

bool UPandoraDescriptionWidget::IsPandoraUnlockedInSave() const
{
	if (!PandoraDefinition)
	{
		return false;
	}

	const bool bUnlockedInTree = PandoraTreeComponent
		&& PandoraTreeComponent->IsPandoraUnlockedForTree(PandoraDefinition.Get());
	if (bUnlockedInTree)
	{

		return true;
	}

	UPdGameInstance* PdGameInstance = GetGameInstance<UPdGameInstance>();
	if (!PdGameInstance)
	{
		return false;
	}

	const APlayerController* PlayerController = GetOwningPlayer();
	const APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState : nullptr;
	FString PlayerId = PdGameInstance->ResolveSavePlayerId(PlayerController, PlayerState);
	if (PlayerId.IsEmpty())
	{
		PlayerId = PdGameInstance->GetPreferredSavePlayerId();
	}

	// Default-unlocked definitions are resolved before IsPandoraGranted requires a player id.
	return PdGameInstance->IsPandoraGranted(PlayerId, PandoraDefinition.Get());
}
