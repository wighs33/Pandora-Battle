#include "Component/Character/CharacterPresentationComponent.h"

#include "AbilitySystem/Projectiles/ProjectileBase.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Mode/ExperienceGameState.h"
#include "Mode/PdPlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Component/Player/PlayerMatchComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterPresentationComponent)

UCharacterPresentationComponent::UCharacterPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCharacterPresentationComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(
		UCharacterPresentationComponent,
		CurrentAnimLayer,
		Params);
}

void UCharacterPresentationComponent::ApplySettings(
	const FCharacterPresentationSettings& InSettings)
{
	Settings = InSettings;
}

void UCharacterPresentationComponent::InitializePresentation(
	UNiagaraComponent* InDefaultBodyAuraComponent)
{
	DefaultBodyAuraComponent = InDefaultBodyAuraComponent;
	ResetAnimationToDefault();
	BindMatchTeamColorChanged();
	ApplyTeamOverlayMaterial();
}

void UCharacterPresentationComponent::ShutdownPresentation()
{
	UnbindMatchTeamColorChanged();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TeamOverlayMaterialRetryTimerHandle);
	}
	DefaultBodyAuraComponent = nullptr;
	SkillPresentationOverlaySources.Reset();
	SkillPresentationOverlayMaterials.Reset();
	ActiveSkillOverlayMaterial = nullptr;
}

void UCharacterPresentationComponent::ResetAnimationToDefault()
{
	SetCurrentAnimLayer(Settings.DefaultAnimLayer);
}

void UCharacterPresentationComponent::SetCurrentAnimLayer(
	TSubclassOf<UAnimInstance> AnimLayerClass)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character)
	{
		return;
	}

	const TSubclassOf<UAnimInstance> NewAnimLayer =
		AnimLayerClass ? AnimLayerClass : Settings.DefaultAnimLayer;
	const bool bAnimLayerChanged = CurrentAnimLayer != NewAnimLayer;
	CurrentAnimLayer = NewAnimLayer;

	if (bAnimLayerChanged && Character->HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(
			UCharacterPresentationComponent,
			CurrentAnimLayer,
			this);
	}

	LinkAnimLayer(NewAnimLayer);
}

void UCharacterPresentationComponent::OnRep_CurrentAnimLayer()
{
	LinkAnimLayer(
		CurrentAnimLayer
			? CurrentAnimLayer
			: Settings.DefaultAnimLayer);
}

void UCharacterPresentationComponent::LinkAnimLayer(
	TSubclassOf<UAnimInstance> AnimLayerClass) const
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (AnimLayerClass && Character && Character->GetMesh())
	{
		Character->GetMesh()->LinkAnimClassLayers(AnimLayerClass);
	}
}

void UCharacterPresentationComponent::UpdateAimOffset()
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
	if (!Character
		|| (!Character->HasAuthority() && !Character->IsLocallyControlled())
		|| Character->IsStatusFrozen())
	{
		return;
	}

	const FRotator AimDelta = (
		Character->GetBaseAimRotation()
		- Character->GetActorRotation()).GetNormalized();
	AimYaw = AimDelta.Yaw;
	AimPitch = AimDelta.Pitch;
}

void UCharacterPresentationComponent::SetAimOffset(
	const float InAimYaw,
	const float InAimPitch)
{
	AimYaw = InAimYaw;
	AimPitch = InAimPitch;
}

void UCharacterPresentationComponent::BindMatchTeamColorChanged()
{
	ACharacterBase* Character = GetCharacterOwner();
	APdPlayerState* PdPlayerState =
		Character ? Character->GetPlayerState<APdPlayerState>() : nullptr;
	if (TeamColorBoundPlayerState.Get() == PdPlayerState)
	{
		return;
	}

	UnbindMatchTeamColorChanged();
	TeamColorBoundPlayerState = PdPlayerState;
	if (PdPlayerState && PdPlayerState->GetPlayerMatchComponent())
	{
		PdPlayerState->GetPlayerMatchComponent()->OnMatchTeamColorChanged.AddUObject(
			this,
			&ThisClass::HandleMatchTeamColorChanged);
	}
}

