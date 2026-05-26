#include "UI/Widget/AbilitiesBarWidget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/Skills/SkillTypes.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Common/LabGameplayTags.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Character/PdCharacterBase.h"
#include "GameFramework/Pawn.h"
#include "Item/ItemDefinition.h"
#include "Mode/PdPlayerState.h"
#include "AbilitySystem/PandoraTree/PandoraTreeComponent.h"
#include "Pandora/PandoraComponent.h"
#include "Pandora/PandoraDefinition.h"
#include "Pandora/PandoraSkillRuntimeContext.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "TimerManager.h"
#include "UI/Widget/AbilitySlotWidget.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilitiesBarWidget)

namespace
{
	const FName AbilitySpecHandlePropertyName(TEXT("AbilitySpecHandle"));
	const FMargin SlotPadding(5.0f, 5.0f, 5.0f, 5.0f);

	const USkillDataAsset* ResolveSourceSkillDataAsset(const FGameplayAbilitySpec* AbilitySpec)
	{
		if (!AbilitySpec)
		{
			return nullptr;
		}

		if (const UPandoraSkillRuntimeContext* RuntimeContext = Cast<UPandoraSkillRuntimeContext>(AbilitySpec->SourceObject.Get()))
		{
			return RuntimeContext->GetSkillDataAsset();
		}

		if (const USkillDataAsset* SkillDataAsset = Cast<USkillDataAsset>(AbilitySpec->SourceObject.Get()))
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
		AddEmptySlot(false);
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
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RebuildAbilitiesBar);
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

		for (int32 SlotIndex = 0; SlotIndex < PandoraSkillSlots; ++SlotIndex)
		{
			if (!SelectedPandoraDefinition->Skill.IsValidIndex(SlotIndex))
			{
				AddEmptySlot(true);
				continue;
			}

			const FSkill& Skill = SelectedPandoraDefinition->Skill[SlotIndex];
			if (!Skill.ShouldShowInAbilitiesBar() || !IsConfiguredPandoraSkill(Skill))
			{
				AddEmptySlot(true);
				continue;
			}

			FAbilityBarSlotData SlotData;
			SlotData.bEnabled = bSelectedPandoraEnabled;
			if (bSelectedPandoraEnabled)
			{
				SlotData.AbilitySpecHandle = FindAbilitySpecHandleForSkill(
					AbilitySystemComponent,
					SelectedPandoraDefinition,
					SlotIndex);
				if (!SlotData.AbilitySpecHandle.IsValid())
				{
					AddEmptySlot(true);
					continue;
				}
			}

			SlotData.DisplayNameOverride = Skill.GetDisplayName();
			SlotData.IconOverride = Skill.GetIconResource();
			SlotData.bHasDisplayOverride = true;
			AddAbilitySlot(SlotData);
		}

		return;
	}

	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : GetAbilitiesToShowInBar())
	{
		AddAbilitySlot(AbilitySpecHandle);
	}

	const int32 CurrentChildrenCount = ContainerHorizontalBox->GetChildrenCount();
	for (int32 SlotIndex = CurrentChildrenCount; SlotIndex < MinimumSlots; ++SlotIndex)
	{
		AddEmptySlot(true);
	}
}

void UAbilitiesBarWidget::AddAbilitySlot(const FGameplayAbilitySpecHandle& AbilitySpecHandle)
{
	FAbilityBarSlotData SlotData;
	SlotData.AbilitySpecHandle = AbilitySpecHandle;
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

	AddWidgetToBar(AbilityWidget, true);
}

void UAbilitiesBarWidget::AddEmptySlot(bool bApplyPadding)
{
	AddWidgetToBar(CreateBarWidget(EmptyAbilityWidgetClass), bApplyPadding);
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

bool UAbilitiesBarWidget::ShouldShowAbilityHandle(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAbilitySpecHandle& AbilitySpecHandle) const
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle);
	const USkillDataAsset* SourceSkill = ResolveSourceSkillDataAsset(AbilitySpec);
	return SourceSkill && SourceSkill->ShouldShowInAbilitiesBar();
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
		: 1;
	return FMath::Clamp(CurrentLevel > 0 ? CurrentLevel : 1, 1, PandoraDefinition->GetMaxLevel());
}

bool UAbilitiesBarWidget::IsSelectedPandoraCompatibleWithCurrentWeapon() const
{
	const UPandoraDefinition* SelectedPandoraDefinition = GetSelectedPandoraDefinition();
	return !SelectedPandoraDefinition || SelectedPandoraDefinition->IsCompatibleWithWeaponDefinition(GetCurrentWeaponDefinition());
}

const UItemDefinition* UAbilitiesBarWidget::GetCurrentWeaponDefinition() const
{
	const APdCharacterBase* OwningCharacter = Cast<APdCharacterBase>(GetOwningPlayerPawn());
	const UEquipmentComponent* EquipmentComponent = OwningCharacter ? OwningCharacter->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponDefinition() : nullptr;
}

FGameplayAbilitySpecHandle UAbilitiesBarWidget::FindAbilitySpecHandleForSkill(
	UAbilitySystemComponent* AbilitySystemComponent,
	const UPandoraDefinition* PandoraDefinition,
	int32 SkillIndex) const
{
	if (!AbilitySystemComponent || !PandoraDefinition)
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
	case 3:
		SkillInputTag = LabGameplayTags::Input_Ability_Skill4;
		break;
	default:
		break;
	}

	FGameplayAbilitySpecHandle FallbackHandle;

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
			&& RuntimeContext->GetSkillIndex() == SkillIndex)
		{
			return AbilityHandle;
		}

		const bool bMatchesSkillInput = !SkillInputTag.IsValid() || AbilitySpec->GetDynamicSpecSourceTags().HasTagExact(SkillInputTag);
		if (bMatchesSkillInput && !FallbackHandle.IsValid())
		{
			FallbackHandle = AbilityHandle;
		}
	}

	return FallbackHandle;
}

bool UAbilitiesBarWidget::IsConfiguredPandoraSkill(const FSkill& Skill) const
{
	return !Skill.GetAbilitiesToGrant().IsEmpty()
		|| Skill.GetIconResource() != nullptr
		|| !Skill.GetDisplayName().IsEmpty();
}

void UAbilitiesBarWidget::BindAbilitiesChangedEvents()
{
	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag AbilitiesChangedTag = LabGameplayTags::Event_Abilities_Changed;
	if (AbilitiesChangedTag.IsValid())
	{
		AbilitiesChangedEventHandle = AbilitySystemComponent->GenericGameplayEventCallbacks
			.FindOrAdd(AbilitiesChangedTag)
			.AddUObject(this, &ThisClass::HandleAbilitiesChangedEvent);
	}

	if (UPdAbilitySystemComponent* PdAbilitySystemComponent = Cast<UPdAbilitySystemComponent>(AbilitySystemComponent))
	{
		AbilitiesChangedNativeHandle = PdAbilitySystemComponent->OnAbilitiesChangedNative.AddUObject(this, &ThisClass::HandleAbilitiesChanged);
	}
}

void UAbilitiesBarWidget::UnbindAbilitiesChangedEvents()
{
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

void UAbilitiesBarWidget::HandleAbilitiesChanged()
{
	FillAbilitiesBar();
}

void UAbilitiesBarWidget::HandleAbilitiesChangedEvent(const FGameplayEventData* Payload)
{
	static_cast<void>(Payload);
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
