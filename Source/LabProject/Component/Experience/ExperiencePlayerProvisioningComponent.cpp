#include "Component/Experience/ExperiencePlayerProvisioningComponent.h"

#include "Component/Experience/ExperienceGameplayLoadoutProvisioner.h"
#include "Component/Experience/ExperienceLobbyProfileProvisioner.h"
#include "Component/Experience/ExperienceTrainingRoomProvisioner.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperiencePlayerProvisioningComponent)

UExperiencePlayerProvisioningComponent::
UExperiencePlayerProvisioningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	LobbyProfileProvisioner =
		CreateDefaultSubobject<UExperienceLobbyProfileProvisioner>(
			TEXT("LobbyProfileProvisioner"));
	GameplayLoadoutProvisioner =
		CreateDefaultSubobject<UExperienceGameplayLoadoutProvisioner>(
			TEXT("GameplayLoadoutProvisioner"));
	TrainingRoomProvisioner =
		CreateDefaultSubobject<UExperienceTrainingRoomProvisioner>(
			TEXT("TrainingRoomProvisioner"));

	// Match the former embedded settings value even when this component is
	// constructed outside AExperienceGameMode and no explicit settings have
	// been injected yet.
	ApplySettings(FExperiencePlayerProvisioningSettings());
}

void UExperiencePlayerProvisioningComponent::OnRegister()
{
	Super::OnRegister();

	// Nested UObject default subobjects inherited through BP_GameMode can keep
	// their archetype outer unless the reference is explicitly instanced. Keep
	// a runtime guard as well so previously saved Blueprint classes are repaired
	// without requiring an asset resave.
	EnsureRuntimeProvisioners();
	ApplySettingsToProvisioners(CachedSettings);
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
	if (GameplayLoadoutProvisioner)
	{
		GameplayLoadoutProvisioner->Shutdown();
	}
	if (TrainingRoomProvisioner)
	{
		TrainingRoomProvisioner->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

void UExperiencePlayerProvisioningComponent::ApplySettings(
	const FExperiencePlayerProvisioningSettings& InSettings)
{
	CachedSettings = InSettings;
	EnsureRuntimeProvisioners();
	ApplySettingsToProvisioners(InSettings);
}

void UExperiencePlayerProvisioningComponent::ApplySettingsToProvisioners(
	const FExperiencePlayerProvisioningSettings& InSettings)
{
	if (LobbyProfileProvisioner)
	{
		LobbyProfileProvisioner->ApplySettings(InSettings);
	}
	if (GameplayLoadoutProvisioner)
	{
		GameplayLoadoutProvisioner->ApplySettings(InSettings);
	}
	if (TrainingRoomProvisioner)
	{
		TrainingRoomProvisioner->ApplySettings(InSettings);
	}
}

void UExperiencePlayerProvisioningComponent::InitializeLoggedInPlayer(
	APlayerController* NewPlayer)
{
	EnsureRuntimeProvisioners();
	if (LobbyProfileProvisioner)
	{
		LobbyProfileProvisioner->InitializeLoggedInPlayer(NewPlayer);
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
	EnsureRuntimeProvisioners();

	const bool bIsTrainingRoom =
		TrainingRoomProvisioner
		&& TrainingRoomProvisioner->IsTrainingRoomMap();

	// Preserve the existing provisioning order: clear/initialize the gameplay
	// loadout, apply cached cosmetics, grant fallback gestures, then add
	// training-room-only content.
	if (GameplayLoadoutProvisioner)
	{
		GameplayLoadoutProvisioner->PrepareGameplayLoadout(
			NewPlayer,
			bIsTrainingRoom);
	}
	if (bApplyLobbySkinEquipment && LobbyProfileProvisioner)
	{
		LobbyProfileProvisioner->ApplyCachedLobbySkinEquipment(NewPlayer);
	}
	if (GameplayLoadoutProvisioner)
	{
		GameplayLoadoutProvisioner->GrantDefaultGameplayGestures(NewPlayer);
	}
	if (TrainingRoomProvisioner)
	{
		TrainingRoomProvisioner->PreparePlayerForGameplay(NewPlayer);
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
	EnsureRuntimeProvisioners();

	if (GameplayLoadoutProvisioner)
	{
		GameplayLoadoutProvisioner->ClearRuntimeStateForController(
			Controller,
			PlayerState);
	}
	if (TrainingRoomProvisioner)
	{
		TrainingRoomProvisioner->ClearRuntimeStateForController(
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
	ContentSubsystem->GetPandoraDefinitionIds(TypeContentIds);
	ContentIds.Append(TypeContentIds);
	ContentSubsystem->GetSkinDefinitionIds(TypeContentIds);
	for (const FPrimaryAssetId& ContentId : TypeContentIds)
	{
		ContentIds.AddUnique(ContentId);
	}

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
GrantTrainingRoomStatusPointsForPlayerState(APlayerState* PlayerState)
{
	EnsureRuntimeProvisioners();
	if (TrainingRoomProvisioner)
	{
		TrainingRoomProvisioner
			->GrantTrainingRoomStatusPointsForPlayerState(PlayerState);
	}
}

bool UExperiencePlayerProvisioningComponent::IsTrainingRoomMap() const
{
	return TrainingRoomProvisioner
		&& TrainingRoomProvisioner->IsTrainingRoomMap();
}

int32 UExperiencePlayerProvisioningComponent::
GetPendingDefaultItemGrantCount() const
{
	return GameplayLoadoutProvisioner
		? GameplayLoadoutProvisioner->GetPendingDefaultItemGrantCount()
		: 0;
}

void UExperiencePlayerProvisioningComponent::EnsureRuntimeProvisioners()
{
	if (IsTemplate())
	{
		return;
	}

	bool bRecreatedProvisioner = false;
	if (!IsValid(LobbyProfileProvisioner)
		|| LobbyProfileProvisioner->GetOuter() != this
		|| LobbyProfileProvisioner->IsTemplate())
	{
		LobbyProfileProvisioner =
			NewObject<UExperienceLobbyProfileProvisioner>(this);
		bRecreatedProvisioner = true;
	}
	if (!IsValid(GameplayLoadoutProvisioner)
		|| GameplayLoadoutProvisioner->GetOuter() != this
		|| GameplayLoadoutProvisioner->IsTemplate())
	{
		GameplayLoadoutProvisioner =
			NewObject<UExperienceGameplayLoadoutProvisioner>(this);
		bRecreatedProvisioner = true;
	}
	if (!IsValid(TrainingRoomProvisioner)
		|| TrainingRoomProvisioner->GetOuter() != this
		|| TrainingRoomProvisioner->IsTemplate())
	{
		TrainingRoomProvisioner =
			NewObject<UExperienceTrainingRoomProvisioner>(this);
		bRecreatedProvisioner = true;
	}

	if (bRecreatedProvisioner)
	{
		ApplySettingsToProvisioners(CachedSettings);
	}
}
