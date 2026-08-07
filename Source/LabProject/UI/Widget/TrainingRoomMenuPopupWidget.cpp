#include "UI/Widget/TrainingRoomMenuPopupWidget.h"

#include "AI/TrainingBotAIController.h"
#include "Character/EnemyBase.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "EngineUtils.h"
#include "Definition/Item/ItemDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TrainingRoomMenuPopupWidget)

void UTrainingBotWeaponOptionClickProxy::Initialize(UTrainingRoomMenuPopupWidget* InOwnerWidget, int32 InOptionIndex)
{
	OwnerWidget = InOwnerWidget;
	OptionIndex = InOptionIndex;
}

void UTrainingBotWeaponOptionClickProxy::HandleClicked()
{
	if (OwnerWidget)
	{
		OwnerWidget->SelectTrainingBotWeaponOptionByIndex(OptionIndex);
	}
}

UTrainingRoomMenuPopupWidget::UTrainingRoomMenuPopupWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDestroySessionOnExit = false;
	bPauseGameWhenOpened = false;
	DaggerWeaponDefinition = TSoftObjectPtr<UItemDefinition>(
		FSoftObjectPath(TEXT("/Game/Item/Weapon/Dagger/DA_LoyalDagger.DA_LoyalDagger")));
}

void UTrainingRoomMenuPopupWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	bTrainingBotAttackEnabled = bInitialTrainingBotAttackEnabled;
	SyncBotCanAttackCheckBox();
	RefreshWeaponOptionSelectionVisuals();
}

void UTrainingRoomMenuPopupWidget::NativeConstruct()
{
	bDestroySessionOnExit = false;
	bPauseGameWhenOpened = false;
	Super::NativeConstruct();

	BeginConfiguredWeaponPreload();
	BindWeaponOptionButtons();

	bTrainingBotAttackEnabled = ResolveTrainingBotAttackEnabled();
	SyncBotCanAttackCheckBox();
	if (Chk_BotCanAttack)
	{
		Chk_BotCanAttack->OnCheckStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleBotCanAttackCheckStateChanged);
	}
	ApplyAttackEnabledToTrainingBots(bTrainingBotAttackEnabled);

	const bool bSyncedSelectionFromBot = SyncSelectedWeaponFromTrainingBot();
	if (!bSyncedSelectionFromBot && !SelectedWeaponDefinition && SelectedBuiltInButtonWidgetName.IsNone() && !InitialWeaponDefinition.IsNull())
	{
		SelectTrainingBotWeaponDefinitionSoft(InitialWeaponDefinition);
	}
	else
	{
		RefreshWeaponOptionSelectionVisuals();
	}
}

void UTrainingRoomMenuPopupWidget::NativeDestruct()
{
	CancelPendingWeaponSelection();
	ReleaseConfiguredWeaponPreload();

	if (Chk_BotCanAttack)
	{
		Chk_BotCanAttack->OnCheckStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleBotCanAttackCheckStateChanged);
	}

	UnbindWeaponOptionButtons();

	Super::NativeDestruct();
}

bool UTrainingRoomMenuPopupWidget::SelectTrainingBotWeaponOptionByIndex(int32 OptionIndex)
{
	if (!WeaponOptions.IsValidIndex(OptionIndex))
	{

		return false;
	}

	const FTrainingBotWeaponOption& Option = WeaponOptions[OptionIndex];
	if (Option.bUseUnarmed)
	{
		return SelectTrainingBotUnarmedInternal(OptionIndex);
	}

	if (Option.WeaponDefinition.IsNull())
	{

		return false;
	}

	return SelectTrainingBotWeaponDefinitionSoft(Option.WeaponDefinition);
}

bool UTrainingRoomMenuPopupWidget::SelectTrainingBotDagger()
{
	if (DaggerWeaponDefinition.IsNull())
	{

		return false;
	}

	return RequestTrainingBotWeaponSelection(
		DaggerWeaponDefinition,
		DaggerButtonWidgetName);
}

bool UTrainingRoomMenuPopupWidget::SelectTrainingBotUnarmed()
{
	return SelectTrainingBotUnarmedInternal(INDEX_NONE);
}

bool UTrainingRoomMenuPopupWidget::SelectTrainingBotWeaponDefinition(UItemDefinition* WeaponDefinition)
{
	CancelPendingWeaponSelection();
	return SelectTrainingBotWeaponDefinitionInternal(WeaponDefinition, NAME_None);
}

