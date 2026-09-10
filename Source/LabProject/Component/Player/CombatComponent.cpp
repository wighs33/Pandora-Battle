#include "Component/Player/CombatComponent.h"

#include "AbilitySystem/Ability/AttackAbility.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/AbilitySystem/Ability/AbilityCostAndCooldownManager.h"
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
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
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
	bEndingPlay = false;
	Super::BeginPlay();

	RefreshCachedReferences();
	// Pawn 정의가 BeginPlay 이전에 적용된 경우에도 몽타주 로딩을 시작한다.
	BeginUnarmedAttackMontagePreload();
}

// 캐릭터 종료 시 입력 반복·타격 판정·로딩·ASC 구독을 함께 정리한다.
void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopAutomaticFire();
	StopUnarmedAttackTrace();
	ReleaseUnarmedAttackMontagePreload();
	if (CachedASC && AttackSpeedChangedDelegateHandle.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAttackSpeedAttribute()).Remove(AttackSpeedChangedDelegateHandle);
	}
	AttackSpeedChangedDelegateHandle.Reset();
	TemporaryWeaponDamageBonuses.Reset();
	Super::EndPlay(EndPlayReason);
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UCombatComponent, ReplicatedTemporaryWeaponDamageBonus, Params);
}

// 빙의·PlayerState 도착에 맞춰 ASC를 갱신하고 공격속도 변화가 연사 예약에 반영되게 한다.
void UCombatComponent::RefreshCachedReferences()
{
	if (bEndingPlay)
	{
		return;
	}
	CachedOwner = Cast<ACharacterBase>(GetOwner());
	UPdAbilitySystemComponent* NewASC = CachedOwner ? CachedOwner->GetPdAbilitySystemComponent() : nullptr;
	if (CachedASC == NewASC)
	{
		return;
	}
	if (CachedASC && AttackSpeedChangedDelegateHandle.IsValid())
	{
		CachedASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAttackSpeedAttribute()).Remove(AttackSpeedChangedDelegateHandle);
	}
	AttackSpeedChangedDelegateHandle.Reset();
	CachedASC = NewASC;
	if (CachedASC)
	{
		AttackSpeedChangedDelegateHandle = CachedASC->GetGameplayAttributeValueChangeDelegate(
			UBasicAttributeSet::GetAttackSpeedAttribute()).AddUObject(this, &ThisClass::HandleAttackSpeedChanged);
	}
}

// 플레이어·AI가 선택한 피해 설정과 맨손 연출 설정을 적용한다.
void UCombatComponent::ApplySettings(const FCombatDamageSettings& DamageSettings, const FUnarmedCombatSettings& UnarmedSettings)
{
	StopUnarmedAttackTrace();
	CombatDamageSettings = DamageSettings;
	UnarmedCombatSettings = UnarmedSettings;
	CachedUnarmedAttackObjectTypes = UnarmedCombatSettings.TraceObjectTypes;
	ResetUnarmedAttackHitTracking();
	if (HasBegunPlay() && !bEndingPlay)
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

// 누르기 입력은 즉시 한 번 처리하고 자동 무기라면 다음 입력을 예약한다.
void UCombatComponent::StartPrimaryAttack()
{
	RefreshCachedReferences();
	if (bEndingPlay)
	{
		return;
	}
	if (IsPrimaryAttackBlockedByAbilityTags())
	{
		if (UAbilitySystemComponent* BlockingAbilitySystemComponent = GetPlayerAbilitySystemComponent())
		{
			BlockingAbilitySystemComponent->LocalInputConfirm();
		}
		StopAutomaticFire();
		return;
	}

	bPrimaryAttackHeld = true;
	LastPrimaryAttackRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
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

	// 무기별 해제 처리가 누락돼도 조준 상태 자체는 반드시 종료한다.
	if (PlayerCharacter->IsWeaponAimActive())
	{
		PlayerCharacter->SetWeaponAimActive(
			false,
			FWeaponAimCameraSettings());
	}
}

void UCombatComponent::StopAutomaticFire()
{
	bPrimaryAttackHeld = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutomaticFireTimerHandle);
	}
}

