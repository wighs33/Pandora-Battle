#include "Component/Player/CombatComponent.h"

#include "AbilitySystem/Ability/AttackAbility.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Common/EquipmentAbilityData.h"
#include "Definition/Common/ProjectTagConfig.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameplayEffect.h"
#include "Definition/Item/ItemDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdHUD.h"
#include "Component/Player/EquipmentComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CombatComponent)

namespace
{
	constexpr float UnarmedTraceDebugDrawTime = 1.0f;
	const FColor UnarmedTraceDebugColor = FColor::Red;
	const FColor UnarmedTraceDebugHitColor = FColor::Green;
}

UCombatComponent::UCombatComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshCachedReferences();
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAutomaticFire();
	StopUnarmedAttackTrace();
	ReleaseUnarmedAttackMontagePreload();
	TemporaryWeaponDamageBonuses.Reset();
	Super::EndPlay(EndPlayReason);
}

void UCombatComponent::RefreshCachedReferences()
{
	CachedOwner = Cast<ACharacterBase>(GetOwner());
	CachedASC = CachedOwner ? CachedOwner->GetPdAbilitySystemComponent() : nullptr;
}

void UCombatComponent::ApplyDefinition(const UPlayerPawnDefinition* Definition)
{
	StopUnarmedAttackTrace();
	UnarmedCombatSettings = Definition
		? Definition->GetUnarmedCombatSettings()
		: FUnarmedCombatSettings();

	CachedUnarmedAttackObjectTypes = UnarmedCombatSettings.TraceObjectTypes;
	HitActorsInCurrentUnarmedAttack.Reset();
	TrackedUnarmedAttackSectionName = NAME_None;

	if (HasBegunPlay())
	{
		BeginUnarmedAttackMontagePreload();
	}
}

void UCombatComponent::PlayUnarmedComboWindowStartEffect() const
{
	ACharacterBase* Character = CachedOwner ? CachedOwner.Get() : Cast<ACharacterBase>(GetOwner());
	if (!UnarmedCombatSettings.ComboWindowStartEffect
		|| !Character
		|| Character->GetNetMode() == NM_DedicatedServer
		|| !Character->IsPlayerControlled()
		|| !Character->IsLocallyControlled())
	{
		return;
	}

	USceneComponent* EffectAttachComponent = Character->GetRootComponent();
	if (!EffectAttachComponent)
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAttached(
		UnarmedCombatSettings.ComboWindowStartEffect,
		EffectAttachComponent,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true,
		true,
		ENCPoolMethod::AutoRelease,
		true);
}

void UCombatComponent::StartPrimaryAttack()
{
	if (IsPrimaryAttackBlockedByAbilityTags())
	{
		if (UAbilitySystemComponent* BlockingAbilitySystemComponent = GetPlayerAbilitySystemComponent())
		{
			BlockingAbilitySystemComponent->LocalInputConfirm();
		}
		StopAutomaticFire();
		return;
	}

	ProcessAttackInput();
	TryStartAutomaticFire();
}

void UCombatComponent::StopPrimaryAttack()
{
	StopAutomaticFire();
}

void UCombatComponent::StartAim()
{
	APdPlayer* PlayerCharacter = GetPlayerOwner();
	if (!PlayerCharacter)
	{
		return;
	}
	if (PlayerCharacter->IsStatusFrozen())
	{
		StopAim();
		return;
	}

	AWeaponBase* WeaponActor = GetCurrentWeaponActor();
	if (!WeaponActor || !WeaponActor->SupportsAimInput())
	{
		return;
	}

	if (!WeaponActor->HandleAimStart(PlayerCharacter))
	{
		return;
	}
	if (!PlayerCharacter->IsWeaponAimActive())
	{
		return;
	}

	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->ShowAimCrosshair(WeaponActor->GetAimCrosshairWidgetTag());
	}
}

void UCombatComponent::StopAim()
{
	APdPlayer* PlayerCharacter = GetPlayerOwner();
	if (!PlayerCharacter)
	{
		return;
	}

	if (APdHUD* HUD = GetPdHUD())
	{
		HUD->HideAimCrosshair();
	}

	AWeaponBase* WeaponActor = GetCurrentWeaponActor();
	if (WeaponActor && WeaponActor->SupportsAimInput())
	{
		WeaponActor->HandleAimEnd(PlayerCharacter);
	}

	// A weapon subclass may perform extra teardown, but aim state itself must
	// never survive a completed StopAim command (or a missing Super call).
	if (PlayerCharacter->IsWeaponAimActive())
	{
		PlayerCharacter->SetWeaponAimActive(
			false,
			FWeaponAimCameraSettings());
	}
}

void UCombatComponent::StopAutomaticFire()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutomaticFireTimerHandle);
	}
}

