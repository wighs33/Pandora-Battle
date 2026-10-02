#include "UI/HUD/Ability/ActionSlotEntryWidget.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "InputAction.h"
#include "TimerManager.h"
#include "UI/Common/InputKeyIconResolver.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/Common/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActionSlotEntryWidget)

namespace
{
	const TSoftObjectPtr<UInputAction>* GetSettingsInputAction(
		const FActionSlotWidgetSettings& Settings,
		const ECharacterActionType ActionType)
	{
		switch (ActionType)
		{
		case ECharacterActionType::PandoraWeaponSwap:
			return &Settings.PandoraWeaponSwapInputAction;
		case ECharacterActionType::GrappleHook:
			return &Settings.GrappleHookInputAction;
		default:
			return nullptr;
		}
	}
}

void UActionSlotEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyWidgetDefinitionSettings();
	RefreshVisual();
}

void UActionSlotEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	RefreshVisual();
	CheckForCooldown();
	PossessedCharacterReadySubscription.SubscribeToPossessedCharacter(GetOwningPlayer(),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePossessedCharacterReady));
}

void UActionSlotEntryWidget::NativeDestruct()
{
	PossessedCharacterReadySubscription.Reset();
	UnbindAbilityCooldownChanged();
	ClearCooldownTimer();

	Super::NativeDestruct();
}

// 조종 캐릭터의 ASC가 준비될 때마다(리스폰 포함) 쿨다운 구독을 다시 연결한다.
void UActionSlotEntryWidget::HandlePossessedCharacterReady(
	ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent)
{
	static_cast<void>(Character);
	static_cast<void>(AbilitySystemComponent);
	BindAbilityCooldownChanged();
	CheckForCooldown();
}

void UActionSlotEntryWidget::SetActionSlotData(
	const int32 InSlotIndex,
	const ECharacterActionType InActionType,
	UCharacterActionDefinition* InActionDefinition)
{
	SlotIndex = FMath::Max(0, InSlotIndex);
	ActionType = InActionType;
	ActionDefinition = InActionDefinition;

	RefreshVisual();
	BindAbilityCooldownChanged();
	CheckForCooldown();
}

void UActionSlotEntryWidget::RefreshVisual()
{
	CacheOptionalWidgets();
	ApplyActionVisual();
	ApplyInputKeyIcon();
}

void UActionSlotEntryWidget::CacheOptionalWidgets()
{

	if (!ActionActiveFrame)
	{
		ActionActiveFrame = PdWidgetLookup::FindWidgetByNames<UWidget>(this, {
			TEXT("ActionActiveFrame"),
			TEXT("AbilityActiveFrame"),
			TEXT("ActiveFrame")
		});
	}
}

void UActionSlotEntryWidget::ApplyWidgetDefinitionSettings()
{
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	if (!WidgetDefinition)
	{
		return;
	}

	const FActionSlotWidgetSettings& Settings = WidgetDefinition->GetActionSlotWidgetSettings();
	bShowCooldownTimeRemaining = Settings.bShowCooldownTimeRemaining;
	ReadyInputKeyOpacity = Settings.ReadyInputKeyOpacity;
	CooldownInputKeyOpacity = Settings.CooldownInputKeyOpacity;
	bHideInputKeyIcon = Settings.InputKeyIconSettings.bHideInputKeyIcon;
	InputKeyIconSize = Settings.InputKeyIconSettings.IconSize;
}