// 태그뿐 아니라 GAS의 능력 핸들과 실행 키까지 확인해 이전 공격의 입력이 새 공격으로 섞이지 않게 한다.
void UCombatComponent::ServerRequestNextComboInput_Implementation(
	FGameplayAbilitySpecHandle AbilityHandle, FPredictionKey ActivationKey, FName ClientExpectedSectionName)
{
	RefreshCachedReferences();
	if (bEndingPlay || !HasCombatAuthority() || IsPrimaryAttackBlockedByAbilityTags() || !ActivationKey.IsValidKey())
	{
		return;
	}
	UAbilitySystemComponent* ASC = GetPlayerAbilitySystemComponent();
	FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(AbilityHandle) : nullptr;
	UAttackAbility* Attack = Spec ? Cast<UAttackAbility>(Spec->GetPrimaryInstance()) : nullptr;
	const FGameplayTag SelectedTag = GetSelectedAttackAbilityTag(GetCurrentWeaponActor());
	if (!Attack || !SelectedTag.IsValid() || !Attack->GetAssetTags().HasTagExact(SelectedTag)
		|| Attack->GetCurrentActivationInfo().GetActivationPredictionKey() != ActivationKey)
	{
		return;
	}
	if (Spec->IsActive())
	{
		if (ClientExpectedSectionName == Attack->GetNextAttackSectionName())
		{
			Attack->RequestNextComboInput();
		}
		return;
	}
	// 같은 실행이 자연 종료된 직후의 입력만 한 번 보정한다. 취소·오래된 입력은 재시작하지 않는다.
	if (Attack->TryConsumeLateComboInput())
	{
		ASC->TryActivateAbility(AbilityHandle);
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

	const FGameplayTagContainer AttackTagContainer = FGameplayTagContainer(SelectedAttackAbilityTag);

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
	const ACharacterBase* CharacterOwner = IsValid(CachedOwner.Get())
		? CachedOwner.Get()
		: Cast<ACharacterBase>(GetOwner());
	if (CharacterOwner && CharacterOwner->IsStatusFrozen())
	{
		return true;
	}

	const UAbilitySystemComponent* AbilitySystemComponent = GetPlayerAbilitySystemComponent();
	return AbilitySystemComponent
		&& (AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Dead)
			|| AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::Status_Frostbite)
			|| AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::GameplayAbility_AOEAttack_Active)
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

void UCombatComponent::RequestNextAttackSection(UAttackAbility* ActiveAttackAbility)
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
		ServerRequestNextComboInput(ActiveAttackAbility->GetCurrentAbilitySpecHandle(),
			ActiveAttackAbility->GetCurrentActivationInfo().GetActivationPredictionKey(), ClientExpectedSectionName);
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

// 반복 주기를 고정하지 않고 현재 공격속도와 마지막 요청 시점으로 다음 한 발을 예약한다.
bool UCombatComponent::TryStartAutomaticFire()
{
	if (bEndingPlay || !bPrimaryAttackHeld || IsPrimaryAttackBlockedByAbilityTags())
	{
		StopAutomaticFire();
		return false;
	}
	AWeaponBase* Weapon = GetCurrentWeaponActor();
	UWorld* World = GetWorld();
	const float Interval = Weapon ? Weapon->GetAutomaticFireInterval() : 0.0f;
	if (!World || !Weapon || !Weapon->SupportsAutomaticFire() || !FMath::IsFinite(Interval) || Interval <= 0.0f)
	{
		StopAutomaticFire();
		return false;
	}
	const double Elapsed = World->GetTimeSeconds() - LastPrimaryAttackRequestTime;
	const float Delay = FMath::Max(static_cast<float>(Interval - Elapsed), UE_SMALL_NUMBER);
	World->GetTimerManager().SetTimer(AutomaticFireTimerHandle, this, &ThisClass::HandleAutomaticFireTick, Delay, false);
	return true;
}

void UCombatComponent::HandleAutomaticFireTick()
{
	AWeaponBase* Weapon = GetCurrentWeaponActor();
	if (!bPrimaryAttackHeld || bEndingPlay || IsPrimaryAttackBlockedByAbilityTags() || !Weapon || !Weapon->SupportsAutomaticFire())
	{
		StopAutomaticFire();
		return;
	}
	LastPrimaryAttackRequestTime = GetWorld()->GetTimeSeconds();
	ProcessAttackInput();
	TryStartAutomaticFire();
}

