#include "UI/Widget/AbilitySlotWidget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Abilities/GameplayAbility.h"
#include "Common/LabGameplayTags.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "InputAction.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "GameFramework/PlayerController.h"
#include "Mode/PdPlayerState.h"
#include "Mode/PdPlayerController.h"
#include "Pandora/PandoraSkillSource.h"
#include "TimerManager.h"
#include "UI/Widget/InputKeyIconResolver.h"
#include "Definition/UI/WidgetClassDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilitySlotWidget)

void UAbilitySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyWidgetDefinitionSettings();
	SetAbilityImage();
	SetInputKeyRenderOpacity(ReadyInputKeyOpacity);

	if (InputKeyOverlay)
	{
		InputKeyOverlay->SetVisibility(bHideInputKeyIcon ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
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

	if (AbilityDisableFrame)
	{
		AbilityDisableFrame->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (TimerText)
	{
		TimerText->SetText(FText::GetEmpty());
	}

	ApplyAbilitySlotEnabledState();

	RefreshAbilityBinding();

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
	RefreshAbilityBinding();
	SetAbilityImage();

	SetInputKeyIcon();
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
	static_cast<void>(InDisplayNameOverride);
	AbilityIconOverride = InIconOverride;

	SetAbilityImage();
}

void UAbilitySlotWidget::SetAbilitySlotEnabled(bool bEnabled)
{
	bAbilitySlotEnabled = bEnabled;
	ApplyAbilitySlotEnabledState();
}

void UAbilitySlotWidget::SetSkillSlotIndex(const int32 InSkillSlotIndex)
{
	SkillSlotIndex = InSkillSlotIndex;
	SetInputKeyIcon();
	CheckForManaAvailability();
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
		return;
	}

	UObject* IconObject = ResolveInputIconObject();
	if (!IconObject)
	{
		InputKeyOverlay->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	InputKeyOverlay->SetVisibility(ESlateVisibility::Visible);
	KeyIcon->SetBrush(MakeImageBrushFromExisting(KeyIcon->GetBrush(), IconObject, InputKeyIconSize));
}

void UAbilitySlotWidget::CheckForCooldown()
{
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
		InputKeyBackground->SetBrushTintColor(FSlateColor(bIsActive ? ActiveInputKeyColor : InactiveInputKeyColor));
	}
}

void UAbilitySlotWidget::CheckForManaAvailability()
{
	if (!AbilityDisableFrame)
	{
		return;
	}

	const USkillDefinition* SkillDataAsset = ResolveSkillDataAsset();
	const double ManaCost = SkillDataAsset ? FMath::Max(SkillDataAsset->ManaCost, 0.0) : 0.0;
	const UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	const bool bHasManaCostData = bAbilitySlotEnabled && AbilitySystemComponent && ManaCost > 0.0;
	const double CurrentMana = bHasManaCostData
		? static_cast<double>(AbilitySystemComponent->GetNumericAttribute(UBasicAttributeSet::GetManaAttribute()))
		: 0.0;
	const bool bInsufficientMana = bHasManaCostData && CurrentMana + UE_SMALL_NUMBER < ManaCost;

	AbilityDisableFrame->SetVisibility(
		bInsufficientMana ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UAbilitySlotWidget::UpdateCooldownProgress()
{
	float TimeRemaining = 0.0f;
	float Duration = 0.0f;
	if (bAbilitySlotEnabled)
	{
		ResolveCooldown(TimeRemaining, Duration);
	}

	if (TimeRemaining <= 0.0f)
	{
		ClearCooldownTimer();
		TotalCooldownTime = 0.0;

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

		SetInputKeyRenderOpacity(bAbilitySlotEnabled ? ReadyInputKeyOpacity : DisabledSlotOpacity);
		return;
	}

	// 위젯 재생성이나 판도라 재연결 시에도 타이머 유무와 관계없이 현재 쿨다운 표시를 복원한다.
	TotalCooldownTime = FMath::Max(Duration, TimeRemaining);
	if (UWorld* World = GetWorld(); World && !World->GetTimerManager().IsTimerActive(UpdateCooldownTimerHandle))
	{
		World->GetTimerManager().SetTimer(UpdateCooldownTimerHandle, this, &ThisClass::UpdateCooldownProgress, 0.05f, true);
	}
	if (CooldownTimerContainer)
	{
		CooldownTimerContainer->SetVisibility(ESlateVisibility::Visible);
	}
	SetInputKeyRenderOpacity(CooldownInputKeyOpacity);

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

void UAbilitySlotWidget::ApplyWidgetDefinitionSettings()
{
	const UWidgetClassDefinition* WidgetDefinition = UWidgetClassDefinition::ResolveWidgetClassDefinition(this);
	if (!WidgetDefinition)
	{
		return;
	}

	const FAbilitySlotWidgetSettings& Settings = WidgetDefinition->GetAbilitySlotWidgetSettings();
	DisabledSlotOpacity = Settings.DisabledSlotOpacity;
	bShowCooldownTimeRemaining = Settings.bShowCooldownTimeRemaining;
}

void UAbilitySlotWidget::RefreshAbilityBinding()
{
	ClearCooldownTimer();
	UnbindGameplayTagEvents();
	InitializeAbilityObject();
	BindGameplayTagEvents();
	CheckForCooldown();
	CheckForActivation();
	CheckForManaAvailability();
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
		BoundCooldownTag = CooldownTag;
		CooldownTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &ThisClass::HandleCooldownTagChanged);
	}

	const FGameplayTag GameplayAbilityTag = LabGameplayTags::GameplayAbility;
	if (GameplayAbilityTag.IsValid())
	{
		GameplayAbilityTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(GameplayAbilityTag, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &ThisClass::HandleGameplayAbilityTagChanged);
	}

	ManaChangedHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetManaAttribute())
		.AddUObject(this, &ThisClass::HandleManaChanged);
}

void UAbilitySlotWidget::UnbindGameplayTagEvents()
{
	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	if (!AbilitySystemComponent)
	{
		CachedAbilitySystemComponent.Reset();
		BoundCooldownTag = FGameplayTag();
		CooldownTagChangedHandle.Reset();
		GameplayAbilityTagChangedHandle.Reset();
		ManaChangedHandle.Reset();
		return;
	}

	if (BoundCooldownTag.IsValid() && CooldownTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(BoundCooldownTag, EGameplayTagEventType::AnyCountChange)
			.Remove(CooldownTagChangedHandle);
		CooldownTagChangedHandle.Reset();
	}
	BoundCooldownTag = FGameplayTag();

	const FGameplayTag GameplayAbilityTag = LabGameplayTags::GameplayAbility;
	if (GameplayAbilityTag.IsValid() && GameplayAbilityTagChangedHandle.IsValid())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(GameplayAbilityTag, EGameplayTagEventType::AnyCountChange)
			.Remove(GameplayAbilityTagChangedHandle);
		GameplayAbilityTagChangedHandle.Reset();
	}

	if (ManaChangedHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetManaAttribute())
			.Remove(ManaChangedHandle);
		ManaChangedHandle.Reset();
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

	CheckForCooldown();
}

