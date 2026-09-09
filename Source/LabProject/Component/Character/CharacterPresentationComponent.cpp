#include "Component/Character/CharacterPresentationComponent.h"

#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Engine/GameInstance.h"
#include "Lobby/LobbyRuntimeSubsystem.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Component/Player/PlayerMatchComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterPresentationComponent)

namespace
{
	constexpr float TeamOverlayMaterialRetryInterval = 0.2f;
	constexpr int32 TeamOverlayMaterialRetryAttempts = 20;
	constexpr float MaxBodyAuraRelativeOffsetDistance = 600.0f;
	constexpr float MaxBodyAuraRelativeScale = 10.0f;
}

UCharacterPresentationComponent::UCharacterPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCharacterPresentationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UCharacterPresentationComponent, CurrentAnimLayer, Params);
}

void UCharacterPresentationComponent::ApplySettings(const FCharacterPresentationSettings& InSettings)
{
	Settings = InSettings;
}

void UCharacterPresentationComponent::InitializePresentation(UNiagaraComponent* InDefaultBodyAuraComponent)
{
	DefaultBodyAuraComponent = InDefaultBodyAuraComponent;
	ResetAnimationToDefault();
	BindMatchTeamColorChanged();
	RefreshCharacterOverlayMaterial();
}

void UCharacterPresentationComponent::ShutdownPresentation()
{
	TemporaryMeshScaleMultipliers.Reset();
	RefreshTemporaryMeshScale();
	UnbindMatchTeamColorChanged();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TeamOverlayMaterialRetryTimerHandle);
	}
	DefaultBodyAuraComponent = nullptr;
	SkillPresentationOverlaySources.Reset();
	SkillPresentationOverlayMaterials.Reset();
}

void UCharacterPresentationComponent::ResetAnimationToDefault()
{
	SetCurrentAnimLayer(Settings.DefaultAnimLayer);
}

void UCharacterPresentationComponent::SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	const TSubclassOf<UAnimInstance> NewAnimLayer = AnimLayerClass ? AnimLayerClass : Settings.DefaultAnimLayer;
	const bool bAnimLayerChanged = CurrentAnimLayer != NewAnimLayer;
	CurrentAnimLayer = NewAnimLayer;

	if (bAnimLayerChanged && Character->HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UCharacterPresentationComponent, CurrentAnimLayer, this);
	}

	LinkAnimLayer(NewAnimLayer);
}

void UCharacterPresentationComponent::OnRep_CurrentAnimLayer()
{
	LinkAnimLayer(CurrentAnimLayer ? CurrentAnimLayer : Settings.DefaultAnimLayer);
}

void UCharacterPresentationComponent::LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (AnimLayerClass && Character && Character->GetMesh())
	{
		Character->GetMesh()->LinkAnimClassLayers(AnimLayerClass);
	}
}

void UCharacterPresentationComponent::UpdateAimOffset()
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (!Character
		|| (!Character->HasAuthority() && !Character->IsLocallyControlled())
		|| Character->IsStatusFrozen())
	{
		return;
	}

	const FRotator AimDelta = (Character->GetBaseAimRotation() - Character->GetActorRotation()).GetNormalized();
	AimYaw = AimDelta.Yaw;
	AimPitch = AimDelta.Pitch;
}

void UCharacterPresentationComponent::SetAimOffset(const float InAimYaw, const float InAimPitch)
{
	AimYaw = InAimYaw;
	AimPitch = InAimPitch;
}

void UCharacterPresentationComponent::BindMatchTeamColorChanged()
{
	ACharacterBase* Character = GetCharacterOwner();
	APdPlayerState* PdPlayerState = Character ? Character->GetPlayerState<APdPlayerState>() : nullptr;
	if (TeamColorBoundPlayerState.Get() == PdPlayerState)
	{
		return;
	}

	UnbindMatchTeamColorChanged();
	TeamColorBoundPlayerState = PdPlayerState;
	if (PdPlayerState && PdPlayerState->GetPlayerMatchComponent())
	{
		PdPlayerState->GetPlayerMatchComponent()->OnMatchTeamColorChanged.AddUObject(
			this, &ThisClass::HandleMatchTeamColorChanged);
	}
}

