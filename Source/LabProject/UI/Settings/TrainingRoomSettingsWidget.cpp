#include "UI/Settings/TrainingRoomSettingsWidget.h"

#include "AI/Training/TrainingBotAIController.h"
#include "Character/EnemyBase.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Level/LevelDefinition.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "UI/Common/ButtonClickRelay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TrainingRoomSettingsWidget)

namespace
{
	bool IsTrainingBot(const AEnemyBase* Enemy)
	{
		const AController* BotController = IsValid(Enemy) ? Enemy->GetController() : nullptr;
		return BotController && BotController->IsA<ATrainingBotAIController>();
	}

	// 훈련장의 첫 훈련 봇. 무기·공격 설정은 이 봇을 기준으로 보여 준다.
	AEnemyBase* FindTrainingBot(const UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<AEnemyBase> It(World); It; ++It)
		{
			if (IsTrainingBot(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}
}

void UTrainingRoomSettingsWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshSelectionBorders();
}

void UTrainingRoomSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 게임 설정 화면은 어느 맵에서나 이 탭을 만들기 때문에, 훈련장이 아니면 봇을 찾거나 무기를 미리 불러오지 않는다.
	bActiveInTrainingRoom = ULevelDefinition::IsTrainingRoomWorld(this);
	if (!bActiveInTrainingRoom)
	{
		return;
	}

	BeginWeaponPreload();
	for (int32 OptionIndex = 0; OptionIndex < WeaponOptions.Num(); ++OptionIndex)
	{
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(WeaponOptions[OptionIndex].ButtonWidgetName)))
		{
			UButtonClickRelay* ClickRelay = NewObject<UButtonClickRelay>(this);
			ClickRelay->Bind(Button, FSimpleDelegate::CreateUObject(this, &ThisClass::SelectWeaponOption, OptionIndex));
			WeaponClickRelays.Add(ClickRelay);
		}
	}

	const AEnemyBase* TrainingBot = FindTrainingBot(GetWorld());
	const bool bAttackEnabled = TrainingBot ? TrainingBot->IsAttackEnabled() : bInitialBotAttackEnabled;
	Chk_BotCanAttack->SetIsChecked(bAttackEnabled);
	Chk_BotCanAttack->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::HandleBotCanAttackChanged);
	ApplyAttackEnabledToTrainingBots(bAttackEnabled);

	SyncSelectionFromTrainingBot();
	RefreshSelectionBorders();
}

void UTrainingRoomSettingsWidget::NativeDestruct()
{
	PendingWeaponLease.Reset();
	WeaponPreloadLease.Reset();
	Chk_BotCanAttack->OnCheckStateChanged.RemoveDynamic(this, &ThisClass::HandleBotCanAttackChanged);
	for (UButtonClickRelay* ClickRelay : WeaponClickRelays)
	{
		ClickRelay->Unbind();
	}
	WeaponClickRelays.Reset();

	Super::NativeDestruct();
}

void UTrainingRoomSettingsWidget::SelectWeaponOption(const int32 OptionIndex)
{
	if (!WeaponOptions.IsValidIndex(OptionIndex))
	{
		return;
	}

	PendingWeaponLease.Reset();
	const FTrainingBotWeaponOption& Option = WeaponOptions[OptionIndex];
	if (Option.bUseUnarmed || Option.WeaponDefinition.Get())
	{
		ApplyWeaponOption(OptionIndex);
		return;
	}
	if (Option.WeaponDefinition.IsNull())
	{
		return;
	}

	if (UContentDataSubsystem* ContentSubsystem = UGameInstance::GetSubsystem<UContentDataSubsystem>(GetGameInstance()))
	{
		PendingWeaponLease = ContentSubsystem->AcquireContent({Option.WeaponDefinition.ToSoftObjectPath()},
			FSimpleDelegate::CreateUObject(this, &ThisClass::CompletePendingWeaponSelection, OptionIndex));
	}
}

