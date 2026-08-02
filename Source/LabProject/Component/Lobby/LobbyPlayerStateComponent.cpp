#include "Component/Lobby/LobbyPlayerStateComponent.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Definition/Lobby/LobbyPreviewDefinition.h"
#include "GameFramework/PlayerState.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPlayerStateComponent)

ULobbyPlayerStateComponent::ULobbyPlayerStateComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void ULobbyPlayerStateComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UPlayerMatchComponent* MatchComponent =
		GetPlayerMatchComponent())
	{
		MatchComponent->OnMatchTeamColorChanged.AddUObject(
			this,
			&ThisClass::HandleMatchTeamColorChanged);
	}
}

void ULobbyPlayerStateComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UPlayerMatchComponent* MatchComponent =
		GetPlayerMatchComponent())
	{
		MatchComponent->OnMatchTeamColorChanged.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ULobbyPlayerStateComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		ULobbyPlayerStateComponent,
		bReady,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		ULobbyPlayerStateComponent,
		bLeavingLobby,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		ULobbyPlayerStateComponent,
		Nickname,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		ULobbyPlayerStateComponent,
		NicknameHint,
		Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(
		ULobbyPlayerStateComponent,
		bUsingNicknameHint,
		Params);
}

void ULobbyPlayerStateComponent::SetReady(const bool bInReady)
{
	if (!HasAuthority() || bReady == bInReady)
	{
		return;
	}

	bReady = bInReady;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		ULobbyPlayerStateComponent,
		bReady,
		this);
	GetOwner()->ForceNetUpdate();
	NotifyLobbyRuntimeStateChanged();
}

void ULobbyPlayerStateComponent::SetLeavingLobby(
	const bool bInLeavingLobby)
{
	if (!HasAuthority() || bLeavingLobby == bInLeavingLobby)
	{
		return;
	}

	bLeavingLobby = bInLeavingLobby;
	MARK_PROPERTY_DIRTY_FROM_NAME(
		ULobbyPlayerStateComponent,
		bLeavingLobby,
		this);
	GetOwner()->ForceNetUpdate();
	NotifyLobbyRuntimeStateChanged();
}

void ULobbyPlayerStateComponent::SetNickname(
	const FText& InNickname)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bUseHint =
		!NicknameHint.IsEmpty()
		&& InNickname.EqualTo(NicknameHint);
	SetNicknameInternal(
		InNickname,
		NicknameHint.IsEmpty() ? InNickname : NicknameHint,
		bUseHint);
}

void ULobbyPlayerStateComponent::SetDefaultNickname(
	const FText& InNickname)
{
	if (HasAuthority())
	{
		SetNicknameInternal(
			InNickname,
			InNickname,
			true);
	}
}

void ULobbyPlayerStateComponent::ClearCustomNickname()
{
	if (HasAuthority() && !NicknameHint.IsEmpty())
	{
		SetNicknameInternal(
			NicknameHint,
			NicknameHint,
			true);
	}
}

void ULobbyPlayerStateComponent::SetTeamColorIndex(
	const int32 InTeamColorIndex)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UPlayerMatchComponent* MatchComponent =
		GetPlayerMatchComponent();
		MatchComponent
		&& MatchComponent->GetMatchTeamColorIndex()
			!= InTeamColorIndex)
	{
		MatchComponent->SetMatchTeamColorIndex(
			InTeamColorIndex);
	}
}

int32 ULobbyPlayerStateComponent::GetTeamColorIndex() const
{
	const UPlayerMatchComponent* MatchComponent =
		GetPlayerMatchComponent();
	return MatchComponent
		? MatchComponent->GetMatchTeamColorIndex()
		: INDEX_NONE;
}

void ULobbyPlayerStateComponent::ImportPlayerMatchIdentity(
	const FPlayerMatchIdentity& InMatchIdentity)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UPlayerMatchComponent* MatchComponent =
		GetPlayerMatchComponent())
	{
		MatchComponent->SetPlayerMatchIdentity(
			InMatchIdentity);
	}
	SetNicknameInternal(
		InMatchIdentity.DisplayName,
		InMatchIdentity.DisplayName,
		true);
}

FPlayerMatchIdentity
ULobbyPlayerStateComponent::BuildConfirmedPlayerMatchIdentity() const
{
	const UPlayerMatchComponent* MatchComponent =
		GetPlayerMatchComponent();
	FPlayerMatchIdentity MatchIdentity = MatchComponent
		? MatchComponent->GetPlayerMatchIdentity()
		: FPlayerMatchIdentity();
	MatchIdentity.DisplayName = Nickname.IsEmpty()
		? MatchIdentity.DisplayName
		: Nickname;
	return MatchIdentity;
}