void UCombatComponent::ServerRequestAttackJumpSection_Implementation(
	FName ClientExpectedSectionName,
	FGameplayTag AbilityTag)
{
	RefreshCachedReferences();

	if (!AbilityTag.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetPlayerAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag ServerSelectedAbilityTag = GetSelectedAttackAbilityTag(GetCurrentWeaponActor());
	if (!ServerSelectedAbilityTag.IsValid() || !AbilityTag.MatchesTagExact(ServerSelectedAbilityTag))
	{
		return;
	}

	if (UAttackAbility* ActiveAttackAbility = ResolveActiveAttackAbility(AbilitySystemComponent, MakeAbilityTagContainer(AbilityTag)))
	{
		const FName ServerExpectedSectionName = ActiveAttackAbility->GetNextAttackSectionName();
		if (ClientExpectedSectionName != ServerExpectedSectionName)
		{
			return;
		}

		ActiveAttackAbility->RequestNextComboInput();
		return;
	}

	// The client may press during the last montage recovery frames and the RPC
	// can arrive after the server has already ended that ability. Treat that
	// valid late request as the next primary attack instead of dropping it.
	TryActivateAttackAbility(
		AbilitySystemComponent,
		MakeAbilityTagContainer(ServerSelectedAbilityTag));
}

APdPlayer* UCombatComponent::GetPlayerOwner() const
{
	return Cast<APdPlayer>(GetOwner());
}

APdHUD* UCombatComponent::GetPdHUD() const
{
	const APdPlayer* PlayerCharacter = GetPlayerOwner();
	const APdPlayerController* Controller = PlayerCharacter ? Cast<APdPlayerController>(PlayerCharacter->GetController()) : nullptr;
	return Controller ? Cast<APdHUD>(Controller->GetHUD()) : nullptr;
}

AWeaponBase* UCombatComponent::GetCurrentWeaponActor() const
{
	const ACharacterBase* CharacterOwner = CachedOwner.Get();
	if (!CharacterOwner)
	{
		CharacterOwner = Cast<ACharacterBase>(GetOwner());
	}

	const UEquipmentComponent* EquipmentComponent = CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

UAbilitySystemComponent* UCombatComponent::GetPlayerAbilitySystemComponent() const
{
	APdPlayer* PlayerCharacter = GetPlayerOwner();
	return PlayerCharacter ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter) : nullptr;
}

FGameplayTag UCombatComponent::GetAttackAbilityTag() const
{
	return UProjectTagConfig::Get(this)->GetCombatAttackAbilityTag();
}

FGameplayTag UCombatComponent::GetPunchAbilityTag() const
{
	return UProjectTagConfig::Get(this)->GetCombatPunchAbilityTag();
}

FGameplayTag UCombatComponent::GetRangedAttackAbilityTag() const
{
	return UProjectTagConfig::Get(this)->GetCombatRangedAttackAbilityTag();
}

FGameplayTag UCombatComponent::GetWeaponDamageSourceTag() const
{
	return UProjectTagConfig::Get(this)->GetCombatWeaponDamageSourceTag();
}

FGameplayTagContainer UCombatComponent::MakeAbilityTagContainer(const FGameplayTag& AbilityTag) const
{
	FGameplayTagContainer AbilityTags;
	if (AbilityTag.IsValid())
	{
		AbilityTags.AddTag(AbilityTag);
	}

	return AbilityTags;
}

UAttackAbility* UCombatComponent::ResolveActiveAttackAbility(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTagContainer& AbilityTags) const
{
	const UPdAbilitySystemComponent* PdASC = Cast<UPdAbilitySystemComponent>(AbilitySystemComponent);
	const FGameplayAbilitySpec* ActiveAbilitySpec = PdASC ? PdASC->FindActiveAbilitySpecByTags(AbilityTags) : nullptr;
	return ActiveAbilitySpec ? Cast<UAttackAbility>(ActiveAbilitySpec->GetPrimaryInstance()) : nullptr;
}

void UCombatComponent::ProcessAttackInput()
{
	APdPlayer* PlayerCharacter = GetPlayerOwner();
	if (!PlayerCharacter)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetPlayerAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (IsPrimaryAttackBlockedByAbilityTags())
	{
		return;
	}

	AWeaponBase* CurrentWeaponActor = GetCurrentWeaponActor();
	if (!TryProcessWeaponPrimaryAttack(PlayerCharacter, CurrentWeaponActor))
	{
		return;
	}

	const FGameplayTag SelectedAttackAbilityTag = GetSelectedAttackAbilityTag(CurrentWeaponActor);
	if (!SelectedAttackAbilityTag.IsValid())
	{
		return;
	}

	const FGameplayTagContainer AttackTagContainer = MakeAbilityTagContainer(SelectedAttackAbilityTag);

	if (ShouldUseRangedAttackAbility(CurrentWeaponActor))
	{
		TryActivateAttackAbility(AbilitySystemComponent, AttackTagContainer);
		return;
	}

	if (UAttackAbility* ActiveAttackAbility = ResolveActiveAttackAbility(AbilitySystemComponent, AttackTagContainer))
	{
		RequestNextAttackSection(ActiveAttackAbility, SelectedAttackAbilityTag);
		return;
	}

	TryActivateAttackAbility(AbilitySystemComponent, AttackTagContainer);
}

bool UCombatComponent::IsPrimaryAttackBlockedByAbilityTags() const
{
	const ACharacterBase* CharacterOwner = IsValid(CachedOwner.Get())
		? CachedOwner.Get()
		: Cast<ACharacterBase>(GetOwner());
	if (CharacterOwner && CharacterOwner->IsStatusFrozen())
	{
		return true;
	}

	const UAbilitySystemComponent* AbilitySystemComponent = GetPlayerAbilitySystemComponent();
	return AbilitySystemComponent
		&& (AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::GameplayAbility_AOEAttack_Active)
			|| AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::GameplayAbility_ShootProjectile_Active));
}

