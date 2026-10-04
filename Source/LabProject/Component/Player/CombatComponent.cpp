#include "Component/Player/CombatComponent.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystem/Ability/PdGameplayAbility.h"
#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Common/LabGameplayTags.h"
#include "Interface/ComboAttackInterface.h"
#include "AbilitySystem/Ability/EquipmentAbilityData.h"
#include "Definition/Common/ProjectTagDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Definition/Item/ItemDefinition.h"
#include "Mode/PdPlayerController.h"
#include "Component/Player/EquipmentComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Weapon/WeaponBase.h"
#include "Weapon/RangedWeaponBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CombatComponent)

namespace
{
	// 플레이어 원거리 공격 한 번의 스태미나 비용. AI는 스태미나를 쓰지 않으므로 0이다.
	float GetPlayerRangedAttackStaminaCost(const ACharacterBase& Character)
	{
		return Character.IsPlayerControlled()
			? UPdGameplayAbility::GetWeaponAttackStaminaCost(&Character)
			: 0.0f;
	}

	bool HasStaminaFor(const UPdAbilitySystemComponent* AbilitySystemComponent, const float StaminaCost)
	{
		const UBasicAttributeSet* AttributeSet = AbilitySystemComponent
			? AbilitySystemComponent->GetSet<UBasicAttributeSet>()
			: nullptr;
		return AttributeSet
			&& AttributeSet->GetStamina() + UE_SMALL_NUMBER >= StaminaCost;
	}
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

	// ASC는 빙의와 PlayerState 도착 순서에 따라 늦게 준비되므로, 준비 알림에서만 공격 속도 변화를 구독한다.
	AbilitySystemSubscription.SubscribeToCharacter(
		GetCharacter(),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemReady),
		FPdAbilitySystemReadyDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemReleased));
	// Pawn 정의가 BeginPlay 이전에 적용된 경우에도 몽타주 로딩을 시작한다.
	BeginUnarmedAttackMontagePreload();
}

// 캐릭터 종료 시 입력 반복·타격 판정·로딩·ASC 구독을 함께 정리한다.
void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	StopPrimaryAttack();
	StopUnarmedAttackTrace();
	ReleaseUnarmedAttackMontagePreload();
	AbilitySystemSubscription.Reset();
	BindAbilitySystem(nullptr);
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

void UCombatComponent::HandleAbilitySystemReady(ACharacterBase* Character, UPdAbilitySystemComponent* ReadyAbilitySystem)
{
	BindAbilitySystem(ReadyAbilitySystem);
}

void UCombatComponent::HandleAbilitySystemReleased(ACharacterBase* Character, UPdAbilitySystemComponent* ReleasedAbilitySystem)
{
	BindAbilitySystem(nullptr);
}

// 공격 속도가 바뀌면 누르고 있는 연사의 다음 입력 시점을 다시 잡고,
// 방어력 계산이 쓰는 근력 반영 무기 피해는 이 컴포넌트가 계산하도록 ASC에 등록한다.
void UCombatComponent::BindAbilitySystem(UPdAbilitySystemComponent* AbilitySystem)
{
	if (BoundAbilitySystem.Get() == AbilitySystem)
	{
		return;
	}

	if (UPdAbilitySystemComponent* PreviousAbilitySystem = BoundAbilitySystem.Get())
	{
		PreviousAbilitySystem->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetAttackSpeedAttribute())
			.Remove(AttackSpeedChangedDelegateHandle);
		PreviousAbilitySystem->ClearFinalStrengthDamageProvider(this);
	}
	AttackSpeedChangedDelegateHandle.Reset();
	BoundAbilitySystem = AbilitySystem;
	if (AbilitySystem)
	{
		AttackSpeedChangedDelegateHandle = AbilitySystem->GetGameplayAttributeValueChangeDelegate(
			UBasicAttributeSet::GetAttackSpeedAttribute()).AddUObject(this, &ThisClass::HandleAttackSpeedChanged);
		AbilitySystem->SetFinalStrengthDamageProvider(
			FPdFinalStrengthDamageProvider::CreateUObject(this, &ThisClass::GetStrengthAdjustedWeaponDamageMagnitude));
	}
}

ACharacterBase* UCombatComponent::GetCharacter() const
{
	return Cast<ACharacterBase>(GetOwner());
}