void UCharacterPresentationComponent::UnbindMatchTeamColorChanged()
{
	if (APdPlayerState* PdPlayerState = TeamColorBoundPlayerState.Get())
	{
		if (UPlayerMatchComponent* MatchComponent =
			PdPlayerState->GetPlayerMatchComponent())
		{
			MatchComponent->OnMatchTeamColorChanged.RemoveAll(this);
		}
	}
	TeamColorBoundPlayerState.Reset();
}

void UCharacterPresentationComponent::HandleMatchTeamColorChanged(
	const int32 NewTeamColorIndex)
{
	static_cast<void>(NewTeamColorIndex);
	ApplyTeamOverlayMaterial();
}

const UMatchRuleDefinition*
UCharacterPresentationComponent::GetTeamOverlayMatchRuleDefinition() const
{
	if (!Settings.MatchRuleDefinition.IsNull())
	{
		if (const UMatchRuleDefinition* LoadedDefinition =
			Settings.MatchRuleDefinition.Get())
		{
			return LoadedDefinition;
		}
	}

	const ACharacterBase* Character = GetCharacterOwnerConst();
	const AExperienceGameState* ExperienceGameState =
		Character && Character->GetWorld()
			? Character->GetWorld()->GetGameState<AExperienceGameState>()
			: nullptr;
	return ExperienceGameState
		? ExperienceGameState->GetMatchRuleDefinition()
		: nullptr;
}

void UCharacterPresentationComponent::ApplyTeamOverlayMaterial()
{
	RefreshCharacterOverlayMaterial();
}

void UCharacterPresentationComponent::RefreshCharacterOverlayMaterial()
{
	ACharacterBase* Character = GetCharacterOwner();
	USkeletalMeshComponent* CharacterMesh =
		Character ? Character->GetMesh() : nullptr;
	if (!CharacterMesh)
	{
		return;
	}

	if (UMaterialInterface* SkillOverlayMaterial =
		GetPreferredSkillOverlayMaterial())
	{
		CharacterMesh->SetOverlayMaterial(SkillOverlayMaterial);
		return;
	}

	if (!Settings.bApplyTeamOverlayMaterial)
	{
		CharacterMesh->SetOverlayMaterial(nullptr);
		return;
	}

	const APdPlayerState* PdPlayerState =
		Character->GetPlayerState<APdPlayerState>();
	const UPlayerMatchComponent* MatchComponent =
		PdPlayerState ? PdPlayerState->GetPlayerMatchComponent() : nullptr;
	const int32 TeamColorIndex = MatchComponent
		? MatchComponent->GetMatchTeamColorIndex()
		: INDEX_NONE;
	if (TeamColorIndex == INDEX_NONE)
	{
		CharacterMesh->SetOverlayMaterial(nullptr);
		QueueTeamOverlayMaterialRetry();
		return;
	}

	const UMatchRuleDefinition* MatchRules =
		GetTeamOverlayMatchRuleDefinition();
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
	CharacterMesh->SetOverlayMaterial(
		MatchRules->GetTeamOverlayMaterial(TeamColorIndex));
}

UMaterialInterface*
UCharacterPresentationComponent::GetPreferredSkillOverlayMaterial()
{
	const int32 EntryCount = FMath::Min(
		SkillPresentationOverlaySources.Num(),
		SkillPresentationOverlayMaterials.Num());
	for (int32 Index = EntryCount - 1; Index >= 0; --Index)
	{
		if (IsValid(SkillPresentationOverlaySources[Index])
			&& IsValid(SkillPresentationOverlayMaterials[Index]))
		{
			return SkillPresentationOverlayMaterials[Index];
		}

		SkillPresentationOverlaySources.RemoveAt(Index);
		SkillPresentationOverlayMaterials.RemoveAt(Index);
	}
	return ActiveSkillOverlayMaterial;
}