void UAbilitySlotWidget::HandleGameplayAbilityTagChanged(FGameplayTag CallbackTag, int32 NewCount)
{
	static_cast<void>(CallbackTag);
	static_cast<void>(NewCount);
	CheckForActivation();
}

void UAbilitySlotWidget::HandleManaChanged(const FOnAttributeChangeData& ChangeData)
{
	static_cast<void>(ChangeData);
	CheckForManaAvailability();
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

	if (InputKeyOverlay)
	{
		InputKeyOverlay->SetRenderOpacity(bAbilitySlotEnabled ? ReadyInputKeyOpacity : DisabledSlotOpacity);
	}

	CheckForCooldown();
	CheckForActivation();
	CheckForManaAvailability();
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

	if (const USkillDefinition* SkillDataAsset = ResolveSkillDataAsset())
	{
		if (UObject* SkillIcon = SkillDataAsset->GetIconResource())
		{
			return SkillIcon;
		}
	}

	return DefaultAbilityImage.Get();
}

const FSkill* UAbilitySlotWidget::ResolvePandoraSkill() const
{
	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	if (!AbilitySystemComponent)
	{
		return nullptr;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle);
	const UPandoraSkillSource* SkillSource = AbilitySpec
		? Cast<UPandoraSkillSource>(AbilitySpec->SourceObject.Get())
		: nullptr;
	return SkillSource ? SkillSource->GetPandoraSkill() : nullptr;
}

