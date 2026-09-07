#include "UI/Widget/AbilitiesBarWidget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/AbilitySystem/SkillTypes.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Common/LabGameplayTags.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Character/CharacterBase.h"
#include "GameFramework/Pawn.h"
#include "Definition/Item/ItemDefinition.h"
#include "Mode/PdPlayerState.h"
#include "Mode/PdPlayerController.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "InputAction.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "Component/Player/EquipmentComponent.h"
#include "TimerManager.h"
#include "UI/Widget/AbilitySlotWidget.h"
#include "UI/Widget/InputKeyIconResolver.h"
#include "UI/WidgetLookup.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilitiesBarWidget)

namespace
{
	const FName AbilitySpecHandlePropertyName(TEXT("AbilitySpecHandle"));

	const USkillDefinition* ResolveSourceSkillDataAsset(const FGameplayAbilitySpec* AbilitySpec)
	{
		if (!AbilitySpec)
		{
			return nullptr;
		}

		if (const UPandoraSkillRuntimeContext* RuntimeContext = Cast<UPandoraSkillRuntimeContext>(AbilitySpec->SourceObject.Get()))
		{
			return RuntimeContext->GetSkillDataAsset();
		}

		if (const USkillDefinition* SkillDataAsset = Cast<USkillDefinition>(AbilitySpec->SourceObject.Get()))
		{
			return SkillDataAsset;
		}

		return nullptr;
	}
}

void UAbilitiesBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!IsDesignTime() || !ContainerHorizontalBox)
	{
		return;
	}

	ContainerHorizontalBox->ClearChildren();
	for (int32 SlotIndex = 0; SlotIndex < MinimumSlots; ++SlotIndex)
	{
		AddEmptySlot(true, SlotIndex);
	}
}

void UAbilitiesBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeAbilitySystemBinding();
}

void UAbilitiesBarWidget::NativeDestruct()
{
	UnbindAbilitiesChangedEvents();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebuildBarTimerHandle);
		World->GetTimerManager().ClearTimer(RetryInitializeTimerHandle);
	}

	Super::NativeDestruct();
}

TArray<FGameplayAbilitySpecHandle> UAbilitiesBarWidget::GetAbilitiesToShowInBar() const
{
	TArray<FGameplayAbilitySpecHandle> AbilitiesToShow;

	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	if (!AbilitySystemComponent)
	{
		AbilitySystemComponent = GetOwningAbilitySystemComponent();
	}

	if (!AbilitySystemComponent)
	{
		return AbilitiesToShow;
	}

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	AbilitySystemComponent->GetAllAbilities(AbilityHandles);

	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilityHandles)
	{
		if (ShouldShowAbilityHandle(AbilitySystemComponent, AbilitySpecHandle))
		{
			AbilitiesToShow.AddUnique(AbilitySpecHandle);
		}
	}

	return AbilitiesToShow;
}

void UAbilitiesBarWidget::FillAbilitiesBar()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebuildBarTimerHandle);
		RebuildBarTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			this,
			&ThisClass::RebuildAbilitiesBar);
		return;
	}

	RebuildAbilitiesBar();
}

void UAbilitiesBarWidget::InitializeAbilitySystemBinding()
{
	UnbindAbilitiesChangedEvents();

	UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RetryInitializeTimerHandle,
				this,
				&ThisClass::InitializeAbilitySystemBinding,
				0.1f,
				false);
		}
		return;
	}

	CachedAbilitySystemComponent = AbilitySystemComponent;
	BindAbilitiesChangedEvents();
	FillAbilitiesBar();
}