void UCharacterPresentationComponent::ApplySkillOverlayMaterial(
	UMaterialInterface* OverlayMaterial)
{
	ActiveSkillOverlayMaterial = OverlayMaterial;
	RefreshCharacterOverlayMaterial();
}

void UCharacterPresentationComponent::ApplySkillPresentationOverlay(
	UObject* PresentationSource,
	UMaterialInterface* OverlayMaterial)
{
	if (!IsValid(PresentationSource))
	{
		return;
	}

	for (int32 Index = SkillPresentationOverlaySources.Num() - 1;
		Index >= 0;
		--Index)
	{
		if (!IsValid(SkillPresentationOverlaySources[Index])
			|| SkillPresentationOverlaySources[Index] == PresentationSource)
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

void UCharacterPresentationComponent::ClearSkillPresentationOverlay(
	UObject* PresentationSource)
{
	for (int32 Index = SkillPresentationOverlaySources.Num() - 1;
		Index >= 0;
		--Index)
	{
		if (!IsValid(SkillPresentationOverlaySources[Index])
			|| SkillPresentationOverlaySources[Index] == PresentationSource)
		{
			SkillPresentationOverlaySources.RemoveAt(Index);
			if (SkillPresentationOverlayMaterials.IsValidIndex(Index))
			{
				SkillPresentationOverlayMaterials.RemoveAt(Index);
			}
		}
	}

	while (SkillPresentationOverlayMaterials.Num()
		> SkillPresentationOverlaySources.Num())
	{
		SkillPresentationOverlayMaterials.Pop();
	}
	RefreshCharacterOverlayMaterial();
}

void UCharacterPresentationComponent::ClearCharacterOverlayMaterialLocal()
{
	ActiveSkillOverlayMaterial = nullptr;
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
		|| World->GetTimerManager().IsTimerActive(
			TeamOverlayMaterialRetryTimerHandle)
		|| (Settings.TeamOverlayMaterialRetryAttempts > 0
			&& TeamOverlayMaterialRetryCount
				>= Settings.TeamOverlayMaterialRetryAttempts))
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
			ApplyTeamOverlayMaterial();
		}),
		FMath::Max(Settings.TeamOverlayMaterialRetryInterval, 0.01f),
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

FVector UCharacterPresentationComponent::GetClampedBodyAuraRelativeLocationOffset(
	FVector RelativeLocationOffset) const
{
	if (RelativeLocationOffset.ContainsNaN())
	{
		return FVector::ZeroVector;
	}

	const float MaxOffsetDistance =
		FMath::Max(Settings.MaxBodyAuraRelativeOffsetDistance, 0.0f);
	return MaxOffsetDistance > 0.0f
		? RelativeLocationOffset.GetClampedToMaxSize(MaxOffsetDistance)
		: FVector::ZeroVector;
}

FVector UCharacterPresentationComponent::GetClampedBodyAuraRelativeScale(
	FVector RelativeScale) const
{
	if (RelativeScale.ContainsNaN())
	{
		return FVector::OneVector;
	}

	const float MaxScale =
		FMath::Max(Settings.MaxBodyAuraRelativeScale, 0.01f);
	return FVector(
		FMath::Clamp(RelativeScale.X, 0.0f, MaxScale),
		FMath::Clamp(RelativeScale.Y, 0.0f, MaxScale),
		FMath::Clamp(RelativeScale.Z, 0.0f, MaxScale));
}

