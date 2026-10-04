#include "Component/Player/CombatComponent.h"

#include "AbilitySystem/Ability/EquipmentAbilityData.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Settings/GameSettingDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "Settings/GameSettingsSubsystem.h"
#include "TimerManager.h"

// 맨손 공격: 몽타주 미리 읽기, 공격 판정 창 동안 손·발 충돌 검사, 맨손 피해량.

UNiagaraSystem* UCombatComponent::GetUnarmedComboWindowStartEffect() const
{
	return UnarmedCombatSettings.ComboWindowStartEffect;
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