void UAbilitiesBarWidget::RebuildAbilitiesBar()
{
	RebuildBarTimerHandle.Invalidate();
	BindPandoraTreeChangedEvent();

	if (!ContainerHorizontalBox)
	{
		return;
	}

	ContainerHorizontalBox->ClearChildren();

	if (const UPandoraDefinition* SelectedPandoraDefinition = GetSelectedPandoraDefinition())
	{
		const bool bSelectedPandoraEnabled = IsSelectedPandoraCompatibleWithCurrentWeapon();
		UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
		if (!AbilitySystemComponent)
		{
			AbilitySystemComponent = GetOwningAbilitySystemComponent();
		}

		const int32 NumPandoraSkillSlots = FMath::Clamp(PandoraSkillSlots, 0, UPandoraDefinition::GetFixedMaxLevel());
		const int32 SelectedPandoraLevel = GetSelectedPandoraLevel(SelectedPandoraDefinition);

		for (int32 SlotIndex = 0; SlotIndex < NumPandoraSkillSlots; ++SlotIndex)
		{
				if (!SelectedPandoraDefinition->Skill.IsValidIndex(SlotIndex))
			{

				AddEmptySlot(true, SlotIndex);
				continue;
			}

			const FSkill& Skill = SelectedPandoraDefinition->Skill[SlotIndex];
			if (!Skill.ShouldShowInAbilitiesBar() || !IsConfiguredPandoraSkill(Skill))
			{

				AddEmptySlot(true, SlotIndex);
				continue;
			}

			FAbilityBarSlotData SlotData;
			SlotData.SkillSlotIndex = SlotIndex;
			const bool bSkillSlotUnlocked = SelectedPandoraDefinition->IsSkillSlotUnlocked(SlotIndex, SelectedPandoraLevel);
			SlotData.bEnabled = bSelectedPandoraEnabled && bSkillSlotUnlocked;
			if (SlotData.bEnabled)
			{
				SlotData.AbilitySpecHandle = FindAbilitySpecHandleForSkill(
					AbilitySystemComponent,
					SelectedPandoraDefinition,
					SlotIndex);
				if (!SlotData.AbilitySpecHandle.IsValid())
				{

					SlotData.bEnabled = false;
				}
			}

			SlotData.DisplayNameOverride = Skill.GetDisplayName();
			SlotData.IconOverride = Skill.GetIconResource();
			SlotData.bHasDisplayOverride = true;

			AddAbilitySlot(SlotData);
		}

		return;
	}

	int32 SkillSlotIndex = 0;
	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : GetAbilitiesToShowInBar())
	{
		AddAbilitySlot(AbilitySpecHandle, SkillSlotIndex++);
	}

	const int32 CurrentChildrenCount = ContainerHorizontalBox->GetChildrenCount();
	for (int32 EmptySlotIndex = CurrentChildrenCount; EmptySlotIndex < MinimumSlots; ++EmptySlotIndex)
	{
		AddEmptySlot(true, EmptySlotIndex);
	}
}

void UAbilitiesBarWidget::AddAbilitySlot(const FGameplayAbilitySpecHandle& AbilitySpecHandle)
{
	AddAbilitySlot(AbilitySpecHandle, INDEX_NONE);
}

void UAbilitiesBarWidget::AddAbilitySlot(const FGameplayAbilitySpecHandle& AbilitySpecHandle, const int32 SkillSlotIndex)
{
	FAbilityBarSlotData SlotData;
	SlotData.AbilitySpecHandle = AbilitySpecHandle;
	SlotData.SkillSlotIndex = SkillSlotIndex;
	AddAbilitySlot(SlotData);
}

void UAbilitiesBarWidget::AddAbilitySlot(const FAbilityBarSlotData& SlotData)
{
	UUserWidget* AbilityWidget = CreateBarWidget(AbilityWidgetClass);
	if (!AbilityWidget)
	{
		return;
	}

	if (UAbilitySlotWidget* AbilitySlotWidget = Cast<UAbilitySlotWidget>(AbilityWidget))
	{
		AbilitySlotWidget->SetSkillSlotIndex(SlotData.SkillSlotIndex);
		if (SlotData.bHasDisplayOverride)
		{
			AbilitySlotWidget->SetAbilitySlotDataEnabled(
				SlotData.AbilitySpecHandle,
				SlotData.DisplayNameOverride,
				SlotData.IconOverride,
				SlotData.bEnabled);
		}
		else
		{
			AbilitySlotWidget->SetAbilitySpecHandle(SlotData.AbilitySpecHandle);
			AbilitySlotWidget->SetAbilitySlotEnabled(SlotData.bEnabled);
		}
	}
	else
	{
		SetAbilitySpecHandleOnWidget(AbilityWidget, SlotData.AbilitySpecHandle);
	}

	ApplySkillSlotKeyIcon(AbilityWidget, SlotData.SkillSlotIndex);
	AddWidgetToBar(AbilityWidget, true);
}