bool UTrainingRoomMenuPopupWidget::SelectTrainingBotWeaponDefinitionInternal(
	UItemDefinition* WeaponDefinition,
	const FName BuiltInButtonWidgetName)
{
	if (!WeaponDefinition)
	{

		return false;
	}

	SelectedWeaponDefinition = WeaponDefinition;
	SelectedWeaponOptionIndex = FindWeaponOptionIndex(WeaponDefinition);
	SelectedBuiltInButtonWidgetName =
		SelectedWeaponOptionIndex == INDEX_NONE ? BuiltInButtonWidgetName : NAME_None;
	RefreshWeaponOptionSelectionVisuals();
	const bool bApplied = ApplyWeaponToTrainingBot(WeaponDefinition);

	OnTrainingBotWeaponSelected.Broadcast(WeaponDefinition);
	BP_OnTrainingBotWeaponSelected(WeaponDefinition);

	return bApplied;
}

bool UTrainingRoomMenuPopupWidget::SelectTrainingBotWeaponDefinitionSoft(TSoftObjectPtr<UItemDefinition> WeaponDefinition)
{
	return RequestTrainingBotWeaponSelection(WeaponDefinition, NAME_None);
}

bool UTrainingRoomMenuPopupWidget::RequestTrainingBotWeaponSelection(
	TSoftObjectPtr<UItemDefinition> WeaponDefinition,
	const FName BuiltInButtonWidgetName)
{
	if (WeaponDefinition.IsNull())
	{
		return false;
	}

	CancelPendingWeaponSelection();
	if (UItemDefinition* LoadedWeaponDefinition = WeaponDefinition.Get())
	{
		return SelectTrainingBotWeaponDefinitionInternal(
			LoadedWeaponDefinition,
			BuiltInButtonWidgetName);
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return false;
	}

	const int32 SelectionGeneration = ++PendingWeaponSelectionGeneration;
	bPendingWeaponSelectionRequestActive = true;
	TSharedPtr<FStreamableHandle> SelectionHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			{WeaponDefinition.ToSoftObjectPath()},
			FSimpleDelegate::CreateWeakLambda(
				this,
				[this, SelectionGeneration, WeaponDefinition, BuiltInButtonWidgetName]()
				{
					CompletePendingWeaponSelection(
						SelectionGeneration,
						WeaponDefinition,
						BuiltInButtonWidgetName);
				}));

	// A resident asset may invoke the completion delegate before RequestAsyncLoad returns.
	if (bPendingWeaponSelectionRequestActive
		&& SelectionGeneration == PendingWeaponSelectionGeneration)
	{
		PendingWeaponSelectionHandle = MoveTemp(SelectionHandle);
	}
	else if (SelectionHandle.IsValid())
	{
		SelectionHandle->ReleaseHandle();
	}

	return true;
}

void UTrainingRoomMenuPopupWidget::CompletePendingWeaponSelection(
	const int32 SelectionGeneration,
	TSoftObjectPtr<UItemDefinition> WeaponDefinition,
	const FName BuiltInButtonWidgetName)
{
	if (SelectionGeneration != PendingWeaponSelectionGeneration)
	{
		return;
	}

	bPendingWeaponSelectionRequestActive = false;
	TSharedPtr<FStreamableHandle> CompletedHandle = MoveTemp(PendingWeaponSelectionHandle);
	PendingWeaponSelectionHandle.Reset();

	if (UItemDefinition* LoadedWeaponDefinition = WeaponDefinition.Get())
	{
		SelectTrainingBotWeaponDefinitionInternal(
			LoadedWeaponDefinition,
			BuiltInButtonWidgetName);
	}

	if (CompletedHandle.IsValid())
	{
		CompletedHandle->ReleaseHandle();
	}
}

