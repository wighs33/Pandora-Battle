#include "UI/Widget/ActionSlotEntryWidget.h"

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
#include "Mode/PdPlayerController.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "TimerManager.h"
#include "UI/Widget/InputKeyIconResolver.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "UI/WidgetLookup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActionSlotEntryWidget)

namespace
{
	constexpr float CooldownBindingRetryInterval = 0.1f;

	FString GetFixedActionSlotKeyName(const int32 SlotIndex)
	{
		return SlotIndex == 0 ? TEXT("Shift") : FString();
	}

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
	BindCharacterActionCooldownChanged();
	BindAbilityCooldownChanged();
	CheckForCooldown();
}

void UActionSlotEntryWidget::NativeDestruct()
{
	UnbindCharacterActionCooldownChanged();
	UnbindAbilityCooldownChanged();
	ClearCooldownTimer();

	Super::NativeDestruct();
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
	BindCharacterActionCooldownChanged();
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
	if (!IconImage)
	{
		IconImage = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("IconImage"),
			TEXT("ActionIconImage"),
			TEXT("ActionIcon"),
			TEXT("AbilityImage"),
			TEXT("SlotIcon"),
			TEXT("Icon")
		});
	}

	if (!TimerText)
	{
		TimerText = PdWidgetLookup::FindWidgetByNames<UTextBlock>(this, {
			TEXT("TimerText"),
			TEXT("CooldownText"),
			TEXT("CooldownTimerText"),
			TEXT("Text_Cooldown"),
			TEXT("QuantityTextBlock"),
			TEXT("Txt_Quantity"),
			TEXT("Text_Quantity"),
			TEXT("QuantityText")
		});
	}

	if (!InputKeyOverlay)
	{
		InputKeyOverlay = PdWidgetLookup::FindWidgetByNames<UOverlay>(this, {
			TEXT("InputKeyOverlay"),
			TEXT("KeyOverlay"),
			TEXT("Overlay_InputKey")
		});
	}

	if (!KeyIcon)
	{
		KeyIcon = PdWidgetLookup::FindWidgetByNames<UImage>(this, {
			TEXT("KeyIcon")
		});
	}

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

void UActionSlotEntryWidget::BindCharacterActionCooldownChanged()
{
	UnbindCharacterActionCooldownChanged();

	if (UsesGameplayAbilityCooldown())
	{
		return;
	}

	APdPlayer* PlayerCharacter = ResolveOwningPlayerCharacter();
	if (!PlayerCharacter)
	{
		ScheduleCharacterActionCooldownBindingRetry();
		return;
	}

	BoundPlayerCharacter = PlayerCharacter;
	CooldownChangedHandle = PlayerCharacter->OnCharacterActionCooldownChanged.AddUObject(
		this,
		&ThisClass::HandleCharacterActionCooldownChanged);
}

void UActionSlotEntryWidget::UnbindCharacterActionCooldownChanged()
{
	ClearCharacterActionCooldownBindingRetry();

	APdPlayer* PlayerCharacter = BoundPlayerCharacter.Get();
	if (PlayerCharacter && CooldownChangedHandle.IsValid())
	{
		PlayerCharacter->OnCharacterActionCooldownChanged.Remove(CooldownChangedHandle);
	}

	CooldownChangedHandle.Reset();
	BoundPlayerCharacter.Reset();
}

void UActionSlotEntryWidget::RetryBindCharacterActionCooldown()
{
	if (UsesGameplayAbilityCooldown())
	{
		ClearCharacterActionCooldownBindingRetry();
		return;
	}

	if (BoundPlayerCharacter.IsValid())
	{
		ClearCharacterActionCooldownBindingRetry();
		return;
	}

	if (!ResolveOwningPlayerCharacter())
	{
		return;
	}

	BindCharacterActionCooldownChanged();
	CheckForCooldown();
}

void UActionSlotEntryWidget::ScheduleCharacterActionCooldownBindingRetry()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(CharacterActionCooldownBindingRetryTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		CharacterActionCooldownBindingRetryTimerHandle,
		this,
		&ThisClass::RetryBindCharacterActionCooldown,
		CooldownBindingRetryInterval,
		true);
}