// 누르는 도중 버프가 바뀌어도 다음 발사 요청의 남은 시간을 다시 계산한다.
void UCombatComponent::HandleAttackSpeedChanged(const FOnAttributeChangeData& Data)
{
	if (bPrimaryAttackHeld)
	{
		TryStartAutomaticFire();
	}
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

	const float StaminaCost = UAbilityCostAndCooldownManager::GetWeaponAttackStaminaCost(CharacterOwner);
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

	const float StaminaCost = UAbilityCostAndCooldownManager::GetWeaponAttackStaminaCost(CharacterOwner);
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

	const TSubclassOf<UGameplayEffect> CostEffectClass = UAbilityCostAndCooldownManager::GetCostGameplayEffectClass(this);
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
	if (!UAbilityCostAndCooldownManager::SetCostEffectMagnitudes(CostSpecHandle, 0.0f, StaminaCost))
	{
		return false;
	}
	return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*CostSpecHandle.Data.Get()).WasSuccessfullyApplied();
}

float UCombatComponent::GetWeaponDamageSourceMagnitude() const
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

float UCombatComponent::GetStrengthAdjustedWeaponDamageMagnitude(const float SourceStrength) const
{

	const bool bHasEquippedWeapon = GetCurrentWeaponActor() != nullptr;
	const float RawWeaponDamageAmount = bHasEquippedWeapon
		? GetWeaponDamageSourceMagnitude()
		: GetUnarmedDamageSourceMagnitude();
	const float SkillWeaponDamageBonus = bHasEquippedWeapon ? GetTemporaryWeaponDamageBonus() : 0.0f;
	const float WeaponDamageWithBonus = FMath::Max(RawWeaponDamageAmount, 0.0f) + SkillWeaponDamageBonus;
	return CalculateStrengthAdjustedWeaponDamage(WeaponDamageWithBonus, SourceStrength);
}

// 출처별 보너스는 서버에만 기록하고 소유 클라이언트에는 합계만 보낸다.
void UCombatComponent::SetTemporaryWeaponDamageBonus(UObject* SourceObject, const float DamageBonus)
{
	if (bEndingPlay || !HasCombatAuthority() || !IsValid(SourceObject))
	{
		return;
	}
	if (FMath::IsFinite(DamageBonus) && DamageBonus > 0.0f)
	{
		TemporaryWeaponDamageBonuses.Add(FObjectKey(SourceObject), DamageBonus);
	}
	else
	{
		TemporaryWeaponDamageBonuses.Remove(FObjectKey(SourceObject));
	}
	RefreshTemporaryWeaponDamageBonus();
}

void UCombatComponent::ClearTemporaryWeaponDamageBonus(UObject* SourceObject)
{
	if (!HasCombatAuthority() || !SourceObject)
	{
		return;
	}
	TemporaryWeaponDamageBonuses.Remove(FObjectKey(SourceObject));
	RefreshTemporaryWeaponDamageBonus();
}

float UCombatComponent::GetTemporaryWeaponDamageBonus() const
{
	if (!HasCombatAuthority())
	{
		return ReplicatedTemporaryWeaponDamageBonus;
	}
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

	ActiveComboDamageMultiplier = FMath::IsFinite(DamageMultiplier) ? FMath::Max(DamageMultiplier, 1.0f) : 1.0f;
}

// 무기 피해량과 출처만 정하고 실제 타격 처리는 맨손과 공유한다.
bool UCombatComponent::ApplyWeaponDamageToTarget(AActor* TargetActor)
{
	RefreshCachedReferences();
	AWeaponBase* Weapon = GetCurrentWeaponActor();
	return Weapon && ApplyAttackDamageToTarget(TargetActor,
		GetWeaponDamageSourceMagnitude() + GetTemporaryWeaponDamageBonus(), Weapon, Weapon, Weapon->ShouldTriggerHitReactOnDamage());
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

	StartUnarmedAttackTrace();
}

void UCombatComponent::ResetUnarmedAttackHitTracking()
{
	++UnarmedAttackTraceGeneration;
	PreviousUnarmedAttackTraceValid.Init(0, UnarmedCombatSettings.AttackTraces.Num());
	TrackedUnarmedAttackSectionName = NAME_None;
	HitActorsInCurrentUnarmedAttack.Reset();
}