bool UCombatComponent::TryProcessWeaponPrimaryAttack(APdPlayer* PlayerCharacter, AWeaponBase* WeaponActor) const
{
	if (!WeaponActor || !WeaponActor->SupportsAimInput())
	{

		return true;
	}

	return PlayerCharacter && WeaponActor->HandlePrimaryAttack(PlayerCharacter);
}

bool UCombatComponent::ShouldUseRangedAttackAbility(const AWeaponBase* WeaponActor) const
{
	return WeaponActor && WeaponActor->SupportsAimInput();
}

FGameplayTag UCombatComponent::GetSelectedAttackAbilityTag(const AWeaponBase* WeaponActor) const
{
	if (ShouldUseRangedAttackAbility(WeaponActor))
	{
		return GetRangedAttackAbilityTag();
	}

	return WeaponActor ? GetAttackAbilityTag() : GetPunchAbilityTag();
}

void UCombatComponent::RequestNextAttackSection(UAttackAbility* ActiveAttackAbility, const FGameplayTag& AbilityTag)
{
	if (!ActiveAttackAbility)
	{
		return;
	}

	const FName ClientExpectedSectionName = ActiveAttackAbility->GetNextAttackSectionName();
	const bool bAccepted = ActiveAttackAbility->RequestNextComboInput();
	if (!bAccepted)
	{
		return;
	}

	if (AActor* OwnerActor = GetOwner(); OwnerActor && !OwnerActor->HasAuthority())
	{
		ServerRequestAttackJumpSection(ClientExpectedSectionName, AbilityTag);
	}
}

bool UCombatComponent::TryActivateAttackAbility(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTagContainer& AbilityTags) const
{
	if (!AbilitySystemComponent)
	{
		return false;
	}

	if (AbilityTags.IsEmpty())
	{
		return false;
	}

	return AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags, true);
}

bool UCombatComponent::TryStartAutomaticFire()
{
	if (IsPrimaryAttackBlockedByAbilityTags())
	{
		StopAutomaticFire();
		return false;
	}

	AWeaponBase* WeaponActor = GetCurrentWeaponActor();
	if (!WeaponActor || !WeaponActor->SupportsAutomaticFire())
	{
		StopAutomaticFire();
		return false;
	}

	const float FireInterval = WeaponActor->GetAutomaticFireInterval();
	if (FireInterval <= 0.f)
	{
		StopAutomaticFire();
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (World->GetTimerManager().IsTimerActive(AutomaticFireTimerHandle))
	{
		return true;
	}

	World->GetTimerManager().SetTimer(
		AutomaticFireTimerHandle,
		this,
		&ThisClass::HandleAutomaticFireTick,
		FireInterval,
		true,
		FireInterval);
	return true;
}

void UCombatComponent::HandleAutomaticFireTick()
{
	if (IsPrimaryAttackBlockedByAbilityTags())
	{
		StopAutomaticFire();
		return;
	}

	AWeaponBase* WeaponActor = GetCurrentWeaponActor();
	if (!WeaponActor || !WeaponActor->SupportsAutomaticFire())
	{
		StopAutomaticFire();
		return;
	}

	ProcessAttackInput();
}

float UCombatComponent::GetActionStaminaCost() const
{
	const ACharacterBase* CharacterOwner = CachedOwner.Get();
	if (!CharacterOwner)
	{
		CharacterOwner = Cast<ACharacterBase>(GetOwner());
	}

	const UEquipmentComponent* EquipmentComponent = CharacterOwner
		? CharacterOwner->GetEquipmentComponent()
		: nullptr;
	if (const UItemDefinition* WeaponDefinition = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponDefinition()
		: nullptr)
	{
		return WeaponDefinition->GetSafeAttackStaminaCost();
	}

	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	if (!SettingDefinition)
	{
		SettingDefinition = GetDefault<UGameSettingDefinition>();
	}

	return SettingDefinition
		? FMath::Max(SettingDefinition->ActionStaminaCost, 0.0f)
		: 0.0f;
}

bool UCombatComponent::CanAffordRangedWeaponAttackStamina() const
{
	const ACharacterBase* CharacterOwner = CachedOwner.Get();
	if (!CharacterOwner)
	{
		CharacterOwner = Cast<ACharacterBase>(GetOwner());
	}
	if (!CharacterOwner)
	{
		return false;
	}

	if (!CharacterOwner->IsPlayerControlled())
	{
		return true;
	}

	const float StaminaCost = GetActionStaminaCost();
	if (StaminaCost <= 0.0f)
	{
		return true;
	}

	const UPdAbilitySystemComponent* AbilitySystemComponent =
		CharacterOwner->GetPdAbilitySystemComponent();
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;
	return AttributeSet
		&& AttributeSet->GetStamina() + UE_SMALL_NUMBER >= StaminaCost;
}

bool UCombatComponent::TryCommitRangedWeaponAttackStamina()
{
	if (!HasCombatAuthority())
	{
		return false;
	}

	ACharacterBase* CharacterOwner = CachedOwner.Get();
	if (!CharacterOwner)
	{
		CharacterOwner = Cast<ACharacterBase>(GetOwner());
	}
	if (!CharacterOwner)
	{
		return false;
	}

	if (!CharacterOwner->IsPlayerControlled())
	{
		return true;
	}

	const float StaminaCost = GetActionStaminaCost();
	if (StaminaCost <= 0.0f)
	{
		return true;
	}

	UPdAbilitySystemComponent* AbilitySystemComponent =
		CharacterOwner->GetPdAbilitySystemComponent();
	const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
		? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
		: nullptr;
	if (!AttributeSet || AttributeSet->GetStamina() + UE_SMALL_NUMBER < StaminaCost)
	{
		return false;
	}

	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const TSubclassOf<UGameplayEffect> CostEffectClass = SettingDefinition
		? SettingDefinition->AbilityCostGameplayEffectClass
		: nullptr;
	if (!CostEffectClass)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext =
		AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle CostSpecHandle =
		AbilitySystemComponent->MakeOutgoingSpec(
			CostEffectClass,
			1.0f,
			EffectContext);
	if (!CostSpecHandle.IsValid() || !CostSpecHandle.Data.IsValid())
	{
		return false;
	}

	CostSpecHandle.Data->SetSetByCallerMagnitude(
		LabGameplayTags::Data_ManaCost,
		0.0f);
	CostSpecHandle.Data->SetSetByCallerMagnitude(
		LabGameplayTags::Data_StaminaCost,
		-StaminaCost);
	return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*CostSpecHandle.Data.Get()).WasSuccessfullyApplied();
}