void UActionSlotEntryWidget::ApplyActionVisual()
{
	if (IconImage)
	{
		UObject* IconObject = ResolveActionIcon();
		IconImage->SetBrushResourceObject(IconObject);
		IconImage->SetVisibility(IconObject ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}

	if (ActionActiveFrame)
	{
		ActionActiveFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UActionSlotEntryWidget::ApplyInputKeyIcon()
{
	if (KeyText && InputKeyOverlay)
	{
		if (KeyIcon)
		{
			KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
		const FText Caption = IsDesignTime() ? KeyText->GetText()
			: PdInputKeyIconResolver::ResolveInputDefinitionKeyText(GetOwningPlayer(), ResolveInputAction());
		KeyText->SetText(Caption);
		InputKeyOverlay->SetVisibility(bHideInputKeyIcon || Caption.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	if (!InputKeyOverlay || !KeyIcon)
	{
		if (InputKeyOverlay)
		{
			InputKeyOverlay->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (bHideInputKeyIcon)
	{
		InputKeyOverlay->SetVisibility(ESlateVisibility::Collapsed);
		KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UObject* IconObject = ResolveInputIconObject();
	if (!IconObject)
	{
		InputKeyOverlay->SetVisibility(ESlateVisibility::Collapsed);
		KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	InputKeyOverlay->SetVisibility(ESlateVisibility::Visible);
	KeyIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	KeyIcon->SetBrush(PdInputKeyIconResolver::MakeImageBrushFromExisting(
		KeyIcon->GetBrush(),
		IconObject,
		InputKeyIconSize));
}

void UActionSlotEntryWidget::CheckForCooldown()
{
	if (IsDesignTime())
	{
		return;
	}

	const float TimeRemaining = ResolveCooldownTimeRemaining();
	if (TimeRemaining <= 0.0f)
	{
		ClearCooldownTimer();

		if (CooldownTimerContainer)
		{
			CooldownTimerContainer->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (CooldownProgress)
		{
			CooldownProgress->SetPercent(1.0f);
		}
		if (TimerText)
		{
			TimerText->SetText(FText::GetEmpty());
			TimerText->SetVisibility(ESlateVisibility::Collapsed);
		}

		SetInputKeyRenderOpacity(ReadyInputKeyOpacity);
		return;
	}

	TotalCooldownTime = FMath::Max(ResolveConfiguredCooldownDuration(), static_cast<double>(TimeRemaining));

	ClearCooldownTimer();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			UpdateCooldownTimerHandle,
			this,
			&ThisClass::UpdateCooldownProgress,
			0.05f,
			true);
	}

	if (CooldownTimerContainer)
	{
		CooldownTimerContainer->SetVisibility(ESlateVisibility::Visible);
	}

	SetInputKeyRenderOpacity(CooldownInputKeyOpacity);
	UpdateCooldownProgress();
}

void UActionSlotEntryWidget::UpdateCooldownProgress()
{
	if (IsDesignTime())
	{
		return;
	}

	const float TimeRemaining = ResolveCooldownTimeRemaining();
	if (TimeRemaining <= 0.0f)
	{
		ClearCooldownTimer();

		if (CooldownTimerContainer)
		{
			CooldownTimerContainer->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (CooldownProgress)
		{
			CooldownProgress->SetPercent(1.0f);
		}
		if (TimerText)
		{
			TimerText->SetText(FText::GetEmpty());
			TimerText->SetVisibility(ESlateVisibility::Collapsed);
		}

		SetInputKeyRenderOpacity(ReadyInputKeyOpacity);
		return;
	}

	if (CooldownProgress)
	{
		CooldownProgress->SetPercent(CalculateCooldownPercent(TimeRemaining, TotalCooldownTime));
	}

	if (TimerText)
	{
		TimerText->SetText(bShowCooldownTimeRemaining ? FText::AsNumber(FMath::CeilToInt(TimeRemaining)) : FText::GetEmpty());
		TimerText->SetVisibility(bShowCooldownTimeRemaining ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UActionSlotEntryWidget::BindAbilityCooldownChanged()
{
	UnbindAbilityCooldownChanged();

	const FGameplayTag CooldownTag = ResolveAbilityCooldownTag();
	if (!CooldownTag.IsValid())
	{
		return;
	}

	// 조종 캐릭터가 아직 준비되지 않았으면 HandlePossessedCharacterReady가 다시 연결한다.
	APdPlayer* PlayerCharacter = ResolveOwningPlayerCharacter();
	UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter
		? PlayerCharacter->GetAbilitySystemComponent()
		: nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	BoundAbilitySystemComponent = AbilitySystemComponent;
	BoundAbilityCooldownTag = CooldownTag;
	AbilityCooldownChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(
		CooldownTag,
		EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&ThisClass::HandleAbilityCooldownTagChanged);
	AbilityCooldownEffectAddedHandle =
		AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
			this,
			&ThisClass::HandleAbilityCooldownEffectAdded);
	AbilityCooldownEffectRemovedHandle =
		AbilitySystemComponent->OnAnyGameplayEffectRemovedDelegate().AddUObject(
			this,
			&ThisClass::HandleAbilityCooldownEffectRemoved);
}

void UActionSlotEntryWidget::UnbindAbilityCooldownChanged()
{
	UAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	if (AbilitySystemComponent
		&& BoundAbilityCooldownTag.IsValid()
		&& AbilityCooldownChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			BoundAbilityCooldownTag,
			EGameplayTagEventType::NewOrRemoved).Remove(AbilityCooldownChangedHandle);
	}
	if (AbilitySystemComponent && AbilityCooldownEffectAddedHandle.IsValid())
	{
		AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.Remove(
			AbilityCooldownEffectAddedHandle);
	}
	if (AbilitySystemComponent && AbilityCooldownEffectRemovedHandle.IsValid())
	{
		AbilitySystemComponent->OnAnyGameplayEffectRemovedDelegate().Remove(
			AbilityCooldownEffectRemovedHandle);
	}

	AbilityCooldownChangedHandle.Reset();
	AbilityCooldownEffectAddedHandle.Reset();
	AbilityCooldownEffectRemovedHandle.Reset();
	BoundAbilityCooldownTag = FGameplayTag();
	BoundAbilitySystemComponent.Reset();
}

void UActionSlotEntryWidget::ClearCooldownTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateCooldownTimerHandle);
	}

	UpdateCooldownTimerHandle.Invalidate();
}

void UActionSlotEntryWidget::HandleAbilityCooldownTagChanged(const FGameplayTag ChangedTag, const int32 NewCount)
{
	static_cast<void>(NewCount);
	if (ChangedTag.MatchesTagExact(ResolveAbilityCooldownTag()))
	{
		CheckForCooldown();
	}
}

void UActionSlotEntryWidget::HandleAbilityCooldownEffectAdded(
	UAbilitySystemComponent* TargetAbilitySystemComponent,
	const FGameplayEffectSpec& AppliedSpec,
	const FActiveGameplayEffectHandle ActiveHandle)
{
	static_cast<void>(TargetAbilitySystemComponent);
	static_cast<void>(ActiveHandle);

	if (IsAbilityCooldownSpec(AppliedSpec))
	{
		CheckForCooldown();
	}
}

void UActionSlotEntryWidget::HandleAbilityCooldownEffectRemoved(
	const FActiveGameplayEffect& RemovedEffect)
{
	if (IsAbilityCooldownSpec(RemovedEffect.Spec))
	{
		CheckForCooldown();
	}
}

void UActionSlotEntryWidget::SetInputKeyRenderOpacity(const float InOpacity) const
{
	if (InputKeyOverlay)
	{
		InputKeyOverlay->SetRenderOpacity(InOpacity);
		return;
	}

	if (KeyIcon)
	{
		KeyIcon->SetRenderOpacity(InOpacity);
	}
}

APdPlayer* UActionSlotEntryWidget::ResolveOwningPlayerCharacter() const
{
	if (APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetOwningPlayerPawn()))
	{
		return PlayerCharacter;
	}

	const APlayerController* PlayerController = GetOwningPlayer();
	return PlayerController ? Cast<APdPlayer>(PlayerController->GetPawn()) : nullptr;
}

UObject* UActionSlotEntryWidget::ResolveActionIcon() const
{
	return ActionDefinition
		? ActionDefinition->GetLoadedIconResource(ActionType)
		: nullptr;
}

UInputAction* UActionSlotEntryWidget::ResolveInputAction() const
{
	if (ActionDefinition)
	{
		if (UInputAction* InputAction =
			ActionDefinition->GetLoadedInputAction(ActionType))
		{
			return InputAction;
		}
	}

	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	if (!WidgetDefinition)
	{
		return nullptr;
	}

	const FActionSlotWidgetSettings& Settings = WidgetDefinition->GetActionSlotWidgetSettings();
	const TSoftObjectPtr<UInputAction>* InputAction = GetSettingsInputAction(Settings, ActionType);
	return InputAction ? InputAction->Get() : nullptr;
}

UObject* UActionSlotEntryWidget::ResolveInputIconObject() const
{
	return PdInputKeyIconResolver::ResolveInputDefinitionIconObject(
		GetOwningPlayer(),
		ResolveInputAction());
}

FGameplayTag UActionSlotEntryWidget::ResolveAbilityCooldownTag() const
{
	switch (ActionType)
	{
	case ECharacterActionType::PandoraWeaponSwap:
		return LabGameplayTags::Cooldown_EquipWeapon;
	case ECharacterActionType::GrappleHook:
		return LabGameplayTags::Cooldown_Grapple;
	default:
		return FGameplayTag();
	}
}

bool UActionSlotEntryWidget::ResolveAbilityCooldownTiming(
	float& OutTimeRemaining,
	float& OutDuration) const
{
	OutTimeRemaining = 0.0f;
	OutDuration = 0.0f;

	const APdPlayer* PlayerCharacter = ResolveOwningPlayerCharacter();
	const UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter
		? PlayerCharacter->GetAbilitySystemComponent()
		: nullptr;
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const auto AccumulateCooldownTiming =
		[AbilitySystemComponent, &OutTimeRemaining, &OutDuration](const FGameplayEffectQuery& Query)
	{
		const TArray<TPair<float, float>> CooldownTimings =
			AbilitySystemComponent->GetActiveEffectsTimeRemainingAndDuration(Query);
		for (const TPair<float, float>& CooldownTiming : CooldownTimings)
		{
			if (CooldownTiming.Key > OutTimeRemaining)
			{
				OutTimeRemaining = CooldownTiming.Key;
				OutDuration = CooldownTiming.Value;
			}
		}
	};

	const FGameplayTag CooldownTag = ResolveAbilityCooldownTag();
	FGameplayTagContainer CooldownTags;
	if (CooldownTag.IsValid())
	{
		CooldownTags.AddTag(CooldownTag);
		AccumulateCooldownTiming(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags));
	}

	return OutTimeRemaining > 0.0f;
}

bool UActionSlotEntryWidget::IsAbilityCooldownSpec(
	const FGameplayEffectSpec& GameplayEffectSpec) const
{
	const FGameplayTag CooldownTag = ResolveAbilityCooldownTag();
	FGameplayTagContainer GrantedTags;
	GameplayEffectSpec.GetAllGrantedTags(GrantedTags);
	if (CooldownTag.IsValid() && GrantedTags.HasTagExact(CooldownTag))
	{
		return true;
	}

	FGameplayTagContainer AssetTags;
	GameplayEffectSpec.GetAllAssetTags(AssetTags);
	return CooldownTag.IsValid() && AssetTags.HasTagExact(CooldownTag);
}

float UActionSlotEntryWidget::ResolveCooldownTimeRemaining() const
{
	float TimeRemaining = 0.0f;
	float Duration = 0.0f;
	ResolveAbilityCooldownTiming(TimeRemaining, Duration);
	return TimeRemaining;
}

double UActionSlotEntryWidget::ResolveConfiguredCooldownDuration() const
{
	float TimeRemaining = 0.0f;
	float Duration = 0.0f;
	if (ResolveAbilityCooldownTiming(TimeRemaining, Duration)
		&& Duration > 0.0f)
	{
		return Duration;
	}

	return ActionDefinition ? ActionDefinition->GetCooldownDuration(ActionType) : 0.0;
}

float UActionSlotEntryWidget::CalculateCooldownPercent(const float TimeRemaining, const double CooldownDuration)
{
	if (CooldownDuration <= UE_SMALL_NUMBER)
	{
		return 1.0f;
	}

	return FMath::Clamp(1.0f - static_cast<float>(TimeRemaining / CooldownDuration), 0.0f, 1.0f);
}