void UTrainingRoomMenuPopupWidget::BeginConfiguredWeaponPreload()
{
	ReleaseConfiguredWeaponPreload();

	TArray<FSoftObjectPath> WeaponPaths;
	if (!InitialWeaponDefinition.IsNull())
	{
		WeaponPaths.Add(InitialWeaponDefinition.ToSoftObjectPath());
	}
	if (!DaggerWeaponDefinition.IsNull())
	{
		WeaponPaths.Add(DaggerWeaponDefinition.ToSoftObjectPath());
	}
	for (const FTrainingBotWeaponOption& Option : WeaponOptions)
	{
		if (!Option.WeaponDefinition.IsNull())
		{
			WeaponPaths.Add(Option.WeaponDefinition.ToSoftObjectPath());
		}
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	ConfiguredWeaponPreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(WeaponPaths);
}

void UTrainingRoomMenuPopupWidget::ReleaseConfiguredWeaponPreload()
{
	if (ConfiguredWeaponPreloadHandle.IsValid())
	{
		ConfiguredWeaponPreloadHandle->CancelHandle();
		ConfiguredWeaponPreloadHandle->ReleaseHandle();
		ConfiguredWeaponPreloadHandle.Reset();
	}
}

void UTrainingRoomMenuPopupWidget::CancelPendingWeaponSelection()
{
	++PendingWeaponSelectionGeneration;
	bPendingWeaponSelectionRequestActive = false;
	if (PendingWeaponSelectionHandle.IsValid())
	{
		PendingWeaponSelectionHandle->CancelHandle();
		PendingWeaponSelectionHandle->ReleaseHandle();
		PendingWeaponSelectionHandle.Reset();
	}
}

void UTrainingRoomMenuPopupWidget::RefreshWeaponOptionSelectionVisuals()
{
	const int32 SelectedIndex = SelectedWeaponOptionIndex != INDEX_NONE
		? SelectedWeaponOptionIndex
		: FindWeaponOptionIndex(SelectedWeaponDefinition);

	for (int32 OptionIndex = 0; OptionIndex < WeaponOptions.Num(); ++OptionIndex)
	{
		UImage* SelectionBorderImage = GetOptionSelectionBorderImage(WeaponOptions[OptionIndex]);
		if (!SelectionBorderImage)
		{
			continue;
		}

		const bool bSelected = OptionIndex == SelectedIndex;
		SelectionBorderImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SelectionBorderImage->SetColorAndOpacity(bSelected ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
	}

	if (!IsButtonNameConfiguredInWeaponOptions(DaggerButtonWidgetName))
	{
		SetBuiltInSelectionBorderState(
			DaggerSelectionBorderImageName,
			SelectedBuiltInButtonWidgetName == DaggerButtonWidgetName);
	}

	if (!IsButtonNameConfiguredInWeaponOptions(UnarmedButtonWidgetName))
	{
		SetBuiltInSelectionBorderState(
			UnarmedSelectionBorderImageName,
			SelectedBuiltInButtonWidgetName == UnarmedButtonWidgetName);
	}
}

void UTrainingRoomMenuPopupWidget::SetTrainingBotAttackEnabled(bool bEnabled)
{
	bTrainingBotAttackEnabled = bEnabled;
	SyncBotCanAttackCheckBox();
	const bool bApplied = ApplyAttackEnabledToTrainingBots(bTrainingBotAttackEnabled);

}

void UTrainingRoomMenuPopupWidget::HandleBotCanAttackCheckStateChanged(bool bIsChecked)
{
	SetTrainingBotAttackEnabled(bIsChecked);
}

void UTrainingRoomMenuPopupWidget::HandleDaggerButtonClicked()
{
	SelectTrainingBotDagger();
}

void UTrainingRoomMenuPopupWidget::HandleUnarmedButtonClicked()
{
	SelectTrainingBotUnarmed();
}

bool UTrainingRoomMenuPopupWidget::SelectTrainingBotUnarmedInternal(const int32 OptionIndex)
{
	CancelPendingWeaponSelection();
	SelectedWeaponDefinition = nullptr;
	SelectedWeaponOptionIndex = OptionIndex;
	SelectedBuiltInButtonWidgetName = OptionIndex == INDEX_NONE ? UnarmedButtonWidgetName : NAME_None;
	RefreshWeaponOptionSelectionVisuals();
	const bool bApplied = ApplyUnarmedToTrainingBot();

	OnTrainingBotWeaponSelected.Broadcast(nullptr);
	BP_OnTrainingBotWeaponSelected(nullptr);

	return bApplied;
}

bool UTrainingRoomMenuPopupWidget::ApplyWeaponToTrainingBot(UItemDefinition* WeaponDefinition) const
{
	if (!WeaponDefinition)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{

		return false;
	}

	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* TrainingBot = *It;
		const AController* BotController = IsValid(TrainingBot) ? TrainingBot->GetController() : nullptr;
		if (!IsValid(TrainingBot) || !BotController || !BotController->IsA<ATrainingBotAIController>())
		{
			continue;
		}

		return TrainingBot->RequestTrainingBotWeaponChange(WeaponDefinition);
	}

	return false;
}

bool UTrainingRoomMenuPopupWidget::ApplyUnarmedToTrainingBot() const
{
	UWorld* World = GetWorld();
	if (!World)
	{

		return false;
	}

	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* TrainingBot = *It;
		const AController* BotController = IsValid(TrainingBot) ? TrainingBot->GetController() : nullptr;
		if (!IsValid(TrainingBot) || !BotController || !BotController->IsA<ATrainingBotAIController>())
		{
			continue;
		}

		return TrainingBot->RequestTrainingBotUnarmed();
	}

	return false;
}