float UCombatComponent::GetWeaponDamageSourceMagnitude()
{
	const ACharacterBase* CharacterOwner = CachedOwner.Get();
	if (!CharacterOwner)
	{
		CharacterOwner = Cast<ACharacterBase>(GetOwner());
	}

	const UEquipmentComponent* EquipmentComponent = CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
	const FGameplayTag WeaponDamageSourceTag = GetWeaponDamageSourceTag();
	return EquipmentComponent && WeaponDamageSourceTag.IsValid()
		? FMath::Max(
			0.0f,
			EquipmentComponent->GetCurrentWeaponStatMagnitude(
				WeaponDamageSourceTag))
		: 0.0f;
}

float UCombatComponent::CalculateStrengthAdjustedWeaponDamage(const float WeaponDamage, const float SourceStrength) const
{
	constexpr float StrengthPercentScale = 0.01f;
	const float ClampedWeaponDamage = FMath::Max(WeaponDamage, 0.0f);
	const float ClampedStrength = FMath::Max(SourceStrength, 0.0f);
	const double StrengthMultiplier = 1.0 + (static_cast<double>(ClampedStrength) * StrengthPercentScale);
	return FMath::Max(static_cast<float>(static_cast<double>(ClampedWeaponDamage) * StrengthMultiplier), 0.0f);
}

float UCombatComponent::GetStrengthAdjustedWeaponDamageMagnitude(const float SourceStrength)
{
	RefreshCachedReferences();

	const bool bHasEquippedWeapon = GetCurrentWeaponActor() != nullptr;
	const float RawWeaponDamageAmount = bHasEquippedWeapon
		? GetWeaponDamageSourceMagnitude()
		: GetUnarmedDamageSourceMagnitude();
	const float SkillWeaponDamageBonus = bHasEquippedWeapon ? GetTemporaryWeaponDamageBonus() : 0.0f;
	const float WeaponDamageWithBonus = FMath::Max(RawWeaponDamageAmount, 0.0f) + SkillWeaponDamageBonus;
	return CalculateStrengthAdjustedWeaponDamage(WeaponDamageWithBonus, SourceStrength);
}

void UCombatComponent::SetTemporaryWeaponDamageBonus(UObject* SourceObject, const float DamageBonus)
{
	if (!HasCombatAuthority())
	{
		return;
	}

	CompactTemporaryWeaponDamageBonuses();

	if (!SourceObject)
	{
		return;
	}

	const FObjectKey SourceKey(SourceObject);
	const float ClampedDamageBonus = FMath::Max(DamageBonus, 0.0f);
	if (ClampedDamageBonus <= 0.0f)
	{
		TemporaryWeaponDamageBonuses.Remove(SourceKey);
		return;
	}

	TemporaryWeaponDamageBonuses.Add(SourceKey, ClampedDamageBonus);

}

void UCombatComponent::ClearTemporaryWeaponDamageBonus(UObject* SourceObject)
{
	if (!HasCombatAuthority())
	{
		return;
	}

	CompactTemporaryWeaponDamageBonuses();

	if (!SourceObject)
	{
		return;
	}

	TemporaryWeaponDamageBonuses.Remove(FObjectKey(SourceObject));
}

float UCombatComponent::GetTemporaryWeaponDamageBonus() const
{
	float TotalBonus = 0.0f;
	for (const TPair<FObjectKey, float>& Entry : TemporaryWeaponDamageBonuses)
	{
		if (!IsValid(Entry.Key.ResolveObjectPtr()))
		{
			continue;
		}

		TotalBonus += FMath::Max(Entry.Value, 0.0f);
	}

	return TotalBonus;
}

