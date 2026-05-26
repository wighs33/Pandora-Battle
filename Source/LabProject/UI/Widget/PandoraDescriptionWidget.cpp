#include "UI/Widget/PandoraDescriptionWidget.h"

#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"
#include "Animation/WidgetAnimation.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraDefinition.h"
#include "View/MVVMView.h"
#include "View/MVVMViewClass.h"
#include "ViewModel/PandoraDescriptionViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraDescriptionWidget)

DEFINE_LOG_CATEGORY_STATIC(LogPandoraDescriptionWidget, Log, All);

namespace
{
	FText FormatCurrentLevelText(int32 CurrentLevel)
	{
		return FText::Format(
			NSLOCTEXT("PandoraDescriptionWidget", "CurrentLevelFormat", "Level {0}"),
			FText::AsNumber(CurrentLevel));
	}

	FText FormatNextLevelText(int32 NextLevel)
	{
		return FText::Format(
			NSLOCTEXT("PandoraDescriptionWidget", "NextLevelFormat", "Next Level ({0})"),
			FText::AsNumber(NextLevel));
	}

	FText FormatPointsRequiredText(int32 Points)
	{
		return FText::Format(
			NSLOCTEXT("PandoraDescriptionWidget", "PointsRequiredFormat", "Points Required: {0}"),
			FText::AsNumber(Points));
	}

	FString MakeWeaponTagDisplayName(const FGameplayTag& WeaponTag)
	{
		FString TagText = WeaponTag.ToString();
		TagText.RemoveFromStart(TEXT("Item.Weapon."));
		return TagText.Replace(TEXT("."), TEXT(" / "));
	}

	UPandoraTreeComponent* ResolvePandoraTreeComponentFromWidget(const UUserWidget* Widget)
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

