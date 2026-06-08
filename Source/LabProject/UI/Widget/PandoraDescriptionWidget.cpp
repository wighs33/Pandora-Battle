#include "UI/Widget/PandoraDescriptionWidget.h"

#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"
#include "Animation/WidgetAnimation.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraDefinition.h"
#include "UI/Widget/PandoraWidgetViewData.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "ViewModel/PandoraDescriptionViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraDescriptionWidget)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraDescriptionWidget, Log, All);

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

	UE_LOG(LogPandoraDescriptionWidget, Verbose,
		TEXT("[Construct] widget=%s tree=%s pandora=%s viewModel=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraTreeComponent.Get()),
		*GetNameSafe(PandoraDefinition.Get()),
		*GetNameSafe(PandoraDescriptionViewModel.Get()));

	if (ScaleUp)
	{
		PlayAnimation(ScaleUp, 0.1f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
	}
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
	UE_LOG(LogPandoraDescriptionWidget, Verbose,
		TEXT("[SetPandoraDefinition] widget=%s pandora=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraDefinition.Get()));
	SetDetails();
}

void UPandoraDescriptionWidget::SetPandoraTreeComponent(UPandoraTreeComponent* InPandoraTreeComponent)
{
	PandoraTreeComponent = InPandoraTreeComponent;
	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = PandoraTreeComponent->GetPandoraDefinition();
	}
	UE_LOG(LogPandoraDescriptionWidget, Verbose,
		TEXT("[SetTreeComponent] widget=%s tree=%s pandora=%s points=%d"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraTreeComponent.Get()),
		*GetNameSafe(PandoraDefinition.Get()),
		PandoraTreeComponent ? PandoraTreeComponent->GetPointsAvailable() : INDEX_NONE);
	SetDetails();
}

void UPandoraDescriptionWidget::SetDetails()
{
	UPandoraDescriptionViewModel* ViewModel = GetOrCreatePandoraDescriptionViewModel();
	if (!ViewModel)
	{
		return;
	}

	ViewModel->SetWeaponRequirementTextColor(WeaponRequirementTextColor);

	const FPandoraDescriptionViewData ViewData = FPandoraDescriptionViewDataBuilder::Build(
		PandoraDefinition.Get(),
		PandoraTreeComponent.Get());

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
		UE_LOG(LogPandoraDescriptionWidget, Warning,
			TEXT("[SetDetails] no pandora definition. widget=%s tree=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraTreeComponent.Get()));
		return;
	}

	if (ViewData.bLockedByPandoraRequirement)
	{
		UE_LOG(LogPandoraDescriptionWidget, Verbose,
			TEXT("[SetDetails] locked requirements only. widget=%s pandora=%s tree=%s current=%d max=%d requirements=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraDefinition.Get()),
			*GetNameSafe(PandoraTreeComponent.Get()),
			ViewData.CurrentLevel,
			ViewData.MaxLevel,
			*ViewData.DescriptionText.ToString());
		return;
	}

	UE_LOG(LogPandoraDescriptionWidget, Verbose,
		TEXT("[SetDetails] widget=%s pandora=%s tree=%s current=%d next=%d max=%d currentDesc=%s nextDesc=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraDefinition.Get()),
		*GetNameSafe(PandoraTreeComponent.Get()),
		ViewData.CurrentLevel,
		ViewData.NextLevel,
		ViewData.MaxLevel,
		ViewData.CurrentLevelVisibility == ESlateVisibility::Visible ? TEXT("true") : TEXT("false"),
		ViewData.NextLevelVisibility == ESlateVisibility::Visible ? TEXT("true") : TEXT("false"));
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

	UE_LOG(LogPandoraDescriptionWidget, Verbose,
		TEXT("[ResolveTreeComponent] widget=%s tree=%s pandora=%s owningPlayer=%s owningPawn=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraTreeComponent.Get()),
		*GetNameSafe(PandoraDefinition.Get()),
		*GetNameSafe(GetOwningPlayer()),
		*GetNameSafe(GetOwningPlayerPawn()));
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
		UE_LOG(LogPandoraDescriptionWidget, Warning,
			TEXT("[ApplyViewModel] skipped: widget has MVVM extension, but no settable PandoraDescriptionViewModel source. widget=%s viewModel=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraDescriptionViewModel.Get()));
		return;
	}

	const bool bSuccess = ViewExtension->SetViewModel(RuntimeViewModelName, PandoraDescriptionViewModel);
	if (!bSuccess)
	{
		UE_LOG(LogPandoraDescriptionWidget, Warning,
			TEXT("[ApplyViewModel] failed. widget=%s viewModelName=%s viewModel=%s"),
			*GetNameSafe(this),
			*RuntimeViewModelName.ToString(),
			*GetNameSafe(PandoraDescriptionViewModel.Get()));
	}
}