void UCombatComponent::SetActiveComboDamageMultiplier(const float DamageMultiplier)
{
	if (!HasCombatAuthority())
	{
		return;
	}

	ActiveComboDamageMultiplier = FMath::Max(DamageMultiplier, 1.0f);
}

bool UCombatComponent::ApplyWeaponDamageToTarget(AActor* TargetActor)
{
	RefreshCachedReferences();

	if (!HasCombatAuthority())
	{
		return false;
	}

	ACharacterBase* SourceCharacter = CachedOwner.Get();
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor);
	UPdAbilitySystemComponent* SourceASC = CachedASC.Get();
	UPdAbilitySystemComponent* TargetASC = TargetCharacter ? TargetCharacter->GetPdAbilitySystemComponent() : nullptr;

	if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter || !SourceASC || !TargetASC)
	{
		return false;
	}

	if (!SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{
		return false;
	}

	AWeaponBase* CurrentWeaponActor = GetCurrentWeaponActor();
	const bool bShouldTriggerHitReactOnDamage = !CurrentWeaponActor || CurrentWeaponActor->ShouldTriggerHitReactOnDamage();
	UBasicAttributeSet* SourceAttributeSet = const_cast<UBasicAttributeSet*>(SourceASC->GetSet<UBasicAttributeSet>());
	const float SourceStrength = SourceAttributeSet ? FMath::Max(SourceAttributeSet->GetStrength(), 0.0f) : 0.0f;
	const float RawWeaponDamageAmount = GetWeaponDamageSourceMagnitude();
	const float SkillWeaponDamageBonus = GetTemporaryWeaponDamageBonus();
	const float WeaponDamageWithBonus = FMath::Max(RawWeaponDamageAmount, 0.0f) + SkillWeaponDamageBonus;
	const float BaseDamageAmount =
		CalculateStrengthAdjustedWeaponDamage(WeaponDamageWithBonus, SourceStrength)
		* ActiveComboDamageMultiplier;
	if (BaseDamageAmount <= 0.f)
	{
		return false;
	}

	if (!SourceAttributeSet)
	{
		return false;
	}

	SourceAttributeSet->ConsumeOutgoingDamage();

	if (!ApplyDamageEffect(
		SourceASC,
		SourceASC,
		UnarmedCombatSettings.OutgoingDamageEffectClass,
		BaseDamageAmount,
		TargetCharacter))
	{
		return false;
	}

	const float FinalOutgoingDamage = SourceAttributeSet->ConsumeOutgoingDamage();
	if (FinalOutgoingDamage <= 0.f)
	{
		return false;
	}
	const bool bCriticalHit = SourceAttributeSet->ConsumeOutgoingDamageCriticalHit();

	UBasicAttributeSet* TargetAttributeSet = const_cast<UBasicAttributeSet*>(TargetASC->GetSet<UBasicAttributeSet>());
	if (!TargetAttributeSet)
	{
		return false;
	}

	AActor* DamageCauser = CurrentWeaponActor ? Cast<AActor>(CurrentWeaponActor) : Cast<AActor>(SourceCharacter);
	UObject* DamageSourceObject = CurrentWeaponActor ? static_cast<UObject*>(CurrentWeaponActor) : static_cast<UObject*>(SourceCharacter);

	TargetAttributeSet->SetPendingIncomingDamageCriticalHit(bCriticalHit);
	TargetAttributeSet->SetPendingIncomingDamageAllowHitReact(bShouldTriggerHitReactOnDamage);
	if (!ApplyDamageEffect(
		SourceASC,
		TargetASC,
		UnarmedCombatSettings.IncomingDamageEffectClass,
		FinalOutgoingDamage,
		DamageSourceObject,
		SourceCharacter,
		DamageCauser))
	{
		TargetAttributeSet->SetPendingIncomingDamageCriticalHit(false);
		TargetAttributeSet->SetPendingIncomingDamageAllowHitReact(true);
		return false;
	}

	return true;
}

UAnimMontage* UCombatComponent::GetCachedUnarmedAttackMontage() const
{
	return CachedUnarmedAttackMontage
		? CachedUnarmedAttackMontage.Get()
		: UnarmedCombatSettings.AttackMontage.Get();
}

void UCombatComponent::BeginUnarmedAttackMontagePreload()
{
	ReleaseUnarmedAttackMontagePreload();
	CachedUnarmedAttackMontage = UnarmedCombatSettings.AttackMontage.Get();
	if (CachedUnarmedAttackMontage || UnarmedCombatSettings.AttackMontage.IsNull())
	{
		return;
	}

	UnarmedAttackMontagePreloadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(
			UnarmedCombatSettings.AttackMontage.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(
				this,
				&ThisClass::HandleUnarmedAttackMontagePreloadComplete));
}

void UCombatComponent::HandleUnarmedAttackMontagePreloadComplete()
{
	CachedUnarmedAttackMontage = UnarmedCombatSettings.AttackMontage.Get();
}