bool UTrainingRoomMenuPopupWidget::ApplyAttackEnabledToTrainingBots(bool bEnabled) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	bool bAppliedToAnyBot = false;
	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		AEnemyBase* TrainingBot = *It;
		const AController* BotController = IsValid(TrainingBot) ? TrainingBot->GetController() : nullptr;
		if (!IsValid(TrainingBot) || !BotController || !BotController->IsA<ATrainingBotAIController>())
		{
			continue;
		}

		TrainingBot->SetAttackEnabled(bEnabled);
		bAppliedToAnyBot = true;

}

	return bAppliedToAnyBot;
}

bool UTrainingRoomMenuPopupWidget::ResolveTrainingBotAttackEnabled() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return bInitialTrainingBotAttackEnabled;
	}

	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		const AEnemyBase* TrainingBot = *It;
		const AController* BotController = IsValid(TrainingBot) ? TrainingBot->GetController() : nullptr;
		if (IsValid(TrainingBot) && BotController && BotController->IsA<ATrainingBotAIController>())
		{
			return TrainingBot->IsAttackEnabled();
		}
	}

	return bInitialTrainingBotAttackEnabled;
}

bool UTrainingRoomMenuPopupWidget::SyncSelectedWeaponFromTrainingBot()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<AEnemyBase> It(World); It; ++It)
	{
		const AEnemyBase* TrainingBot = *It;
		const AController* BotController = IsValid(TrainingBot) ? TrainingBot->GetController() : nullptr;
		if (!IsValid(TrainingBot) || !BotController || !BotController->IsA<ATrainingBotAIController>())
		{
			continue;
		}

		const UItemDefinition* BotWeaponDefinition = TrainingBot->GetCurrentOrStartingEnemyWeaponDefinition();
		if (!BotWeaponDefinition)
		{
			SelectedWeaponDefinition = nullptr;
			SelectedWeaponOptionIndex = FindUnarmedWeaponOptionIndex();
			SelectedBuiltInButtonWidgetName = SelectedWeaponOptionIndex == INDEX_NONE ? UnarmedButtonWidgetName : NAME_None;

			return true;
		}

		SelectedWeaponDefinition = const_cast<UItemDefinition*>(BotWeaponDefinition);
		SelectedWeaponOptionIndex = FindWeaponOptionIndex(BotWeaponDefinition);
		SelectedBuiltInButtonWidgetName = NAME_None;
		if (SelectedWeaponOptionIndex == INDEX_NONE && DoesSoftWeaponDefinitionMatch(DaggerWeaponDefinition, BotWeaponDefinition))
		{
			SelectedBuiltInButtonWidgetName = DaggerButtonWidgetName;
		}

		return true;
	}

	return false;
}

void UTrainingRoomMenuPopupWidget::SyncBotCanAttackCheckBox()
{
	if (Chk_BotCanAttack)
	{
		if (Chk_BotCanAttack->IsChecked() != bTrainingBotAttackEnabled)
		{
			Chk_BotCanAttack->SetIsChecked(bTrainingBotAttackEnabled);
		}
	}
}

void UTrainingRoomMenuPopupWidget::BindWeaponOptionButtons()
{
	UnbindWeaponOptionButtons();

	for (int32 OptionIndex = 0; OptionIndex < WeaponOptions.Num(); ++OptionIndex)
	{
		const FTrainingBotWeaponOption& Option = WeaponOptions[OptionIndex];
		if (Option.ButtonWidgetName.IsNone())
		{
			continue;
		}

		UButton* Button = Cast<UButton>(GetWidgetFromName(Option.ButtonWidgetName));
		if (!Button)
		{

			continue;
		}

		UTrainingBotWeaponOptionClickProxy* ClickProxy = NewObject<UTrainingBotWeaponOptionClickProxy>(this);
		ClickProxy->Initialize(this, OptionIndex);
		Button->OnClicked.AddUniqueDynamic(ClickProxy, &UTrainingBotWeaponOptionClickProxy::HandleClicked);

		FTrainingBotWeaponOptionBinding& Binding = WeaponOptionBindings.AddDefaulted_GetRef();
		Binding.Button = Button;
		Binding.ClickProxy = ClickProxy;
	}

	if (!DaggerButtonWidgetName.IsNone() && !IsButtonNameConfiguredInWeaponOptions(DaggerButtonWidgetName))
	{
		if (UButton* DaggerButton = Cast<UButton>(GetWidgetFromName(DaggerButtonWidgetName)))
		{
			DaggerButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDaggerButtonClicked);
		}
	}

	if (!UnarmedButtonWidgetName.IsNone() && !IsButtonNameConfiguredInWeaponOptions(UnarmedButtonWidgetName))
	{
		if (UButton* UnarmedButton = Cast<UButton>(GetWidgetFromName(UnarmedButtonWidgetName)))
		{
			UnarmedButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUnarmedButtonClicked);
		}
	}
}