void UActionSlotEntryWidget::ClearCharacterActionCooldownBindingRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CharacterActionCooldownBindingRetryTimerHandle);
	}

	CharacterActionCooldownBindingRetryTimerHandle.Invalidate();
}

void UActionSlotEntryWidget::BindAbilityCooldownChanged()
{
	UnbindAbilityCooldownChanged();

	const FGameplayTag CooldownTag = ResolveAbilityCooldownTag();
	if (!UsesGameplayAbilityCooldown() || !CooldownTag.IsValid())
	{
		return;
	}

	APdPlayer* PlayerCharacter = ResolveOwningPlayerCharacter();
	UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter
		? PlayerCharacter->GetAbilitySystemComponent()
		: nullptr;
	if (!AbilitySystemComponent)
	{
		ScheduleAbilityCooldownBindingRetry();
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
	ClearAbilityCooldownBindingRetry();

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

void UActionSlotEntryWidget::RetryBindAbilityCooldown()
{
	if (!UsesGameplayAbilityCooldown())
	{
		ClearAbilityCooldownBindingRetry();
		return;
	}

	if (BoundAbilitySystemComponent.IsValid())
	{
		ClearAbilityCooldownBindingRetry();
		return;
	}

	APdPlayer* PlayerCharacter = ResolveOwningPlayerCharacter();
	if (!PlayerCharacter || !PlayerCharacter->GetAbilitySystemComponent())
	{
		return;
	}

	BindAbilityCooldownChanged();
	CheckForCooldown();
}

void UActionSlotEntryWidget::ScheduleAbilityCooldownBindingRetry()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(AbilityCooldownBindingRetryTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AbilityCooldownBindingRetryTimerHandle,
		this,
		&ThisClass::RetryBindAbilityCooldown,
		CooldownBindingRetryInterval,
		true);
}

void UActionSlotEntryWidget::ClearAbilityCooldownBindingRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AbilityCooldownBindingRetryTimerHandle);
	}

	AbilityCooldownBindingRetryTimerHandle.Invalidate();
}

void UActionSlotEntryWidget::ClearCooldownTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateCooldownTimerHandle);
	}

	UpdateCooldownTimerHandle.Invalidate();
}

void UActionSlotEntryWidget::HandleCharacterActionCooldownChanged(const ECharacterActionType ChangedActionType)
{
	if (ChangedActionType == ActionType)
	{
		CheckForCooldown();
	}
}

void UActionSlotEntryWidget::HandleAbilityCooldownTagChanged(const FGameplayTag ChangedTag, const int32 NewCount)
{
	static_cast<void>(NewCount);
	if (UsesGameplayAbilityCooldown()
		&& ChangedTag.MatchesTagExact(ResolveAbilityCooldownTag()))
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

	if (UsesGameplayAbilityCooldown() && IsAbilityCooldownSpec(AppliedSpec))
	{
		CheckForCooldown();
	}
}

void UActionSlotEntryWidget::HandleAbilityCooldownEffectRemoved(
	const FActiveGameplayEffect& RemovedEffect)
{
	if (UsesGameplayAbilityCooldown() && IsAbilityCooldownSpec(RemovedEffect.Spec))
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
	const UInputAction* InputAction = ResolveInputAction();
	if (UObject* InputDefinitionIconObject = ResolveInputDefinitionIconObject(InputAction))
	{
		return InputDefinitionIconObject;
	}

	if (UObject* FixedIconObject = ResolveFixedActionSlotInputIconObject())
	{
		return FixedIconObject;
	}

	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	if (!InputAction || !WidgetDefinition)
	{
		return nullptr;
	}

	return PdInputKeyIconResolver::ResolveIconObject(
		GetOwningPlayer(),
		InputAction,
		WidgetDefinition->GetActionSlotWidgetSettings().InputKeyIconSettings);
}

UObject* UActionSlotEntryWidget::ResolveInputDefinitionIconObject(const UInputAction* InputAction) const
{
	const APdPlayerController* PlayerController = Cast<APdPlayerController>(GetOwningPlayer());
	const UControllerInputDefinition* InputDefinition = PlayerController ? PlayerController->GetLoadedInputDefinition() : nullptr;
	return InputDefinition ? InputDefinition->ResolveInputActionIconObject(InputAction) : nullptr;
}