void UCombatComponent::ReleaseUnarmedAttackMontagePreload()
{
	if (UnarmedAttackMontagePreloadHandle.IsValid())
	{
		UnarmedAttackMontagePreloadHandle->CancelHandle();
		UnarmedAttackMontagePreloadHandle->ReleaseHandle();
		UnarmedAttackMontagePreloadHandle.Reset();
	}
	CachedUnarmedAttackMontage = nullptr;
}

bool UCombatComponent::GetUnarmedAttackData(FAttackData& OutAttackData) const
{
	OutAttackData = FAttackData();

	UAnimMontage* AttackMontage = GetCachedUnarmedAttackMontage();
	if (!AttackMontage)
	{

		return false;
	}

	OutAttackData.AttackMontage = AttackMontage;

	return true;
}

void UCombatComponent::SetUnarmedAttackTraceEnabledForSection(
	const bool bEnabled,
	const FName AttackSectionName)
{
	if (!bEnabled)
	{
		StopUnarmedAttackTrace();
		return;
	}

	if (AttackSectionName.IsNone())
	{
		return;
	}

	const bool bEnteredNewAttackSection = TrackedUnarmedAttackSectionName != AttackSectionName;
	if (bEnteredNewAttackSection)
	{
		TrackedUnarmedAttackSectionName = AttackSectionName;
		HitActorsInCurrentUnarmedAttack.Reset();
		PreviousUnarmedAttackTraceValid.Init(0, UnarmedCombatSettings.AttackTraces.Num());
	}

	StartUnarmedAttackTrace(false);
}

void UCombatComponent::ResetUnarmedAttackHitTracking()
{
	TrackedUnarmedAttackSectionName = NAME_None;
	HitActorsInCurrentUnarmedAttack.Reset();
}

void UCombatComponent::StartUnarmedAttackTrace(const bool bResetHitActors)
{
	if (!HasCombatAuthority())
	{

		return;
	}

	if (UnarmedAttackTraceTimerHandle.IsValid())
	{

		return;
	}

	if (bResetHitActors)
	{
		HitActorsInCurrentUnarmedAttack.Reset();
	}
	if (UnarmedCombatSettings.AttackTraces.IsEmpty()
		|| UnarmedCombatSettings.TraceObjectTypes.IsEmpty()
		|| !FMath::IsFinite(UnarmedCombatSettings.TraceInterval)
		|| UnarmedCombatSettings.TraceInterval <= 0.0f
		|| !FMath::IsFinite(UnarmedCombatSettings.TraceInterpolationDistance)
		|| UnarmedCombatSettings.TraceInterpolationDistance <= 0.0f)
	{
		return;
	}

	PreviousUnarmedAttackTraceStartLocations.SetNumZeroed(UnarmedCombatSettings.AttackTraces.Num());
	PreviousUnarmedAttackTraceEndLocations.SetNumZeroed(UnarmedCombatSettings.AttackTraces.Num());
	PreviousUnarmedAttackTraceValid.Init(0, UnarmedCombatSettings.AttackTraces.Num());
	PerformUnarmedAttackTrace();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			UnarmedAttackTraceTimerHandle,
			this,
			&ThisClass::PerformUnarmedAttackTrace,
			UnarmedCombatSettings.TraceInterval,
			true);
	}
}

void UCombatComponent::StopUnarmedAttackTrace()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UnarmedAttackTraceTimerHandle);
	}

	UnarmedAttackTraceTimerHandle.Invalidate();
	PreviousUnarmedAttackTraceStartLocations.Reset();
	PreviousUnarmedAttackTraceEndLocations.Reset();
	PreviousUnarmedAttackTraceValid.Reset();
}