void ULobbyPlayerStateComponent::InitializePreviewAbilitySystem(
	UPdAbilitySystemComponent* AbilitySystemComponent,
	UBasicAttributeSet* BasicAttributeSet,
	const ULobbyPreviewDefinition* PreviewDefinition)
{
	if (!AbilitySystemComponent || !BasicAttributeSet)
	{
		return;
	}

	if (!AbilitySystemComponent->GetAttributeSet(
		UBasicAttributeSet::StaticClass()))
	{
		AbilitySystemComponent->AddSpawnedAttribute(
			BasicAttributeSet);
	}

	if (!HasAuthority())
	{
		return;
	}

	const ULobbyPreviewDefinition* EffectiveDefinition =
		PreviewDefinition
			? PreviewDefinition
			: GetDefault<ULobbyPreviewDefinition>();
	const FLobbyPreviewAttributeSettings& Attributes =
		EffectiveDefinition->GetPreviewAttributeSettings();
	AbilitySystemComponent->ApplyAttributeDefaultValue(
		UBasicAttributeSet::GetMaxHealthAttribute(),
		Attributes.MaxHealth);
	AbilitySystemComponent->ApplyAttributeDefaultValue(
		UBasicAttributeSet::GetHealthAttribute(),
		Attributes.Health);
	AbilitySystemComponent->ApplyAttributeDefaultValue(
		UBasicAttributeSet::GetShieldAttribute(),
		Attributes.Shield);
	AbilitySystemComponent->ApplyAttributeDefaultValue(
		UBasicAttributeSet::GetMaxManaAttribute(),
		Attributes.MaxMana);
	AbilitySystemComponent->ApplyAttributeDefaultValue(
		UBasicAttributeSet::GetManaAttribute(),
		Attributes.Mana);
	AbilitySystemComponent->ApplyAttributeDefaultValue(
		UBasicAttributeSet::GetMaxStaminaAttribute(),
		Attributes.MaxStamina);
	AbilitySystemComponent->ApplyAttributeDefaultValue(
		UBasicAttributeSet::GetStaminaAttribute(),
		Attributes.Stamina);
}

bool ULobbyPlayerStateComponent::HasAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

UPlayerMatchComponent*
ULobbyPlayerStateComponent::GetPlayerMatchComponent() const
{
	const APdPlayerState* PlayerState =
		Cast<APdPlayerState>(GetOwner());
	return PlayerState
		? PlayerState->GetPlayerMatchComponent()
		: nullptr;
}

void ULobbyPlayerStateComponent::SetNicknameInternal(
	const FText& InNickname,
	const FText& InNicknameHint,
	const bool bInUsingNicknameHint)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bNicknameChanged =
		!Nickname.EqualTo(InNickname);
	const bool bHintChanged =
		!NicknameHint.EqualTo(InNicknameHint);
	const bool bUsingHintChanged =
		bUsingNicknameHint != bInUsingNicknameHint;
	if (!bNicknameChanged
		&& !bHintChanged
		&& !bUsingHintChanged)
	{
		return;
	}

	Nickname = InNickname;
	NicknameHint = InNicknameHint;
	bUsingNicknameHint = bInUsingNicknameHint;
	if (UPlayerMatchComponent* MatchComponent =
		GetPlayerMatchComponent())
	{
		MatchComponent->SetMatchDisplayName(Nickname);
	}

	if (bNicknameChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(
			ULobbyPlayerStateComponent,
			Nickname,
			this);
	}
	if (bHintChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(
			ULobbyPlayerStateComponent,
			NicknameHint,
			this);
	}
	if (bUsingHintChanged)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(
			ULobbyPlayerStateComponent,
			bUsingNicknameHint,
			this);
	}

	GetOwner()->ForceNetUpdate();
	NotifyLobbyRuntimeStateChanged();
}

void ULobbyPlayerStateComponent::HandleMatchTeamColorChanged(
	const int32 NewTeamColorIndex)
{
	static_cast<void>(NewTeamColorIndex);
	NotifyLobbyRuntimeStateChanged();
}

void ULobbyPlayerStateComponent::NotifyLobbyRuntimeStateChanged()
{
	OnLobbyRuntimeStateChanged.Broadcast();
}

void ULobbyPlayerStateComponent::OnRep_Ready()
{
	NotifyLobbyRuntimeStateChanged();
}

void ULobbyPlayerStateComponent::OnRep_LeavingLobby()
{
	NotifyLobbyRuntimeStateChanged();
}

void ULobbyPlayerStateComponent::OnRep_Nickname()
{
	NotifyLobbyRuntimeStateChanged();
}