// 플레이어·AI가 선택한 피해 설정과 맨손 연출 설정을 적용한다.
void UCombatComponent::ApplySettings(const FCombatDamageSettings& DamageSettings, const FUnarmedCombatSettings& UnarmedSettings)
{
	StopUnarmedAttackTrace();
	CombatDamageSettings = DamageSettings;
	UnarmedCombatSettings = UnarmedSettings;
	ResetUnarmedAttackHitTracking();
	if (HasBegunPlay() && !bEndingPlay)
	{
		BeginUnarmedAttackMontagePreload();
	}
}

void UCombatComponent::PlayUnarmedComboWindowStartEffect() const
{
	ACharacterBase* Character = GetCharacter();
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
		StopPrimaryAttack();
		return;
	}

	bPrimaryAttackHeld = true;
	LastPrimaryAttackRequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	ProcessAttackInput();
	TryStartAutomaticFire();
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

	ARangedWeaponBase* WeaponActor = Cast<ARangedWeaponBase>(GetCurrentWeaponActor());
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

	if (APdPlayerController* Controller = Cast<APdPlayerController>(PlayerCharacter->GetController()))
	{
		Controller->ShowAimCrosshair(WeaponActor->GetAimCrosshairWidgetTag());
	}
}

void UCombatComponent::StopAim()
{
	APdPlayer* PlayerCharacter = GetPlayerOwner();
	if (!PlayerCharacter)
	{
		return;
	}

	if (APdPlayerController* Controller = Cast<APdPlayerController>(PlayerCharacter->GetController()))
	{
		Controller->HideAimCrosshair();
	}

	ARangedWeaponBase* WeaponActor = Cast<ARangedWeaponBase>(GetCurrentWeaponActor());
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

void UCombatComponent::StopPrimaryAttack()
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
	if (bEndingPlay || !HasCombatAuthority() || IsPrimaryAttackBlockedByAbilityTags() || !ActivationKey.IsValidKey())
	{
		return;
	}
	UAbilitySystemComponent* ASC = GetPlayerAbilitySystemComponent();
	FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(AbilityHandle) : nullptr;
	UGameplayAbility* Attack = Spec ? Spec->GetPrimaryInstance() : nullptr;
	IComboAttackInterface* ComboAttack = Cast<IComboAttackInterface>(Attack);
	const FGameplayTag SelectedTag = GetSelectedAttackAbilityTag(GetCurrentWeaponActor());
	if (!ComboAttack || !SelectedTag.IsValid() || !Attack->GetAssetTags().HasTagExact(SelectedTag)
		|| Attack->GetCurrentActivationInfo().GetActivationPredictionKey() != ActivationKey)
	{
		return;
	}
	if (Spec->IsActive())
	{
		if (ClientExpectedSectionName == ComboAttack->GetNextAttackSectionName())
		{
			ComboAttack->RequestNextComboInput();
		}
		return;
	}
	// 같은 실행이 자연 종료된 직후의 입력만 한 번 보정한다. 취소·오래된 입력은 재시작하지 않는다.
	if (ComboAttack->TryConsumeLateComboInput())
	{
		ASC->TryActivateAbility(AbilityHandle);
	}
}

APdPlayer* UCombatComponent::GetPlayerOwner() const
{
	return Cast<APdPlayer>(GetOwner());
}

AWeaponBase* UCombatComponent::GetCurrentWeaponActor() const
{
	const ACharacterBase* CharacterOwner = GetCharacter();

	const UEquipmentComponent* EquipmentComponent = CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeaponActor() : nullptr;
}

UAbilitySystemComponent* UCombatComponent::GetPlayerAbilitySystemComponent() const
{
	APdPlayer* PlayerCharacter = GetPlayerOwner();
	return PlayerCharacter ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter) : nullptr;
}

// 콤보를 받는 공격이 이미 실행 중이면 그 능력을 돌려준다. 콤보 계약이 없으면 nullptr이라 새로 실행한다.
UGameplayAbility* UCombatComponent::ResolveActiveAttackAbility(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTagContainer& AbilityTags) const
{
	const UPdAbilitySystemComponent* PdASC = Cast<UPdAbilitySystemComponent>(AbilitySystemComponent);
	const FGameplayAbilitySpec* ActiveAbilitySpec = PdASC ? PdASC->FindActiveAbilitySpecByTags(AbilityTags) : nullptr;
	UGameplayAbility* ActiveAbility = ActiveAbilitySpec ? ActiveAbilitySpec->GetPrimaryInstance() : nullptr;
	return Cast<IComboAttackInterface>(ActiveAbility) ? ActiveAbility : nullptr;
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

	if (UGameplayAbility* ActiveAttackAbility = ResolveActiveAttackAbility(AbilitySystemComponent, AttackTagContainer))
	{
		RequestNextAttackSection(ActiveAttackAbility);
		return;
	}

	TryActivateAttackAbility(AbilitySystemComponent, AttackTagContainer);
}