void UCombatComponent::PerformUnarmedAttackTrace()
{
	AActor* OwnerActor = GetOwner();
	ACharacterBase* SourceCharacter = CachedOwner.Get();
	USkeletalMeshComponent* SourceMesh = SourceCharacter ? SourceCharacter->GetMesh() : nullptr;
	UWorld* World = GetWorld();
	if (!OwnerActor || !HasCombatAuthority() || !SourceCharacter || !SourceMesh || !World)
	{
		return;
	}

	if (UnarmedCombatSettings.AttackTraces.IsEmpty()
		|| UnarmedCombatSettings.TraceObjectTypes.IsEmpty())
	{

		return;
	}

	UnarmedAttackActorsToIgnore.Reset(2);
	UnarmedAttackActorsToIgnore.Add(SourceCharacter);
	UnarmedAttackActorsToIgnore.Add(OwnerActor);
	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const bool bDrawAttackDebug = SettingDefinition
		&& SettingDefinition->bDrawAttackDebugVisualization;

	if (PreviousUnarmedAttackTraceStartLocations.Num() != UnarmedCombatSettings.AttackTraces.Num()
		|| PreviousUnarmedAttackTraceEndLocations.Num() != UnarmedCombatSettings.AttackTraces.Num()
		|| PreviousUnarmedAttackTraceValid.Num() != UnarmedCombatSettings.AttackTraces.Num())
	{
		PreviousUnarmedAttackTraceStartLocations.SetNumZeroed(UnarmedCombatSettings.AttackTraces.Num());
		PreviousUnarmedAttackTraceEndLocations.SetNumZeroed(UnarmedCombatSettings.AttackTraces.Num());
		PreviousUnarmedAttackTraceValid.Init(0, UnarmedCombatSettings.AttackTraces.Num());
	}

	for (int32 TraceIndex = 0; TraceIndex < UnarmedCombatSettings.AttackTraces.Num(); ++TraceIndex)
	{
		const FUnarmedAttackTraceDefinition& TraceDefinition = UnarmedCombatSettings.AttackTraces[TraceIndex];
		if (TraceDefinition.StartSocketName.IsNone() || !SourceMesh->DoesSocketExist(TraceDefinition.StartSocketName))
		{

			continue;
		}

		const FName EndSocketName = TraceDefinition.EndSocketName.IsNone()
			? TraceDefinition.StartSocketName
			: TraceDefinition.EndSocketName;
		if (!SourceMesh->DoesSocketExist(EndSocketName))
		{

			continue;
		}

		const FVector TraceStart = SourceMesh->GetSocketLocation(TraceDefinition.StartSocketName);
		FVector TraceEnd = SourceMesh->GetSocketLocation(EndSocketName);
		if (TraceStart.Equals(TraceEnd, KINDA_SMALL_NUMBER))
		{
			TraceEnd = TraceStart + SourceCharacter->GetActorForwardVector();
		}

		UnarmedAttackHitResults.Reset();
		const FVector TraceHalfSize = TraceDefinition.HalfSize;
		if (TraceHalfSize.ContainsNaN()
			|| TraceHalfSize.X <= 0.0f
			|| TraceHalfSize.Y <= 0.0f
			|| TraceHalfSize.Z <= 0.0f)
		{
			continue;
		}
		const FRotator TraceRotation = SourceCharacter->GetActorRotation();
		const bool bHasPreviousTrace = PreviousUnarmedAttackTraceValid[TraceIndex] != 0;
		const FVector PreviousTraceStart = bHasPreviousTrace
			? PreviousUnarmedAttackTraceStartLocations[TraceIndex]
			: TraceStart;
		const FVector PreviousTraceEnd = bHasPreviousTrace
			? PreviousUnarmedAttackTraceEndLocations[TraceIndex]
			: TraceEnd;
		const float MaxTravelDistance = FMath::Max(
			FVector::Distance(PreviousTraceStart, TraceStart),
			FVector::Distance(PreviousTraceEnd, TraceEnd));
		const float InterpolationDistance = UnarmedCombatSettings.TraceInterpolationDistance;
		const int32 InterpolationCount = FMath::Max(
			1,
			FMath::CeilToInt(MaxTravelDistance / InterpolationDistance));

		for (int32 InterpolationIndex = 1; InterpolationIndex <= InterpolationCount; ++InterpolationIndex)
		{
			const float Alpha =
				static_cast<float>(InterpolationIndex) / static_cast<float>(InterpolationCount);
			const FVector InterpolatedTraceStart = FMath::Lerp(PreviousTraceStart, TraceStart, Alpha);
			const FVector InterpolatedTraceEnd = FMath::Lerp(PreviousTraceEnd, TraceEnd, Alpha);
			TArray<FHitResult> InterpolatedHitResults;
			UKismetSystemLibrary::BoxTraceMultiForObjects(
				this,
				InterpolatedTraceStart,
				InterpolatedTraceEnd,
				TraceHalfSize,
				TraceRotation,
				CachedUnarmedAttackObjectTypes,
				false,
				UnarmedAttackActorsToIgnore,
				EDrawDebugTrace::None,
				InterpolatedHitResults,
				true,
				FLinearColor::Red,
				FLinearColor::Green,
				UnarmedTraceDebugDrawTime);
			UnarmedAttackHitResults.Append(InterpolatedHitResults);
		}

		PreviousUnarmedAttackTraceStartLocations[TraceIndex] = TraceStart;
		PreviousUnarmedAttackTraceEndLocations[TraceIndex] = TraceEnd;
		PreviousUnarmedAttackTraceValid[TraceIndex] = 1;

		if (bDrawAttackDebug)
		{
			const bool bAnyHit = UnarmedAttackHitResults.ContainsByPredicate(
				[this](const FHitResult& Hit)
				{
					return Hit.GetActor() && !HitActorsInCurrentUnarmedAttack.Contains(Hit.GetActor());
				});
			const FColor DrawColor = bAnyHit
				? UnarmedTraceDebugHitColor
				: UnarmedTraceDebugColor;
			DrawDebugBox(World, TraceStart, TraceHalfSize, TraceRotation.Quaternion(), DrawColor, false, UnarmedTraceDebugDrawTime, 0, 1.5f);
			DrawDebugLine(World, TraceStart, TraceEnd, DrawColor, false, UnarmedTraceDebugDrawTime, 0, 2.0f);
		}

		for (const FHitResult& HitResult : UnarmedAttackHitResults)
		{
			AActor* HitActor = HitResult.GetActor();
			if (!HitActor || HitActorsInCurrentUnarmedAttack.Contains(HitActor))
			{
				continue;
			}

			ACharacterBase* HitCharacter = Cast<ACharacterBase>(HitActor);
			if (!HitCharacter || HitCharacter == SourceCharacter)
			{
				continue;
			}

			if (!SourceCharacter->CanDamageCharacterByTeam(HitCharacter))
			{

				continue;
			}

			HitActorsInCurrentUnarmedAttack.Add(HitActor);
			ApplyUnarmedDamageToTarget(HitActor);
		}
	}
}

