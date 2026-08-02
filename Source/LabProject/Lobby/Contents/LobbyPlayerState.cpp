#include "Lobby/Contents/LobbyPlayerState.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PandoraTreeComponent.h"
#include "Component/Item/InventoryComponent.h"
#include "Component/Lobby/LobbyPlayerStateComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Component/Skin/SkinComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Lobby/Contents/LobbyHUD.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPlayerState)

ALobbyPlayerState::ALobbyPlayerState(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetUpdateFrequency(30.0f);

	LobbyPlayerStateComponent =
		CreateDefaultSubobject<ULobbyPlayerStateComponent>(
			TEXT("LobbyPlayerStateComponent"));
	LobbyInventoryComponent =
		CreateDefaultSubobject<UInventoryComponent>(
			TEXT("LobbyInventoryComponent"));
	LobbySkinComponent =
		CreateDefaultSubobject<USkinComponent>(
			TEXT("LobbySkinComponent"));
	LobbyPandoraComponent =
		CreateDefaultSubobject<UPandoraComponent>(
			TEXT("LobbyPandoraComponent"));
	LobbyPandoraTreeComponent =
		CreateDefaultSubobject<UPandoraTreeComponent>(
			TEXT("LobbyPandoraTreeComponent"));
	LobbyBasicAttributeSet =
		CreateDefaultSubobject<UBasicAttributeSet>(
			TEXT("LobbyBasicAttributeSet"));
}

void ALobbyPlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent
			->OnLobbyRuntimeStateChanged.AddUObject(
				this,
				&ThisClass::HandleLobbyRuntimeStateChanged);
	}
	InitializeLobbyPreviewAbilitySystem();
	RequestDeferredLobbyUiRefresh();
}

void ALobbyPlayerState::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent
			->OnLobbyRuntimeStateChanged.RemoveAll(this);
	}

	RequestDeferredLobbyUiRefresh();
	Super::EndPlay(EndPlayReason);
}

USkinComponent* ALobbyPlayerState::GetSkinComponent() const
{
	return LobbySkinComponent
		? LobbySkinComponent.Get()
		: Super::GetSkinComponent();
}

void ALobbyPlayerState::SetReady(const bool bInReady)
{
	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent->SetReady(bInReady);
	}
}

bool ALobbyPlayerState::IsReady() const
{
	return LobbyPlayerStateComponent
		&& LobbyPlayerStateComponent->IsReady();
}

void ALobbyPlayerState::SetLeavingLobby(
	const bool bInLeavingLobby)
{
	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent->SetLeavingLobby(
			bInLeavingLobby);
	}
}

bool ALobbyPlayerState::IsLeavingLobby() const
{
	return LobbyPlayerStateComponent
		&& LobbyPlayerStateComponent->IsLeavingLobby();
}

void ALobbyPlayerState::SetNickname(const FText& InNickname)
{
	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent->SetNickname(InNickname);
	}
}

void ALobbyPlayerState::SetDefaultNickname(
	const FText& InNickname)
{
	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent->SetDefaultNickname(
			InNickname);
	}
}

void ALobbyPlayerState::ClearCustomNickname()
{
	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent->ClearCustomNickname();
	}
}

FText ALobbyPlayerState::GetNickname() const
{
	return LobbyPlayerStateComponent
		? LobbyPlayerStateComponent->GetNickname()
		: FText::GetEmpty();
}

FText ALobbyPlayerState::GetNicknameHint() const
{
	return LobbyPlayerStateComponent
		? LobbyPlayerStateComponent->GetNicknameHint()
		: FText::GetEmpty();
}

bool ALobbyPlayerState::IsUsingNicknameHint() const
{
	return LobbyPlayerStateComponent
		&& LobbyPlayerStateComponent->IsUsingNicknameHint();
}

void ALobbyPlayerState::SetTeamColorIndex(
	const int32 InTeamColorIndex)
{
	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent->SetTeamColorIndex(
			InTeamColorIndex);
	}
}

int32 ALobbyPlayerState::GetTeamColorIndex() const
{
	return LobbyPlayerStateComponent
		? LobbyPlayerStateComponent->GetTeamColorIndex()
		: INDEX_NONE;
}

void ALobbyPlayerState::ImportPlayerMatchIdentity(
	const FPlayerMatchIdentity& InMatchIdentity)
{
	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent->ImportPlayerMatchIdentity(
			InMatchIdentity);
	}
}

void ALobbyPlayerState::InitializeLobbyPreviewAbilitySystem(
	const ULobbyPreviewDefinition* PreviewDefinition)
{
	if (LobbyPlayerStateComponent)
	{
		LobbyPlayerStateComponent
			->InitializePreviewAbilitySystem(
				GetPdAbilitySystemComponent(),
				LobbyBasicAttributeSet,
				PreviewDefinition);
	}
}

FPlayerMatchIdentity
ALobbyPlayerState::GetMatchIdentityForCopyProperties() const
{
	return LobbyPlayerStateComponent
		? LobbyPlayerStateComponent
			->BuildConfirmedPlayerMatchIdentity()
		: Super::GetMatchIdentityForCopyProperties();
}

void ALobbyPlayerState::RefreshLobbyUI() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* LocalPlayerController =
		UGameplayStatics::GetPlayerController(World, 0);
	ALobbyHUD* LobbyHUD = LocalPlayerController
		? LocalPlayerController->GetHUD<ALobbyHUD>()
		: nullptr;
	if (LobbyHUD)
	{
		LobbyHUD->RefreshLobbyUI();
	}
}

void ALobbyPlayerState::HandleLobbyRuntimeStateChanged()
{
	OnLobbyPlayerStateChanged.Broadcast();
	RefreshLobbyUI();
}

void ALobbyPlayerState::RequestDeferredLobbyUiRefresh() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerDelegate RefreshDelegate;
	RefreshDelegate.BindWeakLambda(this, [this]()
	{
		RefreshLobbyUI();
	});
	World->GetTimerManager().SetTimerForNextTick(
		RefreshDelegate);
}