	UE_LOG(LogPandoraDescriptionWidget, Log,
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
	UE_LOG(LogPandoraDescriptionWidget, Log,
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
	UE_LOG(LogPandoraDescriptionWidget, Log,
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

	FText TitleText = FText::GetEmpty();
	FText DescriptionText = FText::GetEmpty();
	if (PandoraDefinition)
	{
		TitleText = PandoraDefinition->GetDisplayName();
		DescriptionText = PandoraDefinition->GetDescription();
	}

	ViewModel->SetTitleText(TitleText);
	ViewModel->SetDescriptionText(DescriptionText);
	ViewModel->SetWeaponRequirementText(FText::GetEmpty());
	ViewModel->SetWeaponRequirementTextColor(WeaponRequirementTextColor);
	ViewModel->SetWeaponRequirementVisibility(ESlateVisibility::Collapsed);
	ViewModel->SetCurrentLevelVisibility(ESlateVisibility::Collapsed);
	ViewModel->SetCurrentLevelTitleText(FText::GetEmpty());
	ViewModel->SetCurrentLevelDescriptionText(FText::GetEmpty());
	ViewModel->SetNextLevelVisibility(ESlateVisibility::Collapsed);
	ViewModel->SetNextLevelTitleText(FText::GetEmpty());
	ViewModel->SetNextLevelDescriptionText(FText::GetEmpty());
	ViewModel->SetPointsRequiredVisibility(ESlateVisibility::Collapsed);
	ViewModel->SetPointsRequiredText(FText::GetEmpty());

	if (!PandoraDefinition)
	{
		UE_LOG(LogPandoraDescriptionWidget, Warning,
			TEXT("[SetDetails] no pandora definition. widget=%s tree=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraTreeComponent.Get()));
		return;
	}

	int32 CurrentLevel = 0;
	int32 NextLevel = 1;
	int32 MaxLevel = FMath::Max(PandoraDefinition->GetMaxLevel(), 1);
	if (PandoraTreeComponent && PandoraDefinition)
	{
		CurrentLevel = PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition.Get());
		MaxLevel = FMath::Max(PandoraTreeComponent->GetMaxPandoraLevel(PandoraDefinition.Get()), 1);
		NextLevel = CurrentLevel + 1 > MaxLevel ? -1 : CurrentLevel + 1;

		ViewModel->SetPointsRequiredText(FormatPointsRequiredText(PandoraTreeComponent->GetRequiredPointsForPandora(PandoraDefinition.Get(), true)));
	}
	else
	{
		CurrentLevel = 0;
		NextLevel = CurrentLevel + 1 > MaxLevel ? -1 : CurrentLevel + 1;
	}

	const bool bLockedByPandoraRequirement = PandoraTreeComponent
		&& PandoraDefinition
		&& CurrentLevel <= 0
		&& !PandoraTreeComponent->ArePandoraUnlockRulesMet(PandoraDefinition.Get());
	if (bLockedByPandoraRequirement)
	{
		FText RequirementText = PandoraTreeComponent->GetPandoraUnlockRequirementsText(PandoraDefinition.Get());
		if (RequirementText.IsEmpty())
		{
			RequirementText = NSLOCTEXT("PandoraDescriptionWidget", "LockedRequirementFallback", "요구 조건이 충족되지 않았습니다.");
		}

		ViewModel->SetDescriptionText(RequirementText);
		const FText WeaponRequirement = GetWeaponRequirementText();
		ViewModel->SetWeaponRequirementText(WeaponRequirement);
		ViewModel->SetWeaponRequirementVisibility(WeaponRequirement.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

		UE_LOG(LogPandoraDescriptionWidget, Log,
			TEXT("[SetDetails] locked requirements only. widget=%s pandora=%s tree=%s current=%d max=%d requirements=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PandoraDefinition.Get()),
			*GetNameSafe(PandoraTreeComponent.Get()),
			CurrentLevel,
			MaxLevel,
			*RequirementText.ToString());
		return;
	}

	const FText WeaponRequirement = GetWeaponRequirementText();
	ViewModel->SetWeaponRequirementText(WeaponRequirement);
	ViewModel->SetWeaponRequirementVisibility(WeaponRequirement.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	const FText CurrentDescription = GetDescriptionForLevel(CurrentLevel);
	const bool bHasCurrentDescription = !CurrentDescription.IsEmpty();
	ViewModel->SetCurrentLevelVisibility(bHasCurrentDescription ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bHasCurrentDescription)
	{
		ViewModel->SetCurrentLevelTitleText(FormatCurrentLevelText(CurrentLevel));
		ViewModel->SetCurrentLevelDescriptionText(CurrentDescription);
	}

	const FText NextDescription = GetDescriptionForLevel(NextLevel);
	const bool bHasNextDescription = !NextDescription.IsEmpty();
	ViewModel->SetNextLevelVisibility(bHasNextDescription ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bHasNextDescription)
	{
		ViewModel->SetNextLevelTitleText(FormatNextLevelText(NextLevel));
		ViewModel->SetNextLevelDescriptionText(NextDescription);
		ViewModel->SetPointsRequiredVisibility(ESlateVisibility::Visible);
	}
	else if (CurrentLevel >= MaxLevel)
	{
		ViewModel->SetPointsRequiredVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogPandoraDescriptionWidget, Log,
		TEXT("[SetDetails] widget=%s pandora=%s tree=%s current=%d next=%d max=%d currentDesc=%s nextDesc=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PandoraDefinition.Get()),
		*GetNameSafe(PandoraTreeComponent.Get()),
		CurrentLevel,
		NextLevel,
		MaxLevel,
		bHasCurrentDescription ? TEXT("true") : TEXT("false"),
		bHasNextDescription ? TEXT("true") : TEXT("false"));
}

void UPandoraDescriptionWidget::ResolvePandoraTreeComponent()
{
	if (!PandoraTreeComponent)
	{
		PandoraTreeComponent = ResolvePandoraTreeComponentFromWidget(this);
	}

	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = PandoraTreeComponent->GetPandoraDefinition();
	}

	UE_LOG(LogPandoraDescriptionWidget, Log,
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

FText UPandoraDescriptionWidget::GetDescriptionForLevel(int32 Level) const
{
	if (PandoraDefinition)
	{
		return PandoraDefinition->GetDescriptionForLevel(Level);
	}

	return FText::GetEmpty();
}

FText UPandoraDescriptionWidget::GetWeaponRequirementText() const
{
	if (!PandoraDefinition || PandoraDefinition->ActivatableWeaponTags.IsEmpty())
	{
		return FText::GetEmpty();
	}

	TArray<FString> WeaponTypeNames;
	for (const FGameplayTag& WeaponTag : PandoraDefinition->ActivatableWeaponTags)
	{
		if (WeaponTag.IsValid())
		{
			WeaponTypeNames.Add(MakeWeaponTagDisplayName(WeaponTag));
		}
	}

	if (WeaponTypeNames.IsEmpty())
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		NSLOCTEXT("PandoraDescriptionWidget", "WeaponRequirementFormat", "Required Weapon Type: {0}"),
		FText::FromString(FString::Join(WeaponTypeNames, TEXT(", "))));
}