void UAbilitiesBarWidget::AddEmptySlot(const bool bApplyPadding, const int32 SkillSlotIndex)
{
	UUserWidget* EmptySlotWidget = CreateBarWidget(EmptyAbilityWidgetClass);
	ApplySkillSlotKeyIcon(EmptySlotWidget, SkillSlotIndex);
	AddWidgetToBar(EmptySlotWidget, bApplyPadding);
}

UUserWidget* UAbilitiesBarWidget::CreateBarWidget(TSubclassOf<UUserWidget> WidgetClass) const
{
	if (!WidgetClass)
	{
		return nullptr;
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		return CreateWidget<UUserWidget>(OwningPlayer, WidgetClass);
	}

	return CreateWidget<UUserWidget>(const_cast<UAbilitiesBarWidget*>(this), WidgetClass);
}

void UAbilitiesBarWidget::AddWidgetToBar(UUserWidget* Widget, bool bApplyPadding) const
{
	if (!ContainerHorizontalBox || !Widget)
	{
		return;
	}

	UHorizontalBoxSlot* HorizontalBoxSlot = ContainerHorizontalBox->AddChildToHorizontalBox(Widget);
	if (HorizontalBoxSlot && bApplyPadding)
	{
		HorizontalBoxSlot->SetPadding(SlotPadding);
	}
}

void UAbilitiesBarWidget::SetAbilitySpecHandleOnWidget(UUserWidget* Widget, const FGameplayAbilitySpecHandle& AbilitySpecHandle) const
{
	if (!Widget)
	{
		return;
	}

	if (UAbilitySlotWidget* AbilitySlotWidget = Cast<UAbilitySlotWidget>(Widget))
	{
		AbilitySlotWidget->SetAbilitySpecHandle(AbilitySpecHandle);
		return;
	}

	FProperty* Property = FindPropertyByExactNameOrPrefix(Widget->GetClass(), AbilitySpecHandlePropertyName, TEXT("AbilitySpecHandle"));
	FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	if (!StructProperty || StructProperty->Struct != FGameplayAbilitySpecHandle::StaticStruct())
	{
		return;
	}

	void* PropertyValue = StructProperty->ContainerPtrToValuePtr<void>(Widget);
	StructProperty->CopyCompleteValue(PropertyValue, &AbilitySpecHandle);
}

void UAbilitiesBarWidget::ApplySkillSlotKeyIcon(UUserWidget* Widget, const int32 SkillSlotIndex) const
{
	if (!Widget)
	{
		return;
	}

	if (UAbilitySlotWidget* AbilitySlotWidget = Cast<UAbilitySlotWidget>(Widget))
	{
		AbilitySlotWidget->SetSkillSlotIndex(SkillSlotIndex);
		return;
	}

	UImage* KeyIcon = PdWidgetLookup::FindWidgetByNames<UImage>(Widget, {
		TEXT("KeyIcon")
	});
	if (!KeyIcon)
	{
		return;
	}

	const APdPlayerController* PlayerController =
		Cast<APdPlayerController>(GetOwningPlayer());
	const UControllerInputDefinition* InputDefinition = PlayerController
		? PlayerController->GetLoadedInputDefinition()
		: nullptr;
	const UInputAction* InputAction = InputDefinition
		? InputDefinition->GetLoadedSkillInputAction(SkillSlotIndex)
		: nullptr;

	UObject* IconObject = PdInputKeyIconResolver::ResolveInputDefinitionIconObject(
		GetOwningPlayer(),
		InputAction);
	if (!IconObject)
	{
		KeyIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	KeyIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	KeyIcon->SetBrush(PdInputKeyIconResolver::MakeImageBrushFromExisting(
		KeyIcon->GetBrush(),
		IconObject,
		KeyIcon->GetBrush().ImageSize));
}

bool UAbilitiesBarWidget::ShouldShowAbilityHandle(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAbilitySpecHandle& AbilitySpecHandle) const
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle);
	const USkillDefinition* SourceSkill = ResolveSourceSkillDataAsset(AbilitySpec);
	return SourceSkill && SourceSkill->ShouldShowInAbilitiesBar()
		&& !AbilitySpec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora);
}

