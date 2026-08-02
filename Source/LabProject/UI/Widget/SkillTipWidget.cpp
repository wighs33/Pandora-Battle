#include "UI/Widget/SkillTipWidget.h"

#include "Component/Pandora/PandoraComponent.h"
#include "Components/Image.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "GameFramework/Pawn.h"
#include "Mode/PdPlayerState.h"
#include "TimerManager.h"
#include "UI/Widget/SkillEffectIconResolver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkillTipWidget)

void USkillTipWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializePandoraBinding();
}

void USkillTipWidget::NativeDestruct()
{
	UnbindPandoraComponent();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PandoraBindingRetryTimerHandle);
	}

	Super::NativeDestruct();
}

void USkillTipWidget::InitializePandoraBinding()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PandoraBindingRetryTimerHandle);
	}

	UPandoraComponent* PandoraComponent = ResolvePandoraComponent();
	if (!PandoraComponent)
	{
		UnbindPandoraComponent();
		RefreshSkillTips();
		SchedulePandoraBindingRetry();
		return;
	}

	if (CachedPandoraComponent.Get() != PandoraComponent)
	{
		UnbindPandoraComponent();
		CachedPandoraComponent = PandoraComponent;
	}

	PandoraComponent->OnPandoraSelectionChanged.RemoveDynamic(
		this,
		&ThisClass::HandlePandoraSelectionChanged);
	PandoraComponent->OnPandoraSelectionChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandlePandoraSelectionChanged);
	PandoraComponent->OnPandoraLoadoutChanged.RemoveDynamic(
		this,
		&ThisClass::HandlePandoraLoadoutChanged);
	PandoraComponent->OnPandoraLoadoutChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandlePandoraLoadoutChanged);

	RefreshSkillTips();
}

void USkillTipWidget::UnbindPandoraComponent()
{
	if (UPandoraComponent* PandoraComponent = CachedPandoraComponent.Get())
	{
		PandoraComponent->OnPandoraSelectionChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraSelectionChanged);
		PandoraComponent->OnPandoraLoadoutChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraLoadoutChanged);
	}

	CachedPandoraComponent.Reset();
}

void USkillTipWidget::SchedulePandoraBindingRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PandoraBindingRetryTimerHandle,
			this,
			&ThisClass::InitializePandoraBinding,
			0.1f,
			false);
	}
}

UPandoraComponent* USkillTipWidget::ResolvePandoraComponent() const
{
	APdPlayerState* PlayerState = nullptr;
	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		PlayerState = OwningPlayer->GetPlayerState<APdPlayerState>();
	}

	if (!PlayerState)
	{
		if (APawn* OwningPawn = GetOwningPlayerPawn())
		{
			PlayerState = OwningPawn->GetPlayerState<APdPlayerState>();
		}
	}

	return PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
}

void USkillTipWidget::RefreshSkillTips()
{
	UPandoraComponent* PandoraComponent = CachedPandoraComponent.Get();
	if (!PandoraComponent)
	{
		PandoraComponent = ResolvePandoraComponent();
	}

	const UPandoraDefinition* PandoraDefinition = PandoraComponent
		? PandoraComponent->GetCurrentPandoraDefinition()
		: nullptr;

	const TArray<UImage*> ImageWidgets =
	{
		Img_FirstSkillTip.Get(),
		Img_SecondSkillTip.Get(),
		Img_ThirdSkillTip.Get()
	};

	for (int32 SkillIndex = 0; SkillIndex < ImageWidgets.Num(); ++SkillIndex)
	{
		const FSkill* Skill = PandoraDefinition && PandoraDefinition->Skill.IsValidIndex(SkillIndex)
			? &PandoraDefinition->Skill[SkillIndex]
			: nullptr;
		PdSkillEffectIconResolver::ApplySkillEffectIcon(
			this,
			Skill,
			ImageWidgets[SkillIndex],
			ESkillEffectIconSet::SkillTip);
	}
}

void USkillTipWidget::HandlePandoraSelectionChanged(UPandoraDefinition* PandoraDefinition)
{
	static_cast<void>(PandoraDefinition);
	RefreshSkillTips();
}

void USkillTipWidget::HandlePandoraLoadoutChanged()
{
	RefreshSkillTips();
}