bool UCombatComponent::IsPrimaryAttackBlockedByAbilityTags() const
{
	const ACharacterBase* CharacterOwner = GetCharacter();
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
	const ARangedWeaponBase* RangedWeapon = Cast<ARangedWeaponBase>(WeaponActor);
	if (!RangedWeapon || !RangedWeapon->SupportsAimInput())
	{
		return true;
	}

	return PlayerCharacter && WeaponActor->HandlePrimaryAttack(PlayerCharacter);
}

bool UCombatComponent::ShouldUseRangedAttackAbility(const AWeaponBase* WeaponActor) const
{
	const ARangedWeaponBase* RangedWeapon = Cast<ARangedWeaponBase>(WeaponActor);
	return RangedWeapon && RangedWeapon->SupportsAimInput();
}

FGameplayTag UCombatComponent::GetSelectedAttackAbilityTag(const AWeaponBase* WeaponActor) const
{
	if (ShouldUseRangedAttackAbility(WeaponActor))
	{
		return UProjectTagDefinition::Get(this)->GetCombatRangedAttackAbilityTag();
	}

	return WeaponActor ? UProjectTagDefinition::Get(this)->GetCombatAttackAbilityTag() : UProjectTagDefinition::Get(this)->GetCombatPunchAbilityTag();
}

void UCombatComponent::RequestNextAttackSection(UGameplayAbility* ActiveAttackAbility)
{
	IComboAttackInterface* ComboAttack = Cast<IComboAttackInterface>(ActiveAttackAbility);
	if (!ComboAttack)
	{
		return;
	}

	const FName ClientExpectedSectionName = ComboAttack->GetNextAttackSectionName();
	const bool bAccepted = ComboAttack->RequestNextComboInput();
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
		StopPrimaryAttack();
		return false;
	}
	AWeaponBase* Weapon = GetCurrentWeaponActor();
	UWorld* World = GetWorld();
	const float Interval = Weapon ? Weapon->GetAutomaticFireInterval() : 0.0f;
	if (!World || !Weapon || !Weapon->SupportsAutomaticFire() || !FMath::IsFinite(Interval) || Interval <= 0.0f)
	{
		StopPrimaryAttack();
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
		StopPrimaryAttack();
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
	const ACharacterBase* CharacterOwner = GetCharacter();
	if (!CharacterOwner)
	{
		return false;
	}

	const float StaminaCost = GetPlayerRangedAttackStaminaCost(*CharacterOwner);
	return StaminaCost <= 0.0f
		|| HasStaminaFor(CharacterOwner->GetPdAbilitySystemComponent(), StaminaCost);
}

bool UCombatComponent::TryCommitRangedWeaponAttackStamina()
{
	if (!HasCombatAuthority())
	{
		return false;
	}

	ACharacterBase* CharacterOwner = GetCharacter();
	if (!CharacterOwner)
	{
		return false;
	}

	const float StaminaCost = GetPlayerRangedAttackStaminaCost(*CharacterOwner);
	if (StaminaCost <= 0.0f)
	{
		return true;
	}

	UPdAbilitySystemComponent* AbilitySystemComponent =
		CharacterOwner->GetPdAbilitySystemComponent();
	if (!HasStaminaFor(AbilitySystemComponent, StaminaCost))
	{
		return false;
	}

	const TSubclassOf<UGameplayEffect> CostEffectClass = UPdGameplayAbility::GetCostGameplayEffectClass(this);
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
	if (!UPdGameplayAbility::SetCostEffectMagnitudes(CostSpecHandle, 0.0f, StaminaCost))
	{
		return false;
	}
	return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*CostSpecHandle.Data.Get()).WasSuccessfullyApplied();
}