UObject* UActionSlotEntryWidget::ResolveFixedActionSlotInputIconObject() const
{
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	if (!WidgetDefinition)
	{
		return nullptr;
	}

	return PdInputKeyIconResolver::ResolveMappedIconObject(
		WidgetDefinition->GetActionSlotWidgetSettings().InputKeyIconSettings,
		GetFixedActionSlotKeyName(SlotIndex));
}

bool UActionSlotEntryWidget::UsesGameplayAbilityCooldown() const
{
	return ActionType == ECharacterActionType::PandoraWeaponSwap
		|| ActionType == ECharacterActionType::GrappleHook;
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

FGameplayTag UActionSlotEntryWidget::ResolveActionAbilityTag() const
{
	switch (ActionType)
	{
	case ECharacterActionType::PandoraWeaponSwap:
		return LabGameplayTags::Action_Equip;
	case ECharacterActionType::GrappleHook:
		return LabGameplayTags::GameplayAbility_Movement_Grapple;
	default:
		return FGameplayTag();
	}
}

TSubclassOf<UGameplayEffect> UActionSlotEntryWidget::ResolveAbilityCooldownEffectClass() const
{
	const APdPlayer* PlayerCharacter = ResolveOwningPlayerCharacter();
	const UAbilitySystemComponent* AbilitySystemComponent = PlayerCharacter
		? PlayerCharacter->GetAbilitySystemComponent()
		: nullptr;
	const FGameplayTag AbilityTag = ResolveActionAbilityTag();
	if (!AbilitySystemComponent || !AbilityTag.IsValid())
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		UGameplayAbility* Ability = AbilitySpec.Ability.Get();
		if (!Ability
			|| !Ability->GetAssetTags().HasTagExact(AbilityTag))
		{
			continue;
		}

		if (const UGameplayEffect* CooldownEffect = Ability->GetCooldownGameplayEffect())
		{
			return CooldownEffect->GetClass();
		}
	}

	return nullptr;
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

	if (const TSubclassOf<UGameplayEffect> CooldownEffectClass = ResolveAbilityCooldownEffectClass())
	{
		FGameplayEffectQuery EffectClassQuery;
		EffectClassQuery.EffectDefinition = CooldownEffectClass;
		AccumulateCooldownTiming(EffectClassQuery);
	}

	return OutTimeRemaining > 0.0f;
}

bool UActionSlotEntryWidget::IsAbilityCooldownEffect(const UGameplayEffect* GameplayEffect) const
{
	const TSubclassOf<UGameplayEffect> CooldownEffectClass = ResolveAbilityCooldownEffectClass();
	return GameplayEffect
		&& CooldownEffectClass
		&& GameplayEffect->GetClass() == CooldownEffectClass.Get();
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
	return (CooldownTag.IsValid() && AssetTags.HasTagExact(CooldownTag))
		|| IsAbilityCooldownEffect(GameplayEffectSpec.Def);
}

float UActionSlotEntryWidget::ResolveCooldownTimeRemaining() const
{
	const APdPlayer* PlayerCharacter = ResolveOwningPlayerCharacter();
	if (UsesGameplayAbilityCooldown())
	{
		float TimeRemaining = 0.0f;
		float Duration = 0.0f;
		ResolveAbilityCooldownTiming(TimeRemaining, Duration);
		return TimeRemaining;
	}

	return PlayerCharacter ? PlayerCharacter->GetCharacterActionCooldownRemaining(ActionType) : 0.0f;
}

double UActionSlotEntryWidget::ResolveConfiguredCooldownDuration() const
{
	if (UsesGameplayAbilityCooldown())
	{
		float TimeRemaining = 0.0f;
		float Duration = 0.0f;
		if (ResolveAbilityCooldownTiming(TimeRemaining, Duration) && Duration > 0.0f)
		{
			return Duration;
		}

		return ActionDefinition ? ActionDefinition->GetCooldownDuration(ActionType) : 0.0;
	}

	const APdPlayer* PlayerCharacter = ResolveOwningPlayerCharacter();
	const float ActiveCooldownDuration = PlayerCharacter
		? PlayerCharacter->GetCharacterActionCooldownDuration(ActionType)
		: 0.0f;
	if (ActiveCooldownDuration > 0.0f)
	{
		return ActiveCooldownDuration;
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
