#include "Character/CharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Common/CollisionChannels.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Component/Character/CharacterAbilityRuntimeComponent.h"
#include "Component/Character/CharacterDeathComponent.h"
#include "Component/Character/CharacterHealthBarComponent.h"
#include "Component/Character/CharacterPresentationComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Component/UI/DamageIndicatorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Definition/Character/CharacterBaseDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Mode/PdPlayerState.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterBase)

DEFINE_LOG_CATEGORY_STATIC(LogCharacterBaseRuntime, Log, All);

namespace
{
template <typename ComponentType>
ComponentType* FindConfiguredComponent(const AActor* Owner, ComponentType* DefaultComponent)
{
	if (!Owner)
	{
		return DefaultComponent;
	}

	TArray<ComponentType*> Components;
	Owner->GetComponents<ComponentType>(Components);
	for (ComponentType* Component : Components)
	{
		if (Component && Component != DefaultComponent)
		{
			return Component;
		}
	}

	return DefaultComponent ? DefaultComponent : (Components.IsEmpty() ? nullptr : Components[0]);
}

UCombatComponent* FindCombatComponent(const AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}

	TArray<UCombatComponent*> Components;
	Owner->GetComponents<UCombatComponent>(Components);
	if (Components.IsEmpty())
	{
		return nullptr;
	}

	for (UCombatComponent* Component : Components)
	{
		if (Component && Component->CreationMethod == EComponentCreationMethod::Instance)
		{
			return Component;
		}
	}
	return Components[0];
}
} // namespace

ACharacterBase::ACharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (MovementComponent)
	{
		MovementComponent->bOrientRotationToMovement = true;
		MovementComponent->RotationRate = FRotator(0.0f, 700.0f, 0.0f);
		MovementComponent->MaxWalkSpeed = 450.0f;
	}

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;
		CharacterMesh->SetCollisionObjectType(LabCollisionChannels::HitableBody());
	}
	ApplySkillDamageCollisionToCharacterComponents();

	CharacterAbilityRuntimeComponent = CreateDefaultSubobject<UCharacterAbilityRuntimeComponent>(TEXT("CharacterAbilityRuntimeComponent"));
	CharacterDeathComponent = CreateDefaultSubobject<UCharacterDeathComponent>(TEXT("CharacterDeathComponent"));
	CharacterPresentationComponent = CreateDefaultSubobject<UCharacterPresentationComponent>(TEXT("CharacterPresentationComponent"));
	StatusEffectReplicationComponent = CreateDefaultSubobject<UStatusEffectReplicationComponent>(TEXT("StatusEffectReplicationComponent"));

	BodyAuraNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AuraNiagara"));
	BodyAuraNiagaraComponent->SetupAttachment(GetMesh());
	BodyAuraNiagaraComponent->SetAutoActivate(false);

	UCharacterHealthBarComponent* CharacterHealthBar = CreateDefaultSubobject<UCharacterHealthBarComponent>(TEXT("WidgetComponent"));
	CharacterHealthBar->SetupAttachment(GetRootComponent());
	HealthBarWidget = CharacterHealthBar;

	SkinEquipmentComponent = CreateDefaultSubobject<USkinEquipmentComponent>(TEXT("SkinEquipmentComponent"));

	CharacterDefinition = TSoftObjectPtr<UCharacterBaseDefinition>(UCharacterBaseDefinition::GetDefaultDefinitionPath());

	ApplyCameraCollisionIgnoreToCharacterComponents();
}

void ACharacterBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	ApplyCharacterDefinition();
	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->CaptureBaseMovementSpeed();
	}
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
	BeginCharacterDefinitionPreload();
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();
	bCharacterBeginPlayCalled = true;

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;
	}
	ApplyCameraCollisionIgnoreToCharacterComponents();
	// Blueprint component templates may still contain the legacy collision
	// responses, so enforce the shared skill-hit policy after deserialization.
	ApplySkillDamageCollisionToCharacterComponents();

	TryInitializeCharacterRuntime();
}

void ACharacterBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->TickRuntime();
	}
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->TickRuntime(DeltaSeconds);
	}
	RefreshCharacterTickEnabled();
}

void ACharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseCharacterDefinitionPreload();
	bCharacterBeginPlayCalled = false;
	bCharacterDefinitionReady = false;
	bCharacterRuntimeInitialized = false;
	LoadedCharacterDefinition = nullptr;

	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->ShutdownRuntime();
	}
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ShutdownPresentation();
	}
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->ShutdownHealthBar();
	}
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ShutdownDeathRuntime();
	}

	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

void ACharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (bCharacterRuntimeInitialized)
	{
		InitializeAbilitySystemActorInfo();
		if (CharacterPresentationComponent)
		{
			CharacterPresentationComponent->BindMatchTeamColorChanged();
			CharacterPresentationComponent->ApplyTeamOverlayMaterial();
		}
	}
	RefreshCharacterTickEnabled();
}

void ACharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (bCharacterRuntimeInitialized)
	{
		InitializeAbilitySystemActorInfo();
		if (CharacterPresentationComponent)
		{
			CharacterPresentationComponent->BindMatchTeamColorChanged();
			CharacterPresentationComponent->ApplyTeamOverlayMaterial();
		}
	}
	RefreshCharacterTickEnabled();
}

void ACharacterBase::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	RefreshCharacterTickEnabled();
}

void ACharacterBase::OnMovementModeChanged(const EMovementMode PrevMovementMode, const uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->HandleMovementModeChanged();
	}
}

void ACharacterBase::UnPossessed()
{
	Super::UnPossessed();
	ClearAbilitySystemActorInfo();
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->UnbindMatchTeamColorChanged();
	}
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->ShutdownHealthBar();
	}
	RefreshCharacterTickEnabled();
}

void ACharacterBase::ApplyCharacterDefinition()
{
	LoadedCharacterDefinition = CharacterDefinition.Get();
	const UCharacterBaseDefinition* ResolvedDefinition = LoadedCharacterDefinition;
	if (!ResolvedDefinition)
	{
		ResolvedDefinition = GetDefault<UCharacterBaseDefinition>();
	}
	const FCharacterPresentationSettings PresentationSettings =
		ResolvedDefinition ? ResolvedDefinition->GetPresentationSettings() : FCharacterPresentationSettings();
	const FCharacterDeathSettings DeathSettings = ResolvedDefinition ? ResolvedDefinition->GetDeathSettings() : FCharacterDeathSettings();

	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ApplySettings(PresentationSettings);
	}
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ApplySettings(DeathSettings);
	}
}

void ACharacterBase::BeginCharacterDefinitionPreload()
{
	ReleaseCharacterDefinitionPreload();
	bCharacterDefinitionReady = false;

	if (CharacterDefinition.IsNull())
	{
		bCharacterDefinitionReady = true;
		return;
	}

	if (UCharacterBaseDefinition* LoadedDefinition = CharacterDefinition.Get())
	{
		LoadedCharacterDefinition = LoadedDefinition;
		bCharacterDefinitionReady = true;
		return;
	}

	const uint32 RequestGeneration = CharacterDefinitionLoadGeneration;
	TSharedPtr<FStreamableHandle> NewLoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			CharacterDefinition.ToSoftObjectPath(),
			FStreamableDelegate::CreateWeakLambda(
				this,
				[this, RequestGeneration]()
				{
					HandleCharacterDefinitionPreloaded(RequestGeneration);
				}));

	if (NewLoadHandle.IsValid()
		&& RequestGeneration == CharacterDefinitionLoadGeneration
		&& !bCharacterDefinitionReady)
	{
		CharacterDefinitionLoadHandle = MoveTemp(NewLoadHandle);
	}
	else if (NewLoadHandle.IsValid())
	{
		NewLoadHandle->ReleaseHandle();
	}
	else
	{
		HandleCharacterDefinitionPreloaded(RequestGeneration);
	}
}

void ACharacterBase::HandleCharacterDefinitionPreloaded(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != CharacterDefinitionLoadGeneration)
	{
		return;
	}

	LoadedCharacterDefinition = CharacterDefinition.Get();
	bCharacterDefinitionReady = true;
	if (!LoadedCharacterDefinition)
	{
		UE_LOG(
			LogCharacterBaseRuntime,
			Error,
			TEXT("Character definition '%s' did not resolve after asynchronous preload; native defaults will be used."),
			*CharacterDefinition.ToString());
	}

	ApplyCharacterDefinition();
	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->CaptureBaseMovementSpeed();
	}
	TryInitializeCharacterRuntime();
}

void ACharacterBase::ReleaseCharacterDefinitionPreload()
{
	++CharacterDefinitionLoadGeneration;
	if (CharacterDefinitionLoadHandle.IsValid())
	{
		CharacterDefinitionLoadHandle->CancelHandle();
		CharacterDefinitionLoadHandle->ReleaseHandle();
		CharacterDefinitionLoadHandle.Reset();
	}
}

