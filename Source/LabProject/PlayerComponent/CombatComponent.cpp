#include "PlayerComponent/CombatComponent.h"

#include "AbilitySystem/Ability/AttackAbility.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Character/PdCharacterBase.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Common/ProjectTagConfig.h"
#include "GameplayEffect.h"
#include "Mode/PdPlayerController.h"
#include "Mode/PdHUD.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Weapon/WeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CombatComponent)

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
	Super::EndPlay(EndPlayReason);
}

void UCombatComponent::RefreshCachedReferences()
{
	CachedOwner = Cast<APdCharacterBase>(GetOwner());
	CachedASC = CachedOwner ? CachedOwner->GetPdAbilitySystemComponent() : nullptr;
}

void UCombatComponent::StartPrimaryAttack()
{
	if (IsPrimaryAttackBlockedByAbilityTags())
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = GetPlayerAbilitySystemComponent())
		{
			AbilitySystemComponent->LocalInputConfirm();
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

	AWeaponBase* WeaponActor = GetCurrentWeaponActor();
	if (!WeaponActor || !WeaponActor->SupportsAimInput())
	{
		return;
	}

	if (!WeaponActor->HandleAimStart(PlayerCharacter))
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
		return;
	}

	PlayerCharacter->SetWeaponAimActive(false, FWeaponAimCameraSettings());
}

void UCombatComponent::StopAutomaticFire()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutomaticFireTimerHandle);
	}
}

void UCombatComponent::ServerRequestAttackJumpSection_Implementation(FName RequestedSectionName)
{
	if (RequestedSectionName.IsNone())
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent = GetPlayerAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return;
	}

	const FGameplayTag ResolvedAttackAbilityTag = GetAttackAbilityTag();
	if (UAttackAbility* ActiveAttackAbility = ResolveActiveAttackAbility(AbilitySystemComponent, MakeAbilityTagContainer(ResolvedAttackAbilityTag)))
	{
		ActiveAttackAbility->RequestJumpToSection(RequestedSectionName);
	}
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
	const APdPlayer* PlayerCharacter = GetPlayerOwner();
	const UEquipmentComponent* EquipmentComponent = PlayerCharacter ? PlayerCharacter->GetEquipmentComponent() : nullptr;
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

FGameplayTag UCombatComponent::GetRangedAttackAbilityTag() const
{
	return UProjectTagConfig::Get(this)->GetCombatRangedAttackAbilityTag();
}

FGameplayTag UCombatComponent::GetWeaponDamageSourceTag() const
{
	return UProjectTagConfig::Get(this)->GetCombatWeaponDamageSourceTag();
}

FGameplayTag UCombatComponent::GetHitReactAbilityTag() const
{
	return UProjectTagConfig::Get(this)->GetCombatHitReactAbilityTag();
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

const FGameplayAbilitySpec* UCombatComponent::FindActiveAbilitySpec(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTagContainer& AbilityTags) const
{
	if (!AbilitySystemComponent || AbilityTags.IsEmpty())
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (!AbilitySpec.IsActive() || !AbilitySpec.Ability)
		{
			continue;
		}

		if (AbilitySpec.Ability->GetAssetTags().HasAny(AbilityTags))
		{
			return &AbilitySpec;
		}
	}

	return nullptr;
}

UAttackAbility* UCombatComponent::ResolveActiveAttackAbility(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTagContainer& AbilityTags) const
{
	const FGameplayAbilitySpec* ActiveAbilitySpec = FindActiveAbilitySpec(AbilitySystemComponent, AbilityTags);
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
		RequestNextAttackSection(ActiveAttackAbility);
		return;
	}

	TryActivateAttackAbility(AbilitySystemComponent, AttackTagContainer);
}

bool UCombatComponent::IsPrimaryAttackBlockedByAbilityTags() const
{
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
	return ShouldUseRangedAttackAbility(WeaponActor) ? GetRangedAttackAbilityTag() : GetAttackAbilityTag();
}

void UCombatComponent::RequestNextAttackSection(UAttackAbility* ActiveAttackAbility)
{
	if (!ActiveAttackAbility)
	{
		return;
	}

	const FName RequestedSectionName = ActiveAttackAbility->GetNextAttackSectionName();
	if (RequestedSectionName.IsNone() || !ActiveAttackAbility->RequestJumpToSection(RequestedSectionName))
	{
		return;
	}

	if (AActor* OwnerActor = GetOwner(); OwnerActor && !OwnerActor->HasAuthority())
	{
		ServerRequestAttackJumpSection(RequestedSectionName);
	}
}

void UCombatComponent::TryActivateAttackAbility(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTagContainer& AbilityTags) const
{
	if (AbilitySystemComponent && !AbilityTags.IsEmpty())
	{
		AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags, true);
	}
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

float UCombatComponent::GetWeaponDamageSourceMagnitude()
{
	if (!CachedASC)
	{
		return 0.f;
	}

	FGameplayAttribute DamageAttribute;
	if (!ResolveWeaponDamageAttribute(DamageAttribute))
	{
		return 0.f;
	}

	return CachedASC->GetNumericAttribute(DamageAttribute);
}

bool UCombatComponent::ApplyWeaponDamageToTarget(AActor* TargetActor)
{
	APdCharacterBase* SourceCharacter = CachedOwner.Get();
	APdCharacterBase* TargetCharacter = Cast<APdCharacterBase>(TargetActor);
	UPdAbilitySystemComponent* SourceASC = CachedASC.Get();
	UPdAbilitySystemComponent* TargetASC = TargetCharacter ? TargetCharacter->GetPdAbilitySystemComponent() : nullptr;

	if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter || !SourceASC || !TargetASC)
	{
		return false;
	}

	const float BaseDamageAmount = GetWeaponDamageSourceMagnitude();
	if (BaseDamageAmount <= 0.f)
	{
		return false;
	}

	UBasicAttributeSet* SourceAttributeSet = const_cast<UBasicAttributeSet*>(SourceASC->GetSet<UBasicAttributeSet>());
	if (!SourceAttributeSet)
	{
		return false;
	}

	SourceAttributeSet->ConsumeOutgoingDamage();
	if (!ApplyDamageEffect(SourceASC, SourceASC, OutgoingDamageEffectClass, BaseDamageAmount, TargetCharacter))
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
	if (!ApplyDamageEffect(SourceASC, TargetASC, IncomingDamageEffectClass, FinalOutgoingDamage, SourceCharacter))
	{
		TargetAttributeSet->SetPendingIncomingDamageCriticalHit(false);
		return false;
	}

	return true;
}

bool UCombatComponent::ResolveWeaponDamageAttribute(FGameplayAttribute& OutAttribute) const
{
	OutAttribute = FGameplayAttribute();

	const FGameplayTag WeaponDamageSourceTag = GetWeaponDamageSourceTag();
	if (!CachedASC || !WeaponDamageSourceTag.IsValid())
	{
		return false;
	}

	return CachedASC->ResolveAttributeFromTag(WeaponDamageSourceTag, OutAttribute);
}

bool UCombatComponent::ApplyDamageEffect(UPdAbilitySystemComponent* SourceASC, UPdAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> DamageEffectClass, float Magnitude, UObject* SourceObject) const
{
	FGameplayTag DamageMagnitudeSetByCallerTag;
	if (!SourceASC || !TargetASC || !DamageEffectClass || FMath::IsNearlyZero(Magnitude)
		|| !SourceASC->ResolveDamageMagnitudeSetByCallerTag(DamageMagnitudeSetByCallerTag))
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
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
