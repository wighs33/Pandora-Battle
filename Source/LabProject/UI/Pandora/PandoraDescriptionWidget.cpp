#include "UI/Pandora/PandoraDescriptionWidget.h"

#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Components/Image.h"
#include "GameFramework/PlayerState.h"
#include "Mode/PdPlayerState.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "UI/Pandora/SkillEffectIconResolver.h"
#include "UI/Common/ViewModelBinding.h"
#include "UI/Pandora/PandoraWidgetViewData.h"
#include "ViewModel/PandoraDescriptionViewModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PandoraDescriptionWidget)

void UPandoraDescriptionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PdViewModelBinding::SetViewModel(this, GetOrCreatePandoraDescriptionViewModel());
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

void UPandoraDescriptionWidget::OnMenuLanguageChanged()
{
	SetDetails();
}

void UPandoraDescriptionWidget::SetPandoraDefinition(const UPandoraDefinition* InPandoraDefinition)
{
	PandoraDefinition = InPandoraDefinition;

	SetDetails();
}

void UPandoraDescriptionWidget::SetPandoraTreeComponent(UPandoraTreeComponent* InPandoraTreeComponent)
{
	PandoraTreeComponent = InPandoraTreeComponent;
	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = FPandoraWidgetViewDataBuilder::GetSelectedPandoraDefinition(PandoraTreeComponent);
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

	FPandoraDescriptionViewData ViewData = FPandoraDescriptionViewDataBuilder::Build(PandoraDefinition.Get(),
		PandoraTreeComponent.Get(), GetLocalization());
	if (!PandoraTreeComponent && PandoraDefinition && !FPandoraWidgetViewDataBuilder::IsPandoraOwnedInProfile(this, PandoraDefinition))
	{
		ViewData.DescriptionText = NSLOCTEXT("PandoraDescriptionWidget", "UnownedPandoraDescription", "You do not own this Pandora.");
		ViewData.WeaponRequirementVisibility = ESlateVisibility::Collapsed;
		ViewData.SkillSectionVisibility = ESlateVisibility::Collapsed;
	}

	ViewModel->SetTitleText(ViewData.TitleText);
	ViewModel->SetDescriptionText(ViewData.DescriptionText);
	ViewModel->SetWeaponRequirementText(ViewData.WeaponRequirementText);
	ViewModel->SetWeaponRequirementVisibility(ViewData.WeaponRequirementVisibility);
	ViewModel->SetSkillSectionVisibility(ViewData.SkillSectionVisibility);

	for (int32 SkillSlotIndex = 0; SkillSlotIndex < 4; ++SkillSlotIndex)
	{
		const FPandoraSkillSlotViewData* SkillViewData = ViewData.SkillSlots.IsValidIndex(SkillSlotIndex)
			? &ViewData.SkillSlots[SkillSlotIndex]
			: nullptr;

		ViewModel->SetSkillSlot(SkillSlotIndex, SkillViewData ? SkillViewData->IconResource : nullptr,
			SkillViewData ? SkillViewData->DisplayName : FText::GetEmpty(),
			SkillViewData ? SkillViewData->Description : FText::GetEmpty(),
			SkillViewData ? SkillViewData->ManaText : FText::GetEmpty(),
			SkillViewData ? SkillViewData->CooldownText : FText::GetEmpty());
	}
}

void UPandoraDescriptionWidget::ResolvePandoraTreeComponent()
{
	if (!PandoraTreeComponent)
	{
		const APdPlayerState* PlayerState = FPandoraWidgetViewDataBuilder::FindOwningPlayerState(this);
		PandoraTreeComponent = PlayerState ? PlayerState->GetPandoraTreeComponent() : nullptr;
	}

	if (!PandoraDefinition && PandoraTreeComponent)
	{
		PandoraDefinition = FPandoraWidgetViewDataBuilder::GetSelectedPandoraDefinition(PandoraTreeComponent);
	}
}

void UPandoraDescriptionWidget::ApplyEffectIconResources()
{
	const TArray<UImage*> EffectIconResources = {
		EffectIconResource1.Get(),
		EffectIconResource2.Get(),
		EffectIconResource3.Get()
	};

	for (int32 SkillIndex = 0; SkillIndex < EffectIconResources.Num(); ++SkillIndex)
	{
		const USkillDefinition* Skill = PandoraDefinition ? PandoraDefinition->GetSkillDefinition(SkillIndex) : nullptr;
		PdSkillEffectIconResolver::ApplySkillEffectIcon(this, Skill, EffectIconResources[SkillIndex]);
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