void UCharacterPresentationComponent::UnbindMatchTeamColorChanged()
{
	if (APdPlayerState* PdPlayerState = TeamColorBoundPlayerState.Get())
	{
		if (UPlayerMatchComponent* MatchComponent = PdPlayerState->GetPlayerMatchComponent())
		{
			MatchComponent->OnMatchTeamColorChanged.RemoveAll(this);
		}
	}
	TeamColorBoundPlayerState.Reset();
}

void UCharacterPresentationComponent::HandleMatchTeamColorChanged(const int32 NewTeamColorIndex)
{
	static_cast<void>(NewTeamColorIndex);
	RefreshCharacterOverlayMaterial();
}

const UMatchRuleDefinition* UCharacterPresentationComponent::GetTeamOverlayMatchRuleDefinition() const
{
	const ACharacterBase* Character = GetCharacterOwner();
	const AExperienceGameState* ExperienceGameState =
		Character && Character->GetWorld() ? Character->GetWorld()->GetGameState<AExperienceGameState>() : nullptr;
	if (ExperienceGameState && ExperienceGameState->GetMatchRuleDefinition())
	{
		return ExperienceGameState->GetMatchRuleDefinition();
	}

	const UGameInstance* GameInstance = Character ? Character->GetGameInstance() : nullptr;
	const ULobbyRuntimeSubsystem* LobbyRuntimeSubsystem = 
		GameInstance ? GameInstance->GetSubsystem<ULobbyRuntimeSubsystem>() : nullptr;
	if (const UMatchRuleDefinition* LobbyMatchRules =
		LobbyRuntimeSubsystem ? LobbyRuntimeSubsystem->GetLoadedLobbyMatchRuleDefinition() : nullptr)
	{
		return LobbyMatchRules;
	}

	return UMatchRuleDefinition::ResolveDefaultDefinition();
}

void UCharacterPresentationComponent::RefreshCharacterOverlayMaterial()
{
	ACharacterBase* Character = GetCharacterOwner();
	USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return;
	}

	if (UMaterialInterface* SkillOverlayMaterial = GetPreferredSkillOverlayMaterial())
	{
		CharacterMesh->SetOverlayMaterial(SkillOverlayMaterial);
		return;
	}

	const APdPlayerState* PdPlayerState = Character->GetPlayerState<APdPlayerState>();
	const UPlayerMatchComponent* MatchComponent = PdPlayerState ? PdPlayerState->GetPlayerMatchComponent() : nullptr;
	const int32 TeamColorIndex = MatchComponent ? MatchComponent->GetMatchTeamColorIndex() : INDEX_NONE;
	if (TeamColorIndex == INDEX_NONE)
	{
		CharacterMesh->SetOverlayMaterial(nullptr);
		QueueTeamOverlayMaterialRetry();
		return;
	}

	const UMatchRuleDefinition* MatchRules = GetTeamOverlayMatchRuleDefinition();
	if (!MatchRules)
	{
		CharacterMesh->SetOverlayMaterial(nullptr);
		QueueTeamOverlayMaterialRetry();
		return;
	}

	TeamOverlayMaterialRetryCount = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TeamOverlayMaterialRetryTimerHandle);
	}
	CharacterMesh->SetOverlayMaterial(MatchRules->GetTeamOverlayMaterial(TeamColorIndex));
}

UMaterialInterface* UCharacterPresentationComponent::GetPreferredSkillOverlayMaterial()
{
	const int32 EntryCount = FMath::Min(SkillPresentationOverlaySources.Num(), SkillPresentationOverlayMaterials.Num());
	for (int32 Index = EntryCount - 1; Index >= 0; --Index)
	{
		if (IsValid(SkillPresentationOverlaySources[Index]) && IsValid(SkillPresentationOverlayMaterials[Index]))
		{
			return SkillPresentationOverlayMaterials[Index];
		}

		SkillPresentationOverlaySources.RemoveAt(Index);
		SkillPresentationOverlayMaterials.RemoveAt(Index);
	}
	return nullptr;
}

void UCharacterPresentationComponent::ApplySkillPresentationOverlay(UObject* PresentationSource, UMaterialInterface* OverlayMaterial)
{
	if (!IsValid(PresentationSource))
	{
		return;
	}

	for (int32 Index = SkillPresentationOverlaySources.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(SkillPresentationOverlaySources[Index]) || SkillPresentationOverlaySources[Index] == PresentationSource)
		{
			SkillPresentationOverlaySources.RemoveAt(Index);
			if (SkillPresentationOverlayMaterials.IsValidIndex(Index))
			{
				SkillPresentationOverlayMaterials.RemoveAt(Index);
			}
		}
	}

	if (IsValid(OverlayMaterial))
	{
		SkillPresentationOverlaySources.Add(PresentationSource);
		SkillPresentationOverlayMaterials.Add(OverlayMaterial);
	}
	RefreshCharacterOverlayMaterial();
}

