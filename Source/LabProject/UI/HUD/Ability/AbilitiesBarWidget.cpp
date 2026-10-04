#include "UI/HUD/Ability/AbilitiesBarWidget.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Ability/SkillAbility.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Common/LabGameplayTags.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Character/CharacterBase.h"
#include "GameFramework/Pawn.h"
#include "Definition/Item/ItemDefinition.h"
#include "Mode/PdPlayerState.h"
#include "Mode/PdPlayerController.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "InputAction.h"
#include "Pandora/PandoraSkillSource.h"
#include "Component/Player/EquipmentComponent.h"
#include "TimerManager.h"
#include "UI/HUD/Ability/AbilitySlotWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilitiesBarWidget)

namespace
{
	const USkillDefinition* ResolveSourceSkillDataAsset(const FGameplayAbilitySpec* AbilitySpec)
	{
		if (!AbilitySpec)
		{
			return nullptr;
		}

		if (const UPandoraSkillSource* SkillSource = Cast<UPandoraSkillSource>(AbilitySpec->SourceObject.Get()))
		{
			return SkillSource->GetSkillDataAsset();
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

// 조종 캐릭터의 ASC가 준비될 때마다(리스폰 포함) 능력 목록 구독을 다시 연결한다.
void UAbilitiesBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	PossessedCharacterReadySubscription.SubscribeToPossessedCharacter(GetOwningPlayer(),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePossessedCharacterReady));
}

void UAbilitiesBarWidget::NativeDestruct()
{
	PossessedCharacterReadySubscription.Reset();
	UnbindAbilitiesChangedEvents();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebuildBarTimerHandle);
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
		RebuildBarTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RebuildAbilitiesBar);
		return;
	}

	RebuildAbilitiesBar();
}

void UAbilitiesBarWidget::HandlePossessedCharacterReady(
	ACharacterBase* Character, UPdAbilitySystemComponent* AbilitySystemComponent)
{
	static_cast<void>(Character);
	UnbindAbilitiesChangedEvents();

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
			const USkillDefinition* Skill = SelectedPandoraDefinition->GetSkillDefinition(SlotIndex);
			// 스킬 정의가 있으면 표시하고, 해금 및 무기 조건은 슬롯의 활성 상태에만 반영한다.
			if (!Skill)
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
				SlotData.AbilitySpecHandle = FindAbilitySpecHandleForSkill(AbilitySystemComponent,
					SelectedPandoraDefinition, SlotIndex);
				if (!SlotData.AbilitySpecHandle.IsValid())
				{
					SlotData.bEnabled = false;
				}
			}

			SlotData.DisplayNameOverride = Skill->GetDisplayName();
			SlotData.IconOverride = Skill->GetIconResource();
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
	UAbilitySlotWidget* AbilitySlotWidget = Cast<UAbilitySlotWidget>(CreateBarWidget(AbilityWidgetClass));
	if (!AbilitySlotWidget)
	{
		return;
	}

	AbilitySlotWidget->SetSkillSlotIndex(SlotData.SkillSlotIndex);
	if (SlotData.bHasDisplayOverride)
	{
		AbilitySlotWidget->SetAbilitySlotDataEnabled(SlotData.AbilitySpecHandle, SlotData.DisplayNameOverride,
			SlotData.IconOverride, SlotData.bEnabled);
	}
	else
	{
		AbilitySlotWidget->SetAbilitySpecHandle(SlotData.AbilitySpecHandle);
		AbilitySlotWidget->SetAbilitySlotEnabled(SlotData.bEnabled);
	}

	AddWidgetToBar(AbilitySlotWidget, true);
}

void UAbilitiesBarWidget::AddEmptySlot(const bool bApplyPadding, const int32 SkillSlotIndex)
{
	UUserWidget* EmptySlotWidget = CreateBarWidget(EmptyAbilityWidgetClass);
	ApplyEmptySlotKeyText(EmptySlotWidget, SkillSlotIndex);
	AddWidgetToBar(EmptySlotWidget, bApplyPadding);
}

UUserWidget* UAbilitiesBarWidget::CreateBarWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		return nullptr;
	}

	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		return CreateWidget<UUserWidget>(OwningPlayer, WidgetClass);
	}

	return CreateWidget<UUserWidget>(this, WidgetClass);
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