float UCombatComponent::GetWeaponDamageSourceMagnitude() const
{
	const ACharacterBase* CharacterOwner = GetCharacter();

	const UEquipmentComponent* EquipmentComponent = CharacterOwner ? CharacterOwner->GetEquipmentComponent() : nullptr;
	const FGameplayTag WeaponDamageSourceTag = UProjectTagDefinition::Get(this)->GetCombatWeaponDamageSourceTag();
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
	AWeaponBase* Weapon = GetCurrentWeaponActor();
	PdDamageRules::FOutgoingDamage Damage;
	return Weapon && BuildWeaponDamage(*Weapon, Damage)
		&& ApplyOutgoingDamageToTarget(TargetActor, Damage, Weapon, Weapon);
}

bool UCombatComponent::BuildWeaponDamage(const AWeaponBase& Weapon, PdDamageRules::FOutgoingDamage& OutDamage) const
{
	// 무기 능력치와 무기 피해 보너스는 장착 중인 무기에서만 읽는다.
	return GetCurrentWeaponActor() == &Weapon
		&& BuildOutgoingDamage(GetWeaponDamageSourceMagnitude() + GetTemporaryWeaponDamageBonus(),
			Weapon.ShouldTriggerHitReactOnDamage(), OutDamage);
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

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		return;
	}

	UnarmedAttackMontageLease = ContentSubsystem->AcquireContent(
		TArray<FSoftObjectPath>{UnarmedCombatSettings.AttackMontage.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(
			this,
			&ThisClass::HandleUnarmedAttackMontagePreloadComplete));
}

void UCombatComponent::HandleUnarmedAttackMontagePreloadComplete()
{
	CachedUnarmedAttackMontage = UnarmedCombatSettings.AttackMontage.Get();
}

void UCombatComponent::ReleaseUnarmedAttackMontagePreload()
{
	UnarmedAttackMontageLease.Reset();
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

	UnarmedAttackSweep.EnterSection(AttackSectionName, UnarmedCombatSettings.AttackTraces.Num());
	StartUnarmedAttackTrace();
}

void UCombatComponent::ResetUnarmedAttackHitTracking()
{
	UnarmedAttackSweep.ResetHitTracking(UnarmedCombatSettings.AttackTraces.Num());
}

// 공격 판정 창이 열려 있는 동안만 서버가 손·발의 충돌 검사를 반복한다.
void UCombatComponent::StartUnarmedAttackTrace()
{
	UWorld* World = GetWorld();
	if (bEndingPlay || !HasCombatAuthority() || UnarmedAttackSweep.IsActive() || !World
		|| !FUnarmedAttackSweep::CanSweep(UnarmedCombatSettings))
	{
		return;
	}
	UnarmedAttackSweep.Begin(UnarmedCombatSettings.AttackTraces.Num());
	World->GetTimerManager().SetTimer(UnarmedAttackTraceTimerHandle, this, &ThisClass::PerformUnarmedAttackTrace,
		UnarmedCombatSettings.TraceInterval, true);
	// 즉시 타격의 콜백에서 공격이 끝나도 타이머를 다시 등록하지 않는다.
	PerformUnarmedAttackTrace();
}

void UCombatComponent::StopUnarmedAttackTrace()
{
	UnarmedAttackSweep.End();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UnarmedAttackTraceTimerHandle);
	}

	UnarmedAttackTraceTimerHandle.Invalidate();
}

void UCombatComponent::PerformUnarmedAttackTrace()
{
	AActor* OwnerActor = GetOwner();
	ACharacterBase* SourceCharacter = GetCharacter();
	USkeletalMeshComponent* SourceMesh = SourceCharacter ? SourceCharacter->GetMesh() : nullptr;
	if (bEndingPlay || !UnarmedAttackSweep.IsActive() || !OwnerActor || !HasCombatAuthority() || !SourceCharacter || !SourceMesh || !GetWorld())
	{
		return;
	}

	const UGameSettingDefinition* SettingDefinition =
		UGameSettingsSubsystem::ResolveGameSettingDefinition(this);
	const bool bDrawAttackDebug = SettingDefinition
		&& SettingDefinition->bDrawAttackDebugVisualization;
	UnarmedAttackSweep.Sweep(*this, UnarmedCombatSettings, *SourceCharacter, *SourceMesh, bDrawAttackDebug,
		[this](AActor* HitActor)
		{
			ApplyUnarmedDamageToTarget(HitActor);
		});
}

bool UCombatComponent::ApplyUnarmedDamageToTarget(AActor* TargetActor)
{
	return ApplyAttackDamageToTarget(TargetActor, GetUnarmedDamageSourceMagnitude(), GetOwner(), GetOwner(), true);
}