UAbilitySystemComponent* UAbilitiesBarWidget::GetOwningAbilitySystemComponent() const
{
	APawn* OwningPawn = GetOwningPlayerPawn();
	return OwningPawn ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwningPawn) : nullptr;
}

const UPandoraDefinition* UAbilitiesBarWidget::GetSelectedPandoraDefinition() const
{
	const APdPlayerState* PlayerState = nullptr;
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

	const UPandoraComponent* PandoraComponent = PlayerState ? PlayerState->GetPandoraComponent() : nullptr;
	return PandoraComponent ? PandoraComponent->GetCurrentPandoraDefinition() : nullptr;
}

UPandoraTreeComponent* UAbilitiesBarWidget::GetPandoraTreeComponent() const
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

	return PlayerState ? PlayerState->GetPandoraTreeComponent() : nullptr;
}

int32 UAbilitiesBarWidget::GetSelectedPandoraLevel(const UPandoraDefinition* PandoraDefinition) const
{
	if (!PandoraDefinition)
	{
		return 1;
	}

	const UPandoraTreeComponent* PandoraTreeComponent = GetPandoraTreeComponent();
	const int32 CurrentLevel = PandoraTreeComponent
		? PandoraTreeComponent->GetCurrentPandoraLevel(const_cast<UPandoraDefinition*>(PandoraDefinition))
		: 0;
	return FMath::Clamp(CurrentLevel, 0, PandoraDefinition->GetMaxLevel());
}

bool UAbilitiesBarWidget::IsSelectedPandoraCompatibleWithCurrentWeapon() const
{
	const UPandoraDefinition* SelectedPandoraDefinition = GetSelectedPandoraDefinition();
	return !SelectedPandoraDefinition || SelectedPandoraDefinition->IsCompatibleWithWeaponDefinition(GetCurrentWeaponDefinition());
}

const UItemDefinition* UAbilitiesBarWidget::GetCurrentWeaponDefinition() const
{
	const ACharacterBase* OwningCharacter = Cast<ACharacterBase>(GetOwningPlayerPawn());
	const UEquipmentComponent* EquipmentComponent = OwningCharacter ? OwningCharacter->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponDefinition() : nullptr;
}

FGameplayAbilitySpecHandle UAbilitiesBarWidget::FindAbilitySpecHandleForSkill(
	UAbilitySystemComponent* AbilitySystemComponent,
	const UPandoraDefinition* PandoraDefinition,
	int32 SkillIndex) const
{
	if (!AbilitySystemComponent
		|| !PandoraDefinition
		|| SkillIndex < 0
		|| SkillIndex >= UPandoraDefinition::GetFixedMaxLevel())
	{
		return FGameplayAbilitySpecHandle();
	}

	FGameplayTag SkillInputTag;
	switch (SkillIndex)
	{
	case 0:
		SkillInputTag = LabGameplayTags::Input_Ability_Skill1;
		break;
	case 1:
		SkillInputTag = LabGameplayTags::Input_Ability_Skill2;
		break;
	case 2:
		SkillInputTag = LabGameplayTags::Input_Ability_Skill3;
		break;
	default:
		break;
	}

	if (!SkillInputTag.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	AbilitySystemComponent->GetAllAbilities(AbilityHandles);
	for (const FGameplayAbilitySpecHandle& AbilityHandle : AbilityHandles)
	{
		const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilityHandle);
		if (!AbilitySpec)
		{
			continue;
		}

		const UPandoraSkillRuntimeContext* RuntimeContext = Cast<UPandoraSkillRuntimeContext>(AbilitySpec->SourceObject.Get());
		if (RuntimeContext
			&& RuntimeContext->GetPandoraDefinition() == PandoraDefinition
			&& RuntimeContext->GetSkillIndex() == SkillIndex
			&& AbilitySpec->GetDynamicSpecSourceTags().HasTagExact(SkillInputTag))
		{
			return AbilityHandle;
		}

	}

	return FGameplayAbilitySpecHandle();
}