void UCharacterPresentationComponent::ClearSkillPresentationOverlay(UObject* PresentationSource)
{
	for (int32 Index = SkillPresentationOverlaySources.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(SkillPresentationOverlaySources[Index]) || SkillPresentationOverlaySources[Index] == PresentationSource)
		{
			SkillPresentationOverlaySources.RemoveAt(Index);
			if (SkillPresentationOverlayMaterials.IsValidIndex(Index))
			{
				SkillPresentationOverlayMaterials.RemoveAt(Index);
			}
		}
	}

	while (SkillPresentationOverlayMaterials.Num() > SkillPresentationOverlaySources.Num())
	{
		SkillPresentationOverlayMaterials.Pop();
	}
	RefreshCharacterOverlayMaterial();
}

void UCharacterPresentationComponent::ClearCharacterOverlayMaterialLocal()
{
	SkillPresentationOverlaySources.Reset();
	SkillPresentationOverlayMaterials.Reset();
	if (ACharacterBase* Character = GetCharacterOwner())
	{
		if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
		{
			CharacterMesh->SetOverlayMaterial(nullptr);
		}
	}
}

void UCharacterPresentationComponent::QueueTeamOverlayMaterialRetry()
{
	UWorld* World = GetWorld();
	if (!World
		|| World->GetTimerManager().IsTimerActive(TeamOverlayMaterialRetryTimerHandle)
		|| TeamOverlayMaterialRetryCount >= TeamOverlayMaterialRetryAttempts)
	{
		return;
	}

	++TeamOverlayMaterialRetryCount;
	World->GetTimerManager().SetTimer(
		TeamOverlayMaterialRetryTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			TeamOverlayMaterialRetryTimerHandle.Invalidate();
			BindMatchTeamColorChanged();
			RefreshCharacterOverlayMaterial();
		}),
		TeamOverlayMaterialRetryInterval,
		false);
}

void UCharacterPresentationComponent::HandleDashGameplayCue(
	const EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	const bool bShouldHideCharacterMesh = Parameters.RawMagnitude >= 0.0f;
	if (EventType == EGameplayCueEvent::OnActive)
	{
		if (bShouldHideCharacterMesh && Character->GetMesh())
		{
			Character->GetMesh()->SetVisibility(false, true);
		}
		Character->OnDashCueActivated(Parameters);
		return;
	}

	if (EventType == EGameplayCueEvent::Removed)
	{
		if (bShouldHideCharacterMesh && Character->GetMesh())
		{
			Character->GetMesh()->SetVisibility(true, true);
		}
		Character->OnDashCueRemoved(Parameters);
	}
}

FVector UCharacterPresentationComponent::GetClampedBodyAuraRelativeLocationOffset(FVector RelativeLocationOffset) const
{
	if (RelativeLocationOffset.ContainsNaN())
	{
		return FVector::ZeroVector;
	}

	const float MaxOffsetDistance = MaxBodyAuraRelativeOffsetDistance;
	return MaxOffsetDistance > 0.0f ? RelativeLocationOffset.GetClampedToMaxSize(MaxOffsetDistance) : FVector::ZeroVector;
}

FVector UCharacterPresentationComponent::GetClampedBodyAuraRelativeScale(FVector RelativeScale) const
{
	if (RelativeScale.ContainsNaN())
	{
		return FVector::OneVector;
	}

	const float MaxScale = MaxBodyAuraRelativeScale;
	return FVector(
		FMath::Clamp(RelativeScale.X, 0.0f, MaxScale),
		FMath::Clamp(RelativeScale.Y, 0.0f, MaxScale),
		FMath::Clamp(RelativeScale.Z, 0.0f, MaxScale));
}

UNiagaraComponent* UCharacterPresentationComponent::FindBodyAuraNiagaraComponent(const FName ComponentName) const
{
	const ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return DefaultBodyAuraComponent.Get();
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	Character->GetComponents<UNiagaraComponent>(NiagaraComponents);
	if (!ComponentName.IsNone())
	{
		for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
		{
			if (NiagaraComponent
				&& (NiagaraComponent->GetFName() == ComponentName
					|| NiagaraComponent->ComponentHasTag(ComponentName)))
			{
				return NiagaraComponent;
			}
		}
	}
	return DefaultBodyAuraComponent.Get();
}