void UAbilitiesBarWidget::ApplyEmptySlotKeyText(UUserWidget* Widget, const int32 SkillSlotIndex) const
{
	// 빈 스킬 칸은 UAbilitySlotWidget이 아닌 일반 UserWidget이라 키 표시 텍스트를 이름으로 찾는다.
	UTextBlock* KeyText = Widget ? Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("KeyText"))) : nullptr;
	if (!KeyText)
	{
		return;
	}

	const APdPlayerController* PlayerController = Cast<APdPlayerController>(GetOwningPlayer());
	const UControllerInputDefinition* InputDefinition = PlayerController
		? PlayerController->GetLoadedInputDefinition() : nullptr;
	const FText Caption = InputDefinition
		? InputDefinition->ResolveInputActionKeyText(InputDefinition->GetLoadedSkillInputAction(SkillSlotIndex))
		: FText::GetEmpty();
	KeyText->SetText(Caption);
	if (UWidget* KeyOverlay = Widget->GetWidgetFromName(TEXT("InputKeyOverlay")))
	{
		KeyOverlay->SetVisibility(Caption.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

bool UAbilitiesBarWidget::ShouldShowAbilityHandle(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayAbilitySpecHandle& AbilitySpecHandle) const
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(AbilitySpecHandle);
	if (!AbilitySpec || !AbilitySpec->Ability || !AbilitySpec->Ability->IsA<USkillAbility>())
	{
		return false;
	}
	const USkillDefinition* SourceSkill = ResolveSourceSkillDataAsset(AbilitySpec);
	// 판도라 스킬은 선택된 판도라의 전용 슬롯에서 표시한다.
	return SourceSkill && !AbilitySpec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Ability_Source_Pandora);
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
		? PandoraTreeComponent->GetCurrentPandoraLevel(PandoraDefinition)
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
	UAbilitySystemComponent* AbilitySystemComponent, const UPandoraDefinition* PandoraDefinition,
	int32 SkillIndex) const
{
	if (!AbilitySystemComponent || !PandoraDefinition || SkillIndex < 0
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
		if (!AbilitySpec || !AbilitySpec->Ability || !AbilitySpec->Ability->IsA<USkillAbility>())
		{
			continue;
		}

		const UPandoraSkillSource* SkillSource = Cast<UPandoraSkillSource>(AbilitySpec->SourceObject.Get());
		if (SkillSource && SkillSource->GetPandoraDefinition() == PandoraDefinition
			&& SkillSource->GetSkillIndex() == SkillIndex
			&& AbilitySpec->GetDynamicSpecSourceTags().HasTagExact(SkillInputTag))
		{
			return AbilityHandle;
		}
	}

	return FGameplayAbilitySpecHandle();
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
		AbilitiesChangedNativeHandle = PdAbilitySystemComponent->OnAbilitiesChangedNative.AddUObject(this, &ThisClass::FillAbilitiesBar);
	}
}

void UAbilitiesBarWidget::UnbindAbilitiesChangedEvents()
{
	UnbindPandoraTreeChangedEvent();

	UAbilitySystemComponent* AbilitySystemComponent = CachedAbilitySystemComponent.Get();

	if (UPdAbilitySystemComponent* PdAbilitySystemComponent = Cast<UPdAbilitySystemComponent>(AbilitySystemComponent))
	{
		if (AbilitiesChangedNativeHandle.IsValid())
		{
			PdAbilitySystemComponent->OnAbilitiesChangedNative.Remove(AbilitiesChangedNativeHandle);
		}
	}

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

	PandoraTreeComponent->OnPandorasChanged.AddUniqueDynamic(this, &ThisClass::HandlePandoraTreeChanged);
	BoundPandoraTreeComponent = PandoraTreeComponent;
}

void UAbilitiesBarWidget::UnbindPandoraTreeChangedEvent()
{
	if (UPandoraTreeComponent* PandoraTreeComponent = BoundPandoraTreeComponent.Get())
	{
		PandoraTreeComponent->OnPandorasChanged.RemoveDynamic(this, &ThisClass::HandlePandoraTreeChanged);
	}
	BoundPandoraTreeComponent.Reset();
}

void UAbilitiesBarWidget::HandlePandoraTreeChanged()
{
	FillAbilitiesBar();
}