bool UAbilitiesBarWidget::IsConfiguredPandoraSkill(const FSkill& Skill) const
{
	return !Skill.GetAbilitiesToGrant().IsEmpty()
		|| Skill.GetIconResource() != nullptr
		|| !Skill.GetDisplayName().IsEmpty();
}

void UAbilitiesBarWidget::BindAbilitiesChangedEvents()
{
	BindPandoraTreeChangedEvent();

	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (UPdAbilitySystemComponent* PdAbilitySystemComponent = Cast<UPdAbilitySystemComponent>(AbilitySystemComponent))
	{
		AbilitiesChangedNativeHandle = PdAbilitySystemComponent->OnAbilitiesChangedNative.AddUObject(this, &ThisClass::HandleAbilitiesChanged);
		return;
	}

	// Non-project ability system components do not expose the native delegate.
	// Keep the gameplay event as a fallback, but never subscribe to both paths.
	const FGameplayTag AbilitiesChangedTag = LabGameplayTags::Event_Abilities_Changed;
	if (AbilitiesChangedTag.IsValid())
	{
		AbilitiesChangedEventHandle = AbilitySystemComponent->GenericGameplayEventCallbacks
			.FindOrAdd(AbilitiesChangedTag)
			.AddUObject(this, &ThisClass::HandleAbilitiesChangedEvent);
	}
}

void UAbilitiesBarWidget::UnbindAbilitiesChangedEvents()
{
	UnbindPandoraTreeChangedEvent();

	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	const FGameplayTag AbilitiesChangedTag = LabGameplayTags::Event_Abilities_Changed;
	if (AbilitySystemComponent && AbilitiesChangedTag.IsValid() && AbilitiesChangedEventHandle.IsValid())
	{
		AbilitySystemComponent->GenericGameplayEventCallbacks
			.FindOrAdd(AbilitiesChangedTag)
			.Remove(AbilitiesChangedEventHandle);
	}

	if (UPdAbilitySystemComponent* PdAbilitySystemComponent = Cast<UPdAbilitySystemComponent>(AbilitySystemComponent))
	{
		if (AbilitiesChangedNativeHandle.IsValid())
		{
			PdAbilitySystemComponent->OnAbilitiesChangedNative.Remove(AbilitiesChangedNativeHandle);
		}
	}

	AbilitiesChangedEventHandle.Reset();
	AbilitiesChangedNativeHandle.Reset();
	CachedAbilitySystemComponent.Reset();
}

void UAbilitiesBarWidget::BindPandoraTreeChangedEvent()
{
	UPandoraTreeComponent* PandoraTreeComponent = GetPandoraTreeComponent();
	if (BoundPandoraTreeComponent.Get() == PandoraTreeComponent)
	{
		return;
	}

	UnbindPandoraTreeChangedEvent();
	if (!PandoraTreeComponent)
	{
		return;
	}

	PandoraTreeComponent->OnPandorasChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandlePandoraTreeChanged);
	BoundPandoraTreeComponent = PandoraTreeComponent;
}

void UAbilitiesBarWidget::UnbindPandoraTreeChangedEvent()
{
	if (UPandoraTreeComponent* PandoraTreeComponent =
		BoundPandoraTreeComponent.Get())
	{
		PandoraTreeComponent->OnPandorasChanged.RemoveDynamic(
			this,
			&ThisClass::HandlePandoraTreeChanged);
	}
	BoundPandoraTreeComponent.Reset();
}

void UAbilitiesBarWidget::HandleAbilitiesChanged()
{
	FillAbilitiesBar();
}

void UAbilitiesBarWidget::HandleAbilitiesChangedEvent(const FGameplayEventData* Payload)
{
	static_cast<void>(Payload);
	FillAbilitiesBar();
}

void UAbilitiesBarWidget::HandlePandoraTreeChanged()
{
	FillAbilitiesBar();
}

FProperty* UAbilitiesBarWidget::FindPropertyByExactNameOrPrefix(UStruct* Struct, FName ExactName, const FString& Prefix)
{
	if (!Struct)
	{
		return nullptr;
	}

	if (FProperty* ExactProperty = FindFProperty<FProperty>(Struct, ExactName))
	{
		return ExactProperty;
	}

	for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (Property && Property->GetName().StartsWith(Prefix))
		{
			return Property;
		}
	}

	return nullptr;
}