// 공격 판정 창이 열려 있는 동안만 서버가 손·발의 충돌 검사를 반복한다.
void UCombatComponent::StartUnarmedAttackTrace()
{
	UWorld* World = GetWorld();
	if (bEndingPlay || !HasCombatAuthority() || bUnarmedAttackTraceActive || !World
		|| UnarmedCombatSettings.AttackTraces.IsEmpty() || CachedUnarmedAttackObjectTypes.IsEmpty()
		|| !FMath::IsFinite(UnarmedCombatSettings.TraceInterval) || UnarmedCombatSettings.TraceInterval <= 0.0f
		|| !FMath::IsFinite(UnarmedCombatSettings.TraceInterpolationDistance) || UnarmedCombatSettings.TraceInterpolationDistance <= 0.0f
		|| !FMath::IsFinite(UnarmedCombatSettings.MaxTraceTravelDistance) || UnarmedCombatSettings.MaxTraceTravelDistance <= 0.0f)
	{
		return;
	}
	bUnarmedAttackTraceActive = true;
	++UnarmedAttackTraceGeneration;
	PreviousUnarmedAttackTraceStartLocations.SetNumZeroed(UnarmedCombatSettings.AttackTraces.Num());
	PreviousUnarmedAttackTraceEndLocations.SetNumZeroed(UnarmedCombatSettings.AttackTraces.Num());
	PreviousUnarmedAttackTraceValid.Init(0, UnarmedCombatSettings.AttackTraces.Num());
	World->GetTimerManager().SetTimer(UnarmedAttackTraceTimerHandle, this, &ThisClass::PerformUnarmedAttackTrace,
		UnarmedCombatSettings.TraceInterval, true);
	// 즉시 타격의 콜백에서 공격이 끝나도 타이머를 다시 등록하지 않는다.
	PerformUnarmedAttackTrace();
}

void UCombatComponent::StopUnarmedAttackTrace()
{
	bUnarmedAttackTraceActive = false;
	++UnarmedAttackTraceGeneration;
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
	if (bEndingPlay || !bUnarmedAttackTraceActive || !OwnerActor || !HasCombatAuthority() || !SourceCharacter || !SourceMesh || !World)
	{
		return;
	}

	if (UnarmedCombatSettings.AttackTraces.IsEmpty()
		|| UnarmedCombatSettings.TraceObjectTypes.IsEmpty())
	{

		return;
	}

	const uint32 TraceGeneration = UnarmedAttackTraceGeneration;
	UnarmedAttackActorsToIgnore.Reset(1);
	UnarmedAttackActorsToIgnore.Add(SourceCharacter);
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
		if (TraceStart.ContainsNaN() || TraceEnd.ContainsNaN())
		{
			PreviousUnarmedAttackTraceValid[TraceIndex] = 0;
			continue;
		}
		bool bHasPreviousTrace = PreviousUnarmedAttackTraceValid[TraceIndex] != 0;
		if (bHasPreviousTrace)
		{
			const double TravelDistance = FMath::Max(
				FVector::Distance(PreviousUnarmedAttackTraceStartLocations[TraceIndex], TraceStart),
				FVector::Distance(PreviousUnarmedAttackTraceEndLocations[TraceIndex], TraceEnd));
			// 순간이동이나 큰 위치 보정은 이전 위치에서 이어서 휘두른 공격으로 취급하지 않는다.
			bHasPreviousTrace = FMath::IsFinite(TravelDistance) && TravelDistance <= UnarmedCombatSettings.MaxTraceTravelDistance;
		}
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
		const int32 MaxSteps = FMath::Clamp(UnarmedCombatSettings.MaxTraceInterpolationSteps, 1, 64);
		const int32 InterpolationCount = FMath::CeilToInt(FMath::Clamp(MaxTravelDistance / InterpolationDistance, 1.0f, static_cast<float>(MaxSteps)));

		for (int32 InterpolationIndex = 1; InterpolationIndex <= InterpolationCount; ++InterpolationIndex)
		{
			const float Alpha =
				static_cast<float>(InterpolationIndex) / static_cast<float>(InterpolationCount);
			const FVector InterpolatedTraceStart = FMath::Lerp(PreviousTraceStart, TraceStart, Alpha);
			const FVector InterpolatedTraceEnd = FMath::Lerp(PreviousTraceEnd, TraceEnd, Alpha);
			InterpolatedUnarmedHitResults.Reset();
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
				InterpolatedUnarmedHitResults,
				true,
				FLinearColor::Red,
				FLinearColor::Green,
				UnarmedTraceDebugDrawTime);
			UnarmedAttackHitResults.Append(InterpolatedUnarmedHitResults);
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
			if (TraceGeneration != UnarmedAttackTraceGeneration)
			{
				return;
			}
		}
	}
}

bool UCombatComponent::ApplyUnarmedDamageToTarget(AActor* TargetActor)
{
	return ApplyAttackDamageToTarget(TargetActor, GetUnarmedDamageSourceMagnitude(), GetOwner(), GetOwner(), true);
}