// 팀·능력치·치명타·피격 반응을 같은 순서로 적용한다. 치명타는 공격자 능력치로 한 번 판정하고,
// 판정 결과와 피격 반응 여부는 피해 Spec의 태그로 대상 AttributeSet에 전달한다.
bool UCombatComponent::ApplyAttackDamageToTarget(AActor* TargetActor, const float RawDamage,
	UObject* SourceObject, AActor* DamageCauser, const bool bAllowHitReact)
{
	PdDamageRules::FOutgoingDamage Damage;
	return BuildOutgoingDamage(RawDamage, bAllowHitReact, Damage)
		&& ApplyOutgoingDamageToTarget(TargetActor, Damage, SourceObject, DamageCauser);
}

// 공격자 쪽 계산: 근력과 콤보 배율을 반영하고 치명타를 굴린다. 방어·저항은 맞는 쪽 AttributeSet이 계산한다.
bool UCombatComponent::BuildOutgoingDamage(const float RawDamage, const bool bAllowHitReact,
	PdDamageRules::FOutgoingDamage& OutDamage) const
{
	OutDamage = PdDamageRules::FOutgoingDamage();
	const ACharacterBase* Source = GetCharacter();
	const UPdAbilitySystemComponent* SourceASC = IsValid(Source) ? Source->GetPdAbilitySystemComponent() : nullptr;
	const UBasicAttributeSet* SourceAttributes = SourceASC ? SourceASC->GetSet<UBasicAttributeSet>() : nullptr;
	if (bEndingPlay || !HasCombatAuthority() || !SourceAttributes || !FMath::IsFinite(RawDamage))
	{
		return false;
	}
	const float BaseDamage = CalculateStrengthAdjustedWeaponDamage(RawDamage, SourceAttributes->GetStrength()) * ActiveComboDamageMultiplier;
	OutDamage.Damage = SourceAttributes->CalculateOutgoingDamage(BaseDamage, OutDamage.bCriticalHit);
	OutDamage.bAllowHitReact = bAllowHitReact;
	return OutDamage.IsValid();
}

bool UCombatComponent::ApplyOutgoingDamageToTarget(AActor* TargetActor, const PdDamageRules::FOutgoingDamage& Damage,
	UObject* SourceObject, AActor* DamageCauser)
{
	ACharacterBase* Source = GetCharacter();
	ACharacterBase* Target = Cast<ACharacterBase>(TargetActor);
	UPdAbilitySystemComponent* SourceASC = IsValid(Source) ? Source->GetPdAbilitySystemComponent() : nullptr;
	UPdAbilitySystemComponent* TargetASC = IsValid(Target) ? Target->GetPdAbilitySystemComponent() : nullptr;
	if (bEndingPlay || !HasCombatAuthority() || !Damage.IsValid() || !IsValid(Source) || !IsValid(Target)
		|| Source == Target || !SourceASC || !TargetASC || !TargetASC->GetSet<UBasicAttributeSet>()
		|| !Source->CanDamageCharacterByTeam(Target))
	{
		return false;
	}

	FGameplayTagContainer DamageSpecTags;
	if (Damage.bCriticalHit)
	{
		DamageSpecTags.AddTag(LabGameplayTags::Effect_Damage_Critical);
	}
	if (!Damage.bAllowHitReact)
	{
		DamageSpecTags.AddTag(LabGameplayTags::Effect_Damage_NoHitReaction);
	}
	return ApplyDamageEffect(SourceASC, TargetASC, CombatDamageSettings.IncomingDamageEffectClass,
		Damage.Damage, SourceObject, Source, DamageCauser, DamageSpecTags);
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
	AActor* InstigatorActor, AActor* EffectCauserActor, const FGameplayTagContainer& DamageSpecTags) const
{
	const FGameplayTag DamageMagnitudeSetByCallerTag = UProjectTagDefinition::GetDefaultDefinition()->GetSetByCallerDamageMagnitudeTag();
	if (!HasCombatAuthority() || !SourceASC || !TargetASC || !DamageEffectClass || !FMath::IsFinite(Magnitude) || Magnitude <= 0.0f
		|| !DamageMagnitudeSetByCallerTag.IsValid())
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
	SpecHandle.Data->AppendDynamicAssetTags(DamageSpecTags);
	const FActiveGameplayEffectHandle AppliedEffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	return AppliedEffectHandle.WasSuccessfullyApplied();
}
