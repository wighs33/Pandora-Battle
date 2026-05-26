#include "UI/Widget/AbilitySlotWidget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "Abilities/GameplayAbility.h"
#include "Common/LabGameplayTags.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Texture2D.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilitySlotWidget)

void UAbilitySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializeAbilityObject();
	SetAbilityImage();
	SetInputKeyRenderOpacity(ReadyInputKeyOpacity);

	if (InputKeyOverlay)
	{
		InputKeyOverlay->SetVisibility(bHideInputKeyIcon ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (AbilityText)
	{
		AbilityText->SetText(ResolveAbilityDisplayName());
	}

	if (CooldownProgress)
	{
		CooldownProgress->SetPercent(1.0f);
	}

	if (CooldownTimerContainer)
	{
		CooldownTimerContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (AbilityActiveFrame)
	{
		AbilityActiveFrame->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (TimerText)
	{
		TimerText->SetText(FText::GetEmpty());
	}

	ApplyAbilitySlotEnabledState();

	CheckForCooldown();
	CheckForActivation();
	BindGameplayTagEvents();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::SetInputKeyIcon);
	}
	else
	{
		SetInputKeyIcon();
	}
}

void UAbilitySlotWidget::NativeDestruct()
{
	ClearCooldownTimer();
	UnbindGameplayTagEvents();
	Super::NativeDestruct();
}

void UAbilitySlotWidget::SetAbilitySpecHandle(FGameplayAbilitySpecHandle InAbilitySpecHandle)
{
	AbilitySpecHandle = InAbilitySpecHandle;
}

void UAbilitySlotWidget::SetAbilitySlotData(
	FGameplayAbilitySpecHandle InAbilitySpecHandle,
	FText InDisplayNameOverride,
	UObject* InIconOverride)
{
	SetAbilitySpecHandle(InAbilitySpecHandle);
	SetAbilityDisplayOverride(InDisplayNameOverride, InIconOverride);
}

void UAbilitySlotWidget::SetAbilitySlotDataEnabled(
	FGameplayAbilitySpecHandle InAbilitySpecHandle,
	FText InDisplayNameOverride,
	UObject* InIconOverride,
	bool bEnabled)
{
	SetAbilitySlotData(InAbilitySpecHandle, MoveTemp(InDisplayNameOverride), InIconOverride);
	SetAbilitySlotEnabled(bEnabled);
}

void UAbilitySlotWidget::SetAbilityDisplayOverride(FText InDisplayNameOverride, UObject* InIconOverride)
{
	bHasAbilityDisplayOverride = true;
	AbilityDisplayNameOverride = MoveTemp(InDisplayNameOverride);
	AbilityIconOverride = InIconOverride;

	SetAbilityImage();

	if (AbilityText)
	{
		AbilityText->SetText(ResolveAbilityDisplayName());
	}
}

void UAbilitySlotWidget::SetAbilitySlotEnabled(bool bEnabled)
{
	bAbilitySlotEnabled = bEnabled;
	ApplyAbilitySlotEnabledState();
}

void UAbilitySlotWidget::SetAbilityImage()
{
	if (AbilityImage)
	{
		AbilityImage->SetBrush(MakeImageBrush(ResolveAbilityImage()));
	}
}

void UAbilitySlotWidget::SetInputKeyIcon()
{
	if (!InputKeyOverlay)
	{
		return;
	}

	if (bHideInputKeyIcon)
	{
		InputKeyOverlay->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UObject* IconObject = ResolveInputIconObject();
	if (!IconObject)
	{
		InputKeyOverlay->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	InputKeyOverlay->SetVisibility(ESlateVisibility::Visible);
	if (UImage* IconImage = KeyIcon ? KeyIcon.Get() : InputKeyIcon.Get())
	{
		IconImage->SetBrush(MakeImageBrushFromExisting(IconImage->GetBrush(), IconObject, InputKeyIconSize));
	}
}

void UAbilitySlotWidget::CheckForCooldown()
{
	if (!bAbilitySlotEnabled)
	{
		ClearCooldownTimer();
		if (CooldownTimerContainer)
		{
			CooldownTimerContainer->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (!AbilityObjectRef)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (World->GetTimerManager().IsTimerActive(UpdateCooldownTimerHandle))
		{
			return;
		}
	}

	TotalCooldownTime = AbilityObjectRef->GetCooldownTimeRemaining();
	if (TotalCooldownTime <= 0.0)
	{
		return;
	}

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

void UAbilitySlotWidget::CheckForActivation()
{
	if (!bAbilitySlotEnabled)
	{
		if (AbilityActiveFrame)
		{
			AbilityActiveFrame->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const bool bIsActive = AbilityObjectRef && UAbilitySystemBlueprintLibrary::IsGameplayAbilityActive(AbilityObjectRef);
	if (AbilityActiveFrame)
	{
		AbilityActiveFrame->SetVisibility(bIsActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (InputKeyBackground)
	{
		InputKeyBackground->SetColorAndOpacity(bIsActive ? ActiveInputKeyColor : InactiveInputKeyColor);
	}
}

void UAbilitySlotWidget::UpdateCooldownProgress()
{
	if (!bAbilitySlotEnabled)
	{
		ClearCooldownTimer();
		return;
	}

	if (!AbilityObjectRef)
	{
		return;
	}

	const float TimeRemaining = AbilityObjectRef->GetCooldownTimeRemaining();
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
	}
}

void UAbilitySlotWidget::InitializeAbilityObject()
{
	AbilityObjectRef = nullptr;

	APawn* OwningPawn = GetOwningPlayerPawn();
	UAbilitySystemComponent* AbilitySystemComponent = OwningPawn
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn)
		: nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (!AbilitySpecHandle.IsValid())
	{
		return;
	}

	CachedAbilitySystemComponent = AbilitySystemComponent;

	bool bIsInstance = false;
	const UGameplayAbility* GameplayAbility = UAbilitySystemBlueprintLibrary::GetGameplayAbilityFromSpecHandle(
		AbilitySystemComponent,
		AbilitySpecHandle,
		bIsInstance);

	AbilityObjectRef = const_cast<UGameplayAbility*>(GameplayAbility);
}

void UAbilitySlotWidget::BindGameplayTagEvents()
{
	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag CooldownTag = LabGameplayTags::Cooldown;
	if (CooldownTag.IsValid())
	{
		CooldownTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &ThisClass::HandleCooldownTagChanged);
	}

	const FGameplayTag GameplayAbilityTag = LabGameplayTags::GameplayAbility;
	if (GameplayAbilityTag.IsValid())
	{
		GameplayAbilityTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(GameplayAbilityTag, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &ThisClass::HandleGameplayAbilityTagChanged);
	}
}

void UAbilitySlotWidget::UnbindGameplayTagEvents()
{
	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	if (!AbilitySystemComponent)
	{
		CachedAbilitySystemComponent.Reset();
		return;
	}

	const FGameplayTag CooldownTag = LabGameplayTags::Cooldown;
	if (CooldownTag.IsValid() && CooldownTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::AnyCountChange)
			.Remove(CooldownTagChangedHandle);
		CooldownTagChangedHandle.Reset();
	}

	const FGameplayTag GameplayAbilityTag = LabGameplayTags::GameplayAbility;
	if (GameplayAbilityTag.IsValid() && GameplayAbilityTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(GameplayAbilityTag, EGameplayTagEventType::AnyCountChange)
			.Remove(GameplayAbilityTagChangedHandle);
		GameplayAbilityTagChangedHandle.Reset();
	}

	CachedAbilitySystemComponent.Reset();
}

void UAbilitySlotWidget::ClearCooldownTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateCooldownTimerHandle);
	}

	UpdateCooldownTimerHandle.Invalidate();
}

void UAbilitySlotWidget::HandleCooldownTagChanged(FGameplayTag CallbackTag, int32 NewCount)
{
	static_cast<void>(CallbackTag);
	static_cast<void>(NewCount);
	CheckForCooldown();
}

void UAbilitySlotWidget::HandleGameplayAbilityTagChanged(FGameplayTag CallbackTag, int32 NewCount)
{
	static_cast<void>(CallbackTag);
	static_cast<void>(NewCount);
	CheckForActivation();
}

void UAbilitySlotWidget::SetInputKeyRenderOpacity(float InOpacity) const
{
	if (InputKeyOverlay)
	{
		InputKeyOverlay->SetRenderOpacity(InOpacity);
	}
}

void UAbilitySlotWidget::ApplyAbilitySlotEnabledState()
{
	const float ContentOpacity = bAbilitySlotEnabled ? 1.0f : DisabledSlotOpacity;
	if (AbilityImage)
	{
		AbilityImage->SetRenderOpacity(ContentOpacity);
	}

	if (AbilityText)
	{
		AbilityText->SetRenderOpacity(ContentOpacity);
	}

	if (InputKeyOverlay)
	{
		InputKeyOverlay->SetRenderOpacity(bAbilitySlotEnabled ? ReadyInputKeyOpacity : DisabledSlotOpacity);
	}

	if (!bAbilitySlotEnabled)
	{
		ClearCooldownTimer();
		if (CooldownTimerContainer)
		{
			CooldownTimerContainer->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (AbilityActiveFrame)
		{
			AbilityActiveFrame->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

UObject* UAbilitySlotWidget::ResolveAbilityImage() const
{
	if (AbilityIconOverride)
	{
		return AbilityIconOverride;
	}

	if (const FSkill* PandoraSkill = ResolvePandoraSkill())
	{
		if (UObject* SkillIcon = PandoraSkill->GetIconResource())
		{
			return SkillIcon;
		}
	}

	if (const UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get())
	{
		const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle);
		const UObject* SourceObject = AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr;
		const UPandoraSkillRuntimeContext* RuntimeContext = Cast<UPandoraSkillRuntimeContext>(SourceObject);
		const USkillDataAsset* SkillDataAsset = RuntimeContext ? RuntimeContext->GetSkillDataAsset() : Cast<USkillDataAsset>(SourceObject);
		if (SkillDataAsset)
		{
			if (UObject* SkillIcon = SkillDataAsset->GetIconResource())
			{
				return SkillIcon;
			}
		}
	}

	return DefaultAbilityImage.Get();
}

FText UAbilitySlotWidget::ResolveAbilityDisplayName() const
{
	if (bHasAbilityDisplayOverride && !AbilityDisplayNameOverride.IsEmpty())
	{
		return AbilityDisplayNameOverride;
	}

	if (const FSkill* PandoraSkill = ResolvePandoraSkill())
	{
		const FText SkillDisplayName = PandoraSkill->GetDisplayName();
		if (!SkillDisplayName.IsEmpty())
		{
			return SkillDisplayName;
		}
	}

	if (const UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get())
	{
		const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle);
		const UObject* SourceObject = AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr;
		const UPandoraSkillRuntimeContext* RuntimeContext = Cast<UPandoraSkillRuntimeContext>(SourceObject);
		const USkillDataAsset* SkillDataAsset = RuntimeContext ? RuntimeContext->GetSkillDataAsset() : Cast<USkillDataAsset>(SourceObject);
		if (SkillDataAsset)
		{
			const FText SkillDisplayName = SkillDataAsset->GetDisplayName();
			if (!SkillDisplayName.IsEmpty())
			{
				return SkillDisplayName;
			}
		}
	}

	return AbilityObjectRef ? FText::FromString(AbilityObjectRef->GetClass()->GetName()) : FText::GetEmpty();
}

const FSkill* UAbilitySlotWidget::ResolvePandoraSkill() const
{
	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	if (!AbilitySystemComponent)
	{
		return nullptr;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle);
	const UPandoraSkillRuntimeContext* RuntimeContext = AbilitySpec
		? Cast<UPandoraSkillRuntimeContext>(AbilitySpec->SourceObject.Get())
		: nullptr;
	return RuntimeContext ? RuntimeContext->GetPandoraSkill() : nullptr;
}

UInputAction* UAbilitySlotWidget::ResolveInputAction() const
{
	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent ? AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle) : nullptr;
	if (!AbilitySpec)
	{
		return nullptr;
	}

	const FGameplayTagContainer& SourceTags = AbilitySpec->GetDynamicSpecSourceTags();
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill1))
	{
		return Skill1InputAction.Get();
	}
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill2))
	{
		return Skill2InputAction.Get();
	}
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill3))
	{
		return Skill3InputAction.Get();
	}
	if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill4))
	{
		return Skill4InputAction.Get();
	}

	return nullptr;
}