bool UCombatComponent::ApplyUnarmedDamageToTarget(AActor* TargetActor)
{
	RefreshCachedReferences();

	if (!HasCombatAuthority())
	{
		return false;
	}

	ACharacterBase* SourceCharacter = CachedOwner.Get();
	ACharacterBase* TargetCharacter = Cast<ACharacterBase>(TargetActor);
	UPdAbilitySystemComponent* SourceASC = CachedASC.Get();
	UPdAbilitySystemComponent* TargetASC = TargetCharacter ? TargetCharacter->GetPdAbilitySystemComponent() : nullptr;

	if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter || !SourceASC || !TargetASC)
	{

		return false;
	}

	if (!SourceCharacter->CanDamageCharacterByTeam(TargetCharacter))
	{

		return false;
	}

	UBasicAttributeSet* SourceAttributeSet = const_cast<UBasicAttributeSet*>(SourceASC->GetSet<UBasicAttributeSet>());
	if (!SourceAttributeSet)
	{

		return false;
	}

	const float RawUnarmedDamageAmount = GetUnarmedDamageSourceMagnitude();
	const float SourceStrength = FMath::Max(SourceAttributeSet->GetStrength(), 0.0f);
	const float BaseDamageAmount =
		CalculateStrengthAdjustedWeaponDamage(RawUnarmedDamageAmount, SourceStrength)
		* ActiveComboDamageMultiplier;
	if (BaseDamageAmount <= 0.f)
	{

		return false;
	}

	SourceAttributeSet->ConsumeOutgoingDamage();
	if (!ApplyDamageEffect(
		SourceASC,
		SourceASC,
		UnarmedCombatSettings.OutgoingDamageEffectClass,
		BaseDamageAmount,
		TargetCharacter))
	{

		return false;
	}

	const float FinalOutgoingDamage = SourceAttributeSet->ConsumeOutgoingDamage();
	if (FinalOutgoingDamage <= 0.f)
	{

		return false;
	}

	const bool bCriticalHit = SourceAttributeSet->ConsumeOutgoingDamageCriticalHit();

	UBasicAttributeSet* TargetAttributeSet = const_cast<UBasicAttributeSet*>(TargetASC->GetSet<UBasicAttributeSet>());
	if (!TargetAttributeSet)
	{

		return false;
	}

	TargetAttributeSet->SetPendingIncomingDamageCriticalHit(bCriticalHit);
	TargetAttributeSet->SetPendingIncomingDamageAllowHitReact(true);
	if (!ApplyDamageEffect(
		SourceASC,
		TargetASC,
		UnarmedCombatSettings.IncomingDamageEffectClass,
		FinalOutgoingDamage,
		SourceCharacter,
		SourceCharacter,
		SourceCharacter))
	{
		TargetAttributeSet->SetPendingIncomingDamageCriticalHit(false);
		TargetAttributeSet->SetPendingIncomingDamageAllowHitReact(true);

		return false;
	}

	return true;
}

float UCombatComponent::GetUnarmedDamageSourceMagnitude() const
{
	if (UnarmedCombatSettings.DamageMagnitude > 0.0f)
	{
		return UnarmedCombatSettings.DamageMagnitude;
	}

	return GetCurrentWeaponActor()
		? const_cast<UCombatComponent*>(this)->GetWeaponDamageSourceMagnitude()
		: 0.0f;
}

bool UCombatComponent::HasCombatAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UCombatComponent::CompactTemporaryWeaponDamageBonuses()
{
	for (auto BonusIt = TemporaryWeaponDamageBonuses.CreateIterator(); BonusIt; ++BonusIt)
	{
		if (!IsValid(BonusIt.Key().ResolveObjectPtr()) || BonusIt.Value() <= 0.0f)
		{
			BonusIt.RemoveCurrent();
		}
	}
}

bool UCombatComponent::ApplyDamageEffect(UPdAbilitySystemComponent* SourceASC, UPdAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> DamageEffectClass, float Magnitude, UObject* SourceObject,
	AActor* InstigatorActor, AActor* EffectCauserActor) const
{
	FGameplayTag DamageMagnitudeSetByCallerTag;
	const bool bHasDamageMagnitudeTag =
		SourceASC && SourceASC->ResolveDamageMagnitudeSetByCallerTag(DamageMagnitudeSetByCallerTag);
	if (!HasCombatAuthority() || !SourceASC || !TargetASC || !DamageEffectClass || Magnitude <= 0.0f
		|| !bHasDamageMagnitudeTag)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	if (InstigatorActor || EffectCauserActor)
	{
		EffectContext.AddInstigator(InstigatorActor, EffectCauserActor ? EffectCauserActor : InstigatorActor);
	}
	EffectContext.AddSourceObject(SourceObject ? SourceObject : this);

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(DamageMagnitudeSetByCallerTag, Magnitude);
	const FActiveGameplayEffectHandle AppliedEffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return AppliedEffectHandle.WasSuccessfullyApplied();
}
