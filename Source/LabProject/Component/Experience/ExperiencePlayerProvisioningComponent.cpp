#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"

#include "Component/Experience/ExperiencePlayerProfileService.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "Provision/DefaultPlayerProvisioner.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperiencePlayerProvisioningComponent)

UExperiencePlayerProvisioningComponent::
UExperiencePlayerProvisioningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	PlayerProfileService =
		CreateDefaultSubobject<UExperiencePlayerProfileService>(
			TEXT("PlayerProfileService"));
	DefaultPlayerProvisioner =
		CreateDefaultSubobject<UDefaultPlayerProvisioner>(
			TEXT("DefaultPlayerProvisioner"));

	// Match the former embedded settings value even when this component is
	// constructed outside AExperienceGameMode and no explicit settings have
	// been injected yet.
	ApplySettings(FExperiencePlayerProvisioningSettings());
}

void UExperiencePlayerProvisioningComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginProvisioningContentPreload();
}

void UExperiencePlayerProvisioningComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ReleaseProvisioningContentPreload();
	PendingGameplayProvisions.Reset();
	bProvisioningContentReady = false;
	if (DefaultPlayerProvisioner)
	{
		DefaultPlayerProvisioner->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

void UExperiencePlayerProvisioningComponent::ApplySettings(
	const FExperiencePlayerProvisioningSettings& InSettings)
{
	CachedSettings = InSettings;
	if (PlayerProfileService)
	{
		PlayerProfileService->ApplySettings(InSettings);
	}
	if (DefaultPlayerProvisioner)
	{
		DefaultPlayerProvisioner->SetDefinition(
			InSettings.DefaultProvisionDefinition);
	}
}

void UExperiencePlayerProvisioningComponent::InitializeLoggedInPlayer(
	APlayerController* NewPlayer)
{
	if (PlayerProfileService)
	{
		PlayerProfileService->InitializeLoggedInPlayer(NewPlayer);
	}
}

void UExperiencePlayerProvisioningComponent::PreparePlayerForGameplay(
	APlayerController* NewPlayer,
	const bool bApplyLobbySkinEquipment)
{
	if (!NewPlayer)
	{
		return;
	}

	if (!bProvisioningContentReady)
	{
		PendingGameplayProvisions.RemoveAll(
			[NewPlayer](const FPendingGameplayProvision& PendingProvision)
			{
				return PendingProvision.PlayerController.Get() == NewPlayer;
			});
		FPendingGameplayProvision& PendingProvision =
			PendingGameplayProvisions.AddDefaulted_GetRef();
		PendingProvision.PlayerController = NewPlayer;
		PendingProvision.bApplyLobbySkinEquipment =
			bApplyLobbySkinEquipment;
		BeginProvisioningContentPreload();
		return;
	}

	PreparePlayerForGameplayInternal(NewPlayer, bApplyLobbySkinEquipment);
}

void UExperiencePlayerProvisioningComponent::PreparePlayerForGameplayInternal(
	APlayerController* NewPlayer,
	const bool bApplyLobbySkinEquipment)
{
	const EDefaultProvisionMode Mode = IsTrainingRoomMap()
		? EDefaultProvisionMode::TrainingRoom
		: EDefaultProvisionMode::Gameplay;
	if (bApplyLobbySkinEquipment && PlayerProfileService)
	{
		PlayerProfileService->ApplyCachedLobbySkinEquipment(NewPlayer);
	}
	if (DefaultPlayerProvisioner)
	{
		DefaultPlayerProvisioner->ProvisionPlayer(NewPlayer, Mode);
	}
}

void UExperiencePlayerProvisioningComponent::
ClearRuntimeStateForController(
	AController* Controller,
	APlayerState* PlayerState)
{
	// Match the previous contract: a null controller means there is no runtime
	// session key to clear.
	if (!Controller)
	{
		return;
	}
	PendingGameplayProvisions.RemoveAll(
		[Controller](const FPendingGameplayProvision& PendingProvision)
		{
			return PendingProvision.PlayerController.Get() == Controller;
		});
	if (DefaultPlayerProvisioner)
	{
		DefaultPlayerProvisioner->ClearRuntimeStateForController(
			Controller,
			PlayerState);
	}
}

void UExperiencePlayerProvisioningComponent::BeginProvisioningContentPreload()
{
	if (bProvisioningContentReady || bProvisioningContentLoadPending)
	{
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		bProvisioningContentReady = true;
		FlushPendingGameplayProvisions();
		return;
	}

	TArray<FPrimaryAssetId> ContentIds;
	TArray<FPrimaryAssetId> TypeContentIds;
	const auto AppendUniqueContentIds = [&ContentIds](
		const TArray<FPrimaryAssetId>& AssetIds)
	{
		for (const FPrimaryAssetId& AssetId : AssetIds)
		{
			ContentIds.AddUnique(AssetId);
		}
	};
	ContentSubsystem->GetSkillDataAssetIds(TypeContentIds);
	AppendUniqueContentIds(TypeContentIds);
	ContentSubsystem->GetPandoraDefinitionIds(TypeContentIds);
	AppendUniqueContentIds(TypeContentIds);
	ContentSubsystem->GetSkinDefinitionIds(TypeContentIds);
	AppendUniqueContentIds(TypeContentIds);

	bProvisioningContentLoadPending = true;
	TSharedPtr<FStreamableHandle> NewLoadHandle =
		ContentSubsystem->PreloadPrimaryAssetsAsync(
			ContentIds,
			FSimpleDelegate::CreateUObject(
				this,
				&ThisClass::HandleProvisioningContentPreloaded));
	if (NewLoadHandle.IsValid() && bProvisioningContentLoadPending)
	{
		ProvisioningContentLoadHandle = MoveTemp(NewLoadHandle);
	}
}

void UExperiencePlayerProvisioningComponent::HandleProvisioningContentPreloaded()
{
	bProvisioningContentLoadPending = false;
	bProvisioningContentReady = true;
	FlushPendingGameplayProvisions();
}

void UExperiencePlayerProvisioningComponent::ReleaseProvisioningContentPreload()
{
	bProvisioningContentLoadPending = false;
	if (ProvisioningContentLoadHandle.IsValid())
	{
		ProvisioningContentLoadHandle->CancelHandle();
		ProvisioningContentLoadHandle->ReleaseHandle();
		ProvisioningContentLoadHandle.Reset();
	}
}

void UExperiencePlayerProvisioningComponent::FlushPendingGameplayProvisions()
{
	if (!bProvisioningContentReady)
	{
		return;
	}

	TArray<FPendingGameplayProvision> Provisions =
		MoveTemp(PendingGameplayProvisions);
	PendingGameplayProvisions.Reset();
	for (const FPendingGameplayProvision& Provision : Provisions)
	{
		if (APlayerController* PlayerController =
			Provision.PlayerController.Get())
		{
			PreparePlayerForGameplayInternal(
				PlayerController,
				Provision.bApplyLobbySkinEquipment);
		}
	}
}

void UExperiencePlayerProvisioningComponent::
ApplyConfiguredStatusPointsForPlayerState(APlayerState* PlayerState)
{
	if (DefaultPlayerProvisioner)
	{
		DefaultPlayerProvisioner
			->ApplyConfiguredStatusPointsForPlayerState(
				PlayerState,
				IsTrainingRoomMap()
					? EDefaultProvisionMode::TrainingRoom
					: EDefaultProvisionMode::Gameplay);
	}
}

bool UExperiencePlayerProvisioningComponent::IsTrainingRoomMap() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UMatchRuleDefinition* MatchRules =
		CachedSettings.MatchRuleDefinition.Get();
	if (!MatchRules && !CachedSettings.MatchRuleDefinition.IsNull())
	{
		MatchRules = CachedSettings.MatchRuleDefinition.LoadSynchronous();
	}
	if (!MatchRules)
	{
		MatchRules = UMatchRuleDefinition::ResolveDefaultDefinition();
	}
	return MatchRules
		&& MatchRules->IsTrainingRoomMapName(
			UGameplayStatics::GetCurrentLevelName(World, true));
}