void UCharacterPresentationComponent::ApplyBodyAuraNiagaraWithOffset(
	const FName ComponentName,
	UNiagaraSystem* NiagaraSystem,
	const bool bActivate,
	const bool bResetSystem,
	const FVector RelativeLocationOffset,
	const FVector RelativeScale)
{
	UNiagaraComponent* AuraComponent = FindBodyAuraNiagaraComponent(ComponentName);
	if (!AuraComponent)
	{
		return;
	}

	AuraComponent->SetAsset(NiagaraSystem);
	if (NiagaraSystem && bActivate)
	{
		AuraComponent->SetRelativeLocation(GetClampedBodyAuraRelativeLocationOffset(RelativeLocationOffset));
		AuraComponent->SetRelativeScale3D(GetClampedBodyAuraRelativeScale(RelativeScale));
		if (bResetSystem)
		{
			AuraComponent->ResetSystem();
		}
		AuraComponent->Activate(true);
	}
	else
	{
		AuraComponent->Deactivate();
	}
}

void UCharacterPresentationComponent::ClearBodyAuraNiagaraIfMatching(
	const FName ComponentName, const UNiagaraSystem* ExpectedNiagaraSystem)
{
	UNiagaraComponent* AuraComponent = FindBodyAuraNiagaraComponent(ComponentName);
	if (!AuraComponent || (ExpectedNiagaraSystem && AuraComponent->GetAsset() != ExpectedNiagaraSystem))
	{
		return;
	}

	AuraComponent->Deactivate();
	AuraComponent->SetAsset(nullptr);
}

ACharacterBase* UCharacterPresentationComponent::GetCharacterOwner() const
{
	return Cast<ACharacterBase>(GetOwner());
}

// 원래 메시 크기를 한 번만 보관하고, 남은 스킬 배율들의 곱으로 현재 크기를 결정한다.
void UCharacterPresentationComponent::SetTemporaryMeshScaleMultiplier(UObject* SourceObject, const float ScaleMultiplier)
{
	if (!IsValid(SourceObject) || !FMath::IsFinite(ScaleMultiplier))
	{
		return;
	}
	if (ScaleMultiplier <= 1.0f)
	{
		ClearTemporaryMeshScaleMultiplier(SourceObject);
		return;
	}
	if (!ScaledMeshComponent.IsValid())
	{
		ACharacterBase* Character = GetCharacterOwner();
		USkeletalMeshComponent* MeshComponent = Character ? Character->GetMesh() : nullptr;
		if (!MeshComponent)
		{
			return;
		}
		TemporaryMeshScaleMultipliers.Reset();
		ScaledMeshComponent = MeshComponent;
		MeshRelativeScaleBeforeModifiers = MeshComponent->GetRelativeScale3D();
	}
	TemporaryMeshScaleMultipliers.Add(FObjectKey(SourceObject), ScaleMultiplier);
	RefreshTemporaryMeshScale();
}

// 종료된 스킬의 기여만 제거하며, 다른 스킬의 확대 효과는 유지한다.
void UCharacterPresentationComponent::ClearTemporaryMeshScaleMultiplier(UObject* SourceObject)
{
	if (SourceObject)
	{
		TemporaryMeshScaleMultipliers.Remove(FObjectKey(SourceObject));
	}
	RefreshTemporaryMeshScale();
}

void UCharacterPresentationComponent::RefreshTemporaryMeshScale()
{
	float CombinedMultiplier = 1.0f;
	for (auto It = TemporaryMeshScaleMultipliers.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Key().ResolveObjectPtr()))
		{
			It.RemoveCurrent();
			continue;
		}
		CombinedMultiplier *= It.Value();
	}
	if (USkeletalMeshComponent* MeshComponent = ScaledMeshComponent.Get())
	{
		MeshComponent->SetRelativeScale3D(MeshRelativeScaleBeforeModifiers * CombinedMultiplier);
	}
	if (TemporaryMeshScaleMultipliers.IsEmpty())
	{
		ScaledMeshComponent.Reset();
		MeshRelativeScaleBeforeModifiers = FVector::OneVector;
	}
}