void ACharacterBase::TryInitializeCharacterRuntime()
{
	if (bCharacterRuntimeInitialized
		|| !bCharacterBeginPlayCalled
		|| !bCharacterDefinitionReady
		|| !IsAdditionalCharacterRuntimeContentReady())
	{
		return;
	}

	ApplyCharacterDefinition();
	bCharacterRuntimeInitialized = true;

	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->InitializeDeathRuntime();
	}

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(
		this,
		UGameFrameworkComponentManager::NAME_GameActorReady);

	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->CaptureBaseMovementSpeed();
		CharacterAbilityRuntimeComponent->InitializeAbilitySystemActorInfo();
	}
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->InitializePresentation(BodyAuraNiagaraComponent);
		if (Controller || GetPlayerState())
		{
			CharacterPresentationComponent->BindMatchTeamColorChanged();
			CharacterPresentationComponent->ApplyTeamOverlayMaterial();
		}
	}
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->InitializeHealthBar();
	}

	HandleCharacterRuntimeInitialized();
	RefreshCharacterTickEnabled();
}

bool ACharacterBase::IsAdditionalCharacterRuntimeContentReady() const
{
	return true;
}

void ACharacterBase::HandleCharacterRuntimeInitialized()
{
}

UAbilitySystemComponent* ACharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}

UPdAbilitySystemComponent* ACharacterBase::GetPdAbilitySystemComponent() const
{
	return Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponent());
}

void ACharacterBase::InitializeAbilitySystemActorInfo()
{
	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->InitializeAbilitySystemActorInfo();
	}
}

void ACharacterBase::ClearAbilitySystemActorInfo()
{
	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->ClearAbilitySystemActorInfo();
	}
}

AActor* ACharacterBase::GetAbilitySystemOwnerActor() const
{
	return const_cast<ACharacterBase*>(this);
}

AActor* ACharacterBase::GetAbilitySystemAvatarActor() const
{
	return const_cast<ACharacterBase*>(this);
}

UCharacterHealthBarComponent* ACharacterBase::GetCharacterHealthBarComponent() const
{
	return Cast<UCharacterHealthBarComponent>(HealthBarWidget.Get());
}

UEquipmentComponent* ACharacterBase::GetEquipmentComponent() const
{
	return FindConfiguredComponent(this, EquipmentComponent.Get());
}

UCombatComponent* ACharacterBase::GetCombatComponent() const
{
	return FindCombatComponent(this);
}

UDamageIndicatorComponent* ACharacterBase::GetDamageIndicatorComponent() const
{
	return FindConfiguredComponent(this, DamageIndicatorComponent.Get());
}

void ACharacterBase::RefreshHealthBarViewModel()
{
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->RefreshViewModel();
	}
}

UHealthBarViewModel* ACharacterBase::GetHealthBarViewModel() const
{
	const UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent();
	return CharacterHealthBar ? CharacterHealthBar->GetHealthBarViewModel() : nullptr;
}

void ACharacterBase::UpdateHealthBarVisibilityForLocalViewer(
	APlayerController* LocalPlayerController,
	const FVector& CameraLocation,
	const FRotator& CameraRotation,
	const float MaxDistanceSquared)
{
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->UpdateVisibilityForLocalViewer(LocalPlayerController, CameraLocation, CameraRotation, MaxDistanceSquared);
	}
}

void ACharacterBase::SetHealthBarVisibleForLocalViewer(const bool bVisible)
{
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->SetVisibleForLocalViewer(bVisible);
	}
}

float ACharacterBase::GetAimYawForAnimation() const
{
	return CharacterPresentationComponent ? CharacterPresentationComponent->GetAimYaw() : 0.0f;
}

float ACharacterBase::GetAimPitchForAnimation() const
{
	return CharacterPresentationComponent ? CharacterPresentationComponent->GetAimPitch() : 0.0f;
}

void ACharacterBase::ResetAnimationToDefault()
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ResetAnimationToDefault();
	}
}

void ACharacterBase::SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->SetCurrentAnimLayer(AnimLayerClass);
	}
}

void ACharacterBase::LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->LinkAnimLayer(AnimLayerClass);
	}
}

void ACharacterBase::UpdateAimOffsetForAnimation()
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->UpdateAimOffset();
	}
}

void ACharacterBase::SetAimOffsetForAnimation(const float AimYaw, const float AimPitch)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->SetAimOffset(AimYaw, AimPitch);
	}
}

void ACharacterBase::ApplyTeamOverlayMaterial()
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ApplyTeamOverlayMaterial();
	}
}

void ACharacterBase::ApplySkillPresentationOverlay(UObject* PresentationSource, UMaterialInterface* OverlayMaterial)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ApplySkillPresentationOverlay(PresentationSource, OverlayMaterial);
	}
}