void UTrainingRoomMenuPopupWidget::UnbindWeaponOptionButtons()
{
	if (!DaggerButtonWidgetName.IsNone() && !IsButtonNameConfiguredInWeaponOptions(DaggerButtonWidgetName))
	{
		if (UButton* DaggerButton = Cast<UButton>(GetWidgetFromName(DaggerButtonWidgetName)))
		{
			DaggerButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleDaggerButtonClicked);
		}
	}

	if (!UnarmedButtonWidgetName.IsNone() && !IsButtonNameConfiguredInWeaponOptions(UnarmedButtonWidgetName))
	{
		if (UButton* UnarmedButton = Cast<UButton>(GetWidgetFromName(UnarmedButtonWidgetName)))
		{
			UnarmedButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleUnarmedButtonClicked);
		}
	}

	for (const FTrainingBotWeaponOptionBinding& Binding : WeaponOptionBindings)
	{
		if (Binding.Button && Binding.ClickProxy)
		{
			Binding.Button->OnClicked.RemoveDynamic(
				Binding.ClickProxy,
				&UTrainingBotWeaponOptionClickProxy::HandleClicked);
		}
	}

	WeaponOptionBindings.Reset();
}

int32 UTrainingRoomMenuPopupWidget::FindWeaponOptionIndex(const UItemDefinition* WeaponDefinition) const
{
	if (!WeaponDefinition)
	{
		return INDEX_NONE;
	}

	const FString SelectedPath = WeaponDefinition->GetPathName();
	for (int32 OptionIndex = 0; OptionIndex < WeaponOptions.Num(); ++OptionIndex)
	{
		const TSoftObjectPtr<UItemDefinition>& OptionDefinition = WeaponOptions[OptionIndex].WeaponDefinition;
		if (OptionDefinition.Get() == WeaponDefinition)
		{
			return OptionIndex;
		}

		if (!OptionDefinition.IsNull() && OptionDefinition.ToSoftObjectPath().ToString() == SelectedPath)
		{
			return OptionIndex;
		}
	}

	return INDEX_NONE;
}

int32 UTrainingRoomMenuPopupWidget::FindUnarmedWeaponOptionIndex() const
{
	for (int32 OptionIndex = 0; OptionIndex < WeaponOptions.Num(); ++OptionIndex)
	{
		if (WeaponOptions[OptionIndex].bUseUnarmed)
		{
			return OptionIndex;
		}
	}

	return INDEX_NONE;
}

bool UTrainingRoomMenuPopupWidget::DoesSoftWeaponDefinitionMatch(
	TSoftObjectPtr<UItemDefinition> SoftWeaponDefinition,
	const UItemDefinition* WeaponDefinition) const
{
	if (!WeaponDefinition || SoftWeaponDefinition.IsNull())
	{
		return false;
	}

	if (SoftWeaponDefinition.Get() == WeaponDefinition)
	{
		return true;
	}

	return SoftWeaponDefinition.ToSoftObjectPath().ToString() == WeaponDefinition->GetPathName();
}

bool UTrainingRoomMenuPopupWidget::IsButtonNameConfiguredInWeaponOptions(const FName ButtonWidgetName) const
{
	if (ButtonWidgetName.IsNone())
	{
		return false;
	}

	for (const FTrainingBotWeaponOption& Option : WeaponOptions)
	{
		if (Option.ButtonWidgetName == ButtonWidgetName)
		{
			return true;
		}
	}

	return false;
}

UImage* UTrainingRoomMenuPopupWidget::GetOptionSelectionBorderImage(const FTrainingBotWeaponOption& Option) const
{
	if (Option.SelectionBorderImageName.IsNone())
	{
		return nullptr;
	}

	return Cast<UImage>(GetWidgetFromName(Option.SelectionBorderImageName));
}

void UTrainingRoomMenuPopupWidget::SetBuiltInSelectionBorderState(
	const FName SelectionBorderImageName,
	const bool bSelected) const
{
	if (SelectionBorderImageName.IsNone())
	{
		return;
	}

	UImage* SelectionBorderImage = Cast<UImage>(GetWidgetFromName(SelectionBorderImageName));
	if (!SelectionBorderImage)
	{
		return;
	}

	SelectionBorderImage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SelectionBorderImage->SetColorAndOpacity(bSelected ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
}