// 팀·능력치·치명타·피격 반응을 같은 순서로 적용한다. GE와 AttributeSet의 피해 규칙은 유지한다.
bool UCombatComponent::ApplyAttackDamageToTarget(AActor* TargetActor, const float RawDamage,
	UObject* SourceObject, AActor* DamageCauser, const bool bAllowHitReact)
{
	RefreshCachedReferences();
	ACharacterBase* Source = CachedOwner.Get();
	ACharacterBase* Target = Cast<ACharacterBase>(TargetActor);
	UPdAbilitySystemComponent* SourceASC = CachedASC.Get();
	UPdAbilitySystemComponent* TargetASC = IsValid(Target) ? Target->GetPdAbilitySystemComponent() : nullptr;
	if (bEndingPlay || !HasCombatAuthority() || !IsValid(Source) || !IsValid(Target)
		|| Source == Target || !SourceASC || !TargetASC || !Source->CanDamageCharacterByTeam(Target))
	{
		return false;
	}
	UBasicAttributeSet* SourceAttributes = const_cast<UBasicAttributeSet*>(SourceASC->GetSet<UBasicAttributeSet>());
	UBasicAttributeSet* TargetAttributes = const_cast<UBasicAttributeSet*>(TargetASC->GetSet<UBasicAttributeSet>());
	if (!SourceAttributes || !TargetAttributes || !FMath::IsFinite(RawDamage))
	{
		return false;
	}
	const float BaseDamage = CalculateStrengthAdjustedWeaponDamage(RawDamage, SourceAttributes->GetStrength()) * ActiveComboDamageMultiplier;
	if (!FMath::IsFinite(BaseDamage) || BaseDamage <= 0.0f)
	{
		return false;
	}
	SourceAttributes->ConsumeOutgoingDamage();
	if (!ApplyDamageEffect(SourceASC, SourceASC, CombatDamageSettings.OutgoingDamageEffectClass, BaseDamage, Target))
	{
		return false;
	}
	const float FinalDamage = SourceAttributes->ConsumeOutgoingDamage();
	const bool bCritical = SourceAttributes->ConsumeOutgoingDamageCriticalHit();
	if (!FMath::IsFinite(FinalDamage) || FinalDamage <= 0.0f)
	{
		return false;
	}
	TargetAttributes->SetPendingIncomingDamageCriticalHit(bCritical);
	TargetAttributes->SetPendingIncomingDamageAllowHitReact(bAllowHitReact);
	if (!ApplyDamageEffect(SourceASC, TargetASC, CombatDamageSettings.IncomingDamageEffectClass,
		FinalDamage, SourceObject, Source, DamageCauser))
	{
		TargetAttributes->SetPendingIncomingDamageCriticalHit(false);
		TargetAttributes->SetPendingIncomingDamageAllowHitReact(true);
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
		? GetWeaponDamageSourceMagnitude()
		: 0.0f;
}

bool UCombatComponent::HasCombatAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor && OwnerActor->HasAuthority();
}

void UCombatComponent::RefreshTemporaryWeaponDamageBonus()
{
	for (auto It = TemporaryWeaponDamageBonuses.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Key().ResolveObjectPtr()))
		{
			It.RemoveCurrent();
		}
	}
	const float NewBonus = GetTemporaryWeaponDamageBonus();
	if (ReplicatedTemporaryWeaponDamageBonus == NewBonus)
	{
		return;
	}
	ReplicatedTemporaryWeaponDamageBonus = NewBonus;
	MARK_PROPERTY_DIRTY_FROM_NAME(UCombatComponent, ReplicatedTemporaryWeaponDamageBonus, this);
	GetOwner()->ForceNetUpdate();
	OnRep_TemporaryWeaponDamageBonus();
}

// 서버의 실제 보너스가 바뀌면 능력치 UI도 공격력·방어 파생값을 다시 계산한다.
void UCombatComponent::OnRep_TemporaryWeaponDamageBonus()
{
	OnDamageBonusChanged.Broadcast();
}

bool UCombatComponent::ApplyDamageEffect(UPdAbilitySystemComponent* SourceASC, UPdAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> DamageEffectClass, float Magnitude, UObject* SourceObject,
	AActor* InstigatorActor, AActor* EffectCauserActor) const
{
	FGameplayTag DamageMagnitudeSetByCallerTag;
	const bool bHasDamageMagnitudeTag =
		SourceASC && SourceASC->ResolveDamageMagnitudeSetByCallerTag(DamageMagnitudeSetByCallerTag);
	if (!HasCombatAuthority() || !SourceASC || !TargetASC || !DamageEffectClass || !FMath::IsFinite(Magnitude) || Magnitude <= 0.0f
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