const USkillDefinition* UAbilitySlotWidget::ResolveSkillDataAsset() const
{
	if (const FSkill* PandoraSkill = ResolvePandoraSkill())
	{
		if (const USkillDefinition* SkillDataAsset = PandoraSkill->SkillDefinition.Get())
		{
			return SkillDataAsset;
		}
	}

	const UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent ? AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle) : nullptr;
	const UObject* SourceObject = AbilitySpec ? AbilitySpec->SourceObject.Get() : nullptr;
	const UPandoraSkillSource* SkillSource = Cast<UPandoraSkillSource>(SourceObject);
	if (SkillSource)
	{
		return SkillSource->GetSkillDataAsset();
	}
	if (const USkillDefinition* SourceSkillDataAsset = Cast<USkillDefinition>(SourceObject))
	{
		return SourceSkillDataAsset;
	}

	const APdPlayerState* PlayerState = nullptr;
	if (const APlayerController* OwningPlayer = GetOwningPlayer())
	{
		PlayerState = OwningPlayer->GetPlayerState<APdPlayerState>();
	}
	if (!PlayerState)
	{
		if (const APawn* OwningPawn = GetOwningPlayerPawn())
		{
			PlayerState = OwningPawn->GetPlayerState<APdPlayerState>();
		}
	}

	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	const UPandoraDefinition* PandoraDefinition = PandoraComponent
		? PandoraComponent->GetCurrentPandoraDefinition()
		: nullptr;
	return PandoraDefinition && PandoraDefinition->Skill.IsValidIndex(SkillSlotIndex)
		? PandoraDefinition->Skill[SkillSlotIndex].SkillDefinition.Get()
		: nullptr;
}

void UAbilitySlotWidget::ResolveCooldown(float& OutTimeRemaining, float& OutDuration) const
{
	OutTimeRemaining = 0.0f;
	OutDuration = 0.0f;
	const UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	if (!AbilityObjectRef || !AbilitySystemComponent || !AbilitySystemComponent->AbilityActorInfo.IsValid()
		|| !AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle))
	{
		return;
	}

	// 능력 인스턴스의 최근 실행 정보가 아닌 슬롯의 Spec으로 적용 당시 효과의 남은 시간과 전체 시간을 함께 읽는다.
	AbilityObjectRef->GetCooldownTimeRemainingAndDuration(
		AbilitySpecHandle, AbilitySystemComponent->AbilityActorInfo.Get(), OutTimeRemaining, OutDuration);
}

UInputAction* UAbilitySlotWidget::ResolveInputAction() const
{
	const APdPlayerController* PlayerController =
		Cast<APdPlayerController>(GetOwningPlayer());
	const UControllerInputDefinition* InputDefinition = PlayerController
		? PlayerController->GetLoadedInputDefinition()
		: nullptr;
	if (!InputDefinition)
	{
		return nullptr;
	}

	int32 ResolvedSkillSlotIndex = SkillSlotIndex;
	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent ? AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle) : nullptr;
	if (AbilitySpec)
	{
		const FGameplayTagContainer& SourceTags =
			AbilitySpec->GetDynamicSpecSourceTags();
		if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill1))
		{
			ResolvedSkillSlotIndex = 0;
		}
		else if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill2))
		{
			ResolvedSkillSlotIndex = 1;
		}
		else if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill3))
		{
			ResolvedSkillSlotIndex = 2;
		}
		else if (SourceTags.HasTagExact(LabGameplayTags::Input_Ability_Skill4))
		{
			ResolvedSkillSlotIndex = 3;
		}
	}

	return InputDefinition->GetLoadedSkillInputAction(ResolvedSkillSlotIndex);
}

UObject* UAbilitySlotWidget::ResolveInputIconObject() const
{
	return PdInputKeyIconResolver::ResolveInputDefinitionIconObject(
		GetOwningPlayer(),
		ResolveInputAction());
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