UObject* UAbilitySlotWidget::ResolveInputIconObject() const
{
	const UInputAction* InputAction = ResolveInputAction();
	APlayerController* PlayerController = GetOwningPlayer();
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!InputAction || !InputSubsystem)
	{
		return nullptr;
	}

	const auto FindIcon = [this](const FString& Candidate) -> UObject*
	{
		if (const TObjectPtr<UObject>* IconObject = StringToIconMapping.Find(Candidate))
		{
			return IconObject->Get();
		}

		for (const TPair<FString, TObjectPtr<UObject>>& Pair : StringToIconMapping)
		{
			if (Pair.Key.Equals(Candidate, ESearchCase::IgnoreCase))
			{
				return Pair.Value.Get();
			}
		}

		return nullptr;
	};

	const TArray<FKey> MappedKeys = InputSubsystem->QueryKeysMappedToAction(InputAction);
	for (const FKey& MappedKey : MappedKeys)
	{
		TArray<FString> Candidates;
		Candidates.Add(MappedKey.GetDisplayName(false).ToString());
		Candidates.Add(MappedKey.GetDisplayName(true).ToString());
		Candidates.Add(MappedKey.GetFName().ToString());

		const int32 InitialCandidateCount = Candidates.Num();
		for (int32 Index = 0; Index < InitialCandidateCount; ++Index)
		{
			Candidates.Add(Candidates[Index].Replace(TEXT(" "), TEXT("")));
		}

		for (const FString& Candidate : Candidates)
		{
			if (UObject* IconObject = FindIcon(Candidate))
			{
				return IconObject;
			}
		}
	}

	return nullptr;
}

FSlateBrush UAbilitySlotWidget::MakeImageBrush(UObject* ResourceObject)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = FVector2D(64.0f, 64.0f);
	Brush.SetResourceObject(ResourceObject);
	return Brush;
}

FSlateBrush UAbilitySlotWidget::MakeImageBrushFromExisting(const FSlateBrush& ExistingBrush, UObject* ResourceObject, FVector2D ImageSize)
{
	FSlateBrush Brush = ExistingBrush;
	if (ImageSize.X > 0.0f && ImageSize.Y > 0.0f)
	{
		Brush.ImageSize = ImageSize;
	}
	Brush.SetResourceObject(ResourceObject);
	return Brush;
}

float UAbilitySlotWidget::CalculateCooldownPercent(float TimeRemaining, double CooldownDuration)
{
	if (CooldownDuration <= UE_SMALL_NUMBER)
	{
		return 1.0f;
	}

	return FMath::Clamp(1.0f - static_cast<float>(TimeRemaining / CooldownDuration), 0.0f, 1.0f);
}