void ACharacterBase::ClearSkillPresentationOverlay(UObject* PresentationSource)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ClearSkillPresentationOverlay(PresentationSource);
	}
}

UNiagaraComponent* ACharacterBase::FindBodyAuraNiagaraComponent(const FName ComponentName) const
{
	return CharacterPresentationComponent ? CharacterPresentationComponent->FindBodyAuraNiagaraComponent(ComponentName)
										  : BodyAuraNiagaraComponent.Get();
}

void ACharacterBase::ApplyBodyAuraNiagaraWithOffset(
	const FName ComponentName,
	UNiagaraSystem* NiagaraSystem,
	const bool bActivate,
	const bool bResetSystem,
	const FVector RelativeLocationOffset,
	const FVector RelativeScale)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ApplyBodyAuraNiagaraWithOffset(
			ComponentName, NiagaraSystem, bActivate, bResetSystem, RelativeLocationOffset, RelativeScale);
	}
}

void ACharacterBase::ClearBodyAuraNiagaraIfMatching(const FName ComponentName, const UNiagaraSystem* ExpectedNiagaraSystem)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ClearBodyAuraNiagaraIfMatching(ComponentName, ExpectedNiagaraSystem);
	}
}

void ACharacterBase::HandleGameplayCue(
	AActor* Self,
	const FGameplayTag GameplayCueTag,
	const EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (GameplayCueTag.MatchesTagExact(LabGameplayTags::GameplayCue_Dash_Active) && CharacterPresentationComponent)
	{
		CharacterPresentationComponent->HandleDashGameplayCue(EventType, Parameters);
	}
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

int32 ACharacterBase::GetMatchTeamColorIndex() const
{
	const APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>();
	const UPlayerMatchComponent* MatchComponent = PdPlayerState ? PdPlayerState->GetPlayerMatchComponent() : nullptr;
	return MatchComponent ? MatchComponent->GetMatchTeamColorIndex() : INDEX_NONE;
}

bool ACharacterBase::IsSameTeam(const ACharacterBase* OtherCharacter) const
{
	if (!OtherCharacter)
	{
		return false;
	}
	const int32 MyTeamColorIndex = GetMatchTeamColorIndex();
	const int32 OtherTeamColorIndex = OtherCharacter->GetMatchTeamColorIndex();
	return MyTeamColorIndex != INDEX_NONE && OtherTeamColorIndex != INDEX_NONE && MyTeamColorIndex == OtherTeamColorIndex;
}

bool ACharacterBase::CanDamageCharacterByTeam(const ACharacterBase* OtherCharacter) const
{
	return OtherCharacter && OtherCharacter != this && !IsSameTeam(OtherCharacter);
}

bool ACharacterBase::IsStatusFrozen() const
{
	return CharacterAbilityRuntimeComponent && CharacterAbilityRuntimeComponent->IsFrozen();
}

void ACharacterBase::ReapplyCurrentRotationPolicy()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent || IsStatusFrozen())
	{
		return;
	}

	ApplyCurrentRotationPolicy(MovementComponent);
}

bool ACharacterBase::IsDeathHandled() const
{
	return CharacterDeathComponent && CharacterDeathComponent->IsDeathHandled();
}

void ACharacterBase::RestoreRotationSettingsAfterFrozen(UCharacterMovementComponent* MovementComponent)
{
	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->RestoreCachedRotationSettings(MovementComponent);
	}

	ReapplyCurrentRotationPolicy();
}

void ACharacterBase::ApplyCurrentRotationPolicy(
	UCharacterMovementComponent* MovementComponent)
{
	static_cast<void>(MovementComponent);
}

bool ACharacterBase::ShouldUseContinuousCharacterTick() const
{
	return false;
}

void ACharacterBase::ApplyMovementSpeedFromAttribute()
{
	if (CharacterAbilityRuntimeComponent)
	{
		CharacterAbilityRuntimeComponent->ApplyMovementSpeedFromAttribute();
	}
}

void ACharacterBase::RefreshCharacterTickEnabled()
{
	const bool bRuntimeNeedsTick = CharacterAbilityRuntimeComponent && CharacterAbilityRuntimeComponent->NeedsCharacterTick();
	const bool bDeathNeedsTick = CharacterDeathComponent && CharacterDeathComponent->NeedsCharacterTick();
	SetActorTickEnabled(!HasActorBegunPlay() || ShouldUseContinuousCharacterTick() || bRuntimeNeedsTick || bDeathNeedsTick);
}