UNiagaraComponent*
UCharacterPresentationComponent::FindBodyAuraNiagaraComponent(
	const FName ComponentName) const
{
	const ACharacterBase* Character = GetCharacterOwnerConst();
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

void UCharacterPresentationComponent::ApplyBodyAuraNiagara(
	const FName ComponentName,
	UNiagaraSystem* NiagaraSystem,
	const bool bActivate,
	const bool bResetSystem)
{
	UNiagaraComponent* AuraComponent =
		FindBodyAuraNiagaraComponent(ComponentName);
	if (!AuraComponent)
	{
		return;
	}

	AuraComponent->SetAsset(NiagaraSystem);
	if (NiagaraSystem && bActivate)
	{
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

void UCharacterPresentationComponent::ApplyBodyAuraNiagaraWithOffset(
	const FName ComponentName,
	UNiagaraSystem* NiagaraSystem,
	const bool bActivate,
	const bool bResetSystem,
	const FVector RelativeLocationOffset,
	const FVector RelativeScale)
{
	UNiagaraComponent* AuraComponent =
		FindBodyAuraNiagaraComponent(ComponentName);
	if (!AuraComponent)
	{
		return;
	}

	AuraComponent->SetAsset(NiagaraSystem);
	if (NiagaraSystem && bActivate)
	{
		AuraComponent->SetRelativeLocation(
			GetClampedBodyAuraRelativeLocationOffset(RelativeLocationOffset));
		AuraComponent->SetRelativeScale3D(
			GetClampedBodyAuraRelativeScale(RelativeScale));
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
	const FName ComponentName,
	const UNiagaraSystem* ExpectedNiagaraSystem)
{
	UNiagaraComponent* AuraComponent =
		FindBodyAuraNiagaraComponent(ComponentName);
	if (!AuraComponent
		|| (ExpectedNiagaraSystem
			&& AuraComponent->GetAsset() != ExpectedNiagaraSystem))
	{
		return;
	}

	AuraComponent->Deactivate();
	AuraComponent->SetAsset(nullptr);
}

void UCharacterPresentationComponent::SpawnProjectileCosmetic(
	TSubclassOf<AProjectileBase> ProjectileClass,
	const FVector SpawnLocation,
	const FRotator SpawnRotation,
	const FVector TargetLocation,
	const float Speed,
	const bool bUseArcTrajectory,
	const float ArcHeight,
	const float ArcGravityScale,
	UNiagaraSystem* MuzzleFX,
	UNiagaraSystem* ProjectileFX,
	UNiagaraSystem* HitFX,
	const bool bSpawnHitNiagaraOnGround,
	const FGameplayTag SpawnGameplayCueTag,
	const FGameplayTag ImpactGameplayCueTag,
	const FVector SpawnScale,
	const FName NiagaraVector2DParameterName,
	const FVector2D NiagaraSize,
	const float LifeSpan)
{
	ACharacterBase* Character = GetCharacterOwner();
	if (!Character
		|| Character->HasAuthority()
		|| Character->GetNetMode() == NM_DedicatedServer
		|| !ProjectileClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform SpawnTransform(SpawnRotation, SpawnLocation);
	AProjectileBase* Projectile =
		World->SpawnActorDeferred<AProjectileBase>(
			ProjectileClass,
			SpawnTransform,
			Character,
			Character,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		return;
	}

	Projectile->SetReplicates(false);
	Projectile->SetReplicateMovement(false);
	Projectile->ConfigureProjectileVisuals(
		MuzzleFX,
		ProjectileFX,
		HitFX,
		bSpawnHitNiagaraOnGround,
		SpawnGameplayCueTag,
		ImpactGameplayCueTag);
	Projectile->ConfigureArcTrajectory(
		bUseArcTrajectory,
		ArcHeight,
		ArcGravityScale);
	Projectile->InitializeCosmeticProjectile(
		TargetLocation,
		Speed,
		LifeSpan);
	Projectile->StartReadiedScaleGrowth(
		SpawnScale,
		SpawnScale,
		0.0f,
		NiagaraVector2DParameterName,
		NiagaraSize,
		NiagaraSize);
	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
}

ACharacterBase* UCharacterPresentationComponent::GetCharacterOwner() const
{
	return Cast<ACharacterBase>(GetOwner());
}

const ACharacterBase*
UCharacterPresentationComponent::GetCharacterOwnerConst() const
{
	return Cast<ACharacterBase>(GetOwner());
}