void UTrainingRoomSettingsWidget::HandleBotCanAttackChanged(const bool bIsChecked)
{
	ApplyAttackEnabledToTrainingBots(bIsChecked);
}

void UTrainingRoomSettingsWidget::CompletePendingWeaponSelection(const int32 OptionIndex)
{
	// 선택 처리 중에 새 요청이 시작될 수 있으므로 끝난 lease를 먼저 꺼내 두고 선택이 끝난 뒤 놓는다.
	const TSharedPtr<FContentLease> CompletedLease = MoveTemp(PendingWeaponLease);
	if (CompletedLease.IsValid() && CompletedLease->IsReady() && WeaponOptions[OptionIndex].WeaponDefinition.Get())
	{
		ApplyWeaponOption(OptionIndex);
	}
}

// 봇이 없어도 선택 표시는 바꿔, 나중에 봇이 생기면 같은 버튼을 다시 눌러 적용할 수 있게 한다.
void UTrainingRoomSettingsWidget::ApplyWeaponOption(const int32 OptionIndex)
{
	SelectedOptionIndex = OptionIndex;
	RefreshSelectionBorders();

	AEnemyBase* TrainingBot = FindTrainingBot(GetWorld());
	if (!TrainingBot)
	{
		return;
	}
	const FTrainingBotWeaponOption& Option = WeaponOptions[OptionIndex];
	if (Option.bUseUnarmed)
	{
		TrainingBot->RequestTrainingBotUnarmed();
	}
	else
	{
		TrainingBot->RequestTrainingBotWeaponChange(Option.WeaponDefinition.Get());
	}
}

void UTrainingRoomSettingsWidget::ApplyAttackEnabledToTrainingBots(const bool bEnabled) const
{
	for (TActorIterator<AEnemyBase> It(GetWorld()); It; ++It)
	{
		if (IsTrainingBot(*It))
		{
			It->SetAttackEnabled(bEnabled);
		}
	}
}

void UTrainingRoomSettingsWidget::SyncSelectionFromTrainingBot()
{
	if (const AEnemyBase* TrainingBot = FindTrainingBot(GetWorld()))
	{
		SelectedOptionIndex = FindWeaponOptionIndex(TrainingBot->GetCurrentOrStartingEnemyWeaponDefinition());
	}
}

// 무기 정의가 없으면 맨손 버튼을 찾는다.
int32 UTrainingRoomSettingsWidget::FindWeaponOptionIndex(const UItemDefinition* WeaponDefinition) const
{
	return WeaponOptions.IndexOfByPredicate([WeaponDefinition](const FTrainingBotWeaponOption& Option)
	{
		return WeaponDefinition
			? !Option.bUseUnarmed && Option.WeaponDefinition.ToSoftObjectPath() == FSoftObjectPath(WeaponDefinition)
			: Option.bUseUnarmed;
	});
}

void UTrainingRoomSettingsWidget::RefreshSelectionBorders()
{
	for (int32 OptionIndex = 0; OptionIndex < WeaponOptions.Num(); ++OptionIndex)
	{
		if (UImage* SelectionBorder = Cast<UImage>(GetWidgetFromName(WeaponOptions[OptionIndex].SelectionBorderImageName)))
		{
			SelectionBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			SelectionBorder->SetColorAndOpacity(
				OptionIndex == SelectedOptionIndex ? SelectionBorderSelectedColor : SelectionBorderDefaultColor);
		}
	}
}

void UTrainingRoomSettingsWidget::BeginWeaponPreload()
{
	TArray<FSoftObjectPath> WeaponPaths;
	for (const FTrainingBotWeaponOption& Option : WeaponOptions)
	{
		if (!Option.WeaponDefinition.IsNull())
		{
			WeaponPaths.Add(Option.WeaponDefinition.ToSoftObjectPath());
		}
	}
	if (UContentDataSubsystem* ContentSubsystem = UGameInstance::GetSubsystem<UContentDataSubsystem>(GetGameInstance()))
	{
		WeaponPreloadLease = ContentSubsystem->AcquireContent(WeaponPaths);
	}
}