TSubclassOf<UUserWidget> ACharacterBase::ResolveHealthBarWidgetClass(const UWidgetClassDefinition* WidgetDefinition) const
{
	return WidgetDefinition ? WidgetDefinition->GetHealthBarWidgetClass() : nullptr;
}

bool ACharacterBase::ShouldApplyResolvedHealthBarWidgetClass(UClass* CurrentWidgetClass, TSubclassOf<UUserWidget> ResolvedWidgetClass) const
{
	return !CurrentWidgetClass && ResolvedWidgetClass != nullptr;
}

void ACharacterBase::ApplyCameraCollisionIgnoreToCharacterComponents() const
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	if (HealthBarWidget)
	{
		HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ACharacterBase::ApplySkillDamageCollisionToCharacterComponents() const
{
	// Capsules are movement geometry, not damage geometry. Skill projectiles must
	// pass through them and stop only on the primary skeletal mesh.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(LabCollisionChannels::Projectile(), ECR_Ignore);
	}

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetCollisionObjectType(LabCollisionChannels::HitableBody());
		CharacterMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		CharacterMesh->SetCollisionResponseToChannel(LabCollisionChannels::Projectile(), ECR_Block);
		if (CharacterMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
	}
}

void ACharacterBase::HandleDeath_Implementation()
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ApplyDeathPhysics();
	}
}

void ACharacterBase::ResetDeathStateForRespawn()
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ResetDeathStateForRespawn();
	}
}

void ACharacterBase::ResetDeathStateForRespawnAtTransform(const FTransform& RespawnTransform)
{
	SetActorTransform(RespawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	ResetDeathStateForRespawn();

	if (HasAuthority())
	{
		MulticastResetDeathStateForRespawnAtTransform(RespawnTransform);
		ForceNetUpdate();
	}
}

void ACharacterBase::StartDeathDissolve(const float DurationSeconds)
{
	if (!CharacterDeathComponent)
	{
		return;
	}
	const float SafeDuration = CharacterDeathComponent->GetSafeDissolveDuration(DurationSeconds);
	if (HasAuthority())
	{
		MulticastStartDeathDissolve(SafeDuration);
		return;
	}
	CharacterDeathComponent->StartDeathDissolveLocal(SafeDuration);
}

void ACharacterBase::ClearCharacterOverlayMaterial()
{
	if (HasAuthority())
	{
		MulticastClearCharacterOverlayMaterial();
		return;
	}
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ClearCharacterOverlayMaterialLocal();
	}
}

void ACharacterBase::MulticastHandleDeath_Implementation()
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->HandleRemoteDeath();
	}
}

void ACharacterBase::MulticastStartDeathDissolve_Implementation(const float DurationSeconds)
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->StartDeathDissolveLocal(DurationSeconds);
	}
}

void ACharacterBase::MulticastResetDeathStateForRespawnAtTransform_Implementation(const FTransform& RespawnTransform)
{
	if (HasAuthority())
	{
		return;
	}
	SetActorTransform(RespawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	ResetDeathStateForRespawn();
}

void ACharacterBase::MulticastClearCharacterOverlayMaterial_Implementation()
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ClearCharacterOverlayMaterialLocal();
	}
}

void ACharacterBase::HandleDamageTaken(
	const float DamageAmount,
	const bool bCriticalHit,
	const bool bAllowHitReact,
	AActor* DamageInstigator,
	AActor* DamageCauser)
{
	static_cast<void>(bAllowHitReact);
	static_cast<void>(DamageInstigator);
	static_cast<void>(DamageCauser);
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->HandleDamageTaken(DamageAmount, bCriticalHit);
	}
}

FVector ACharacterBase::GetDamageIndicatorWorldLocation() const
{
	if (UDamageIndicatorComponent* DamageIndicator = GetDamageIndicatorComponent())
	{
		return DamageIndicator->ResolveDamageIndicatorWorldLocation();
	}

	const UCapsuleComponent* CharacterCapsule = GetCapsuleComponent();
	const float HeightOffset = CharacterCapsule ? CharacterCapsule->GetScaledCapsuleHalfHeight() + 40.0f : 120.0f;
	return GetActorLocation() + FVector(0.0f, 0.0f, HeightOffset);
}

void ACharacterBase::MulticastHandleDamageTaken_Implementation(
	const float DamageAmount,
	const bool bCriticalHit,
	const FVector_NetQuantize WorldLocation)
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->HandleRemoteDamageTaken(DamageAmount, bCriticalHit, WorldLocation);
	}
}

int32 ACharacterBase::GetFactionId() const
{
	return FactionId;
}
