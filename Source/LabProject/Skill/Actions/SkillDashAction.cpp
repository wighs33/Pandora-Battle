#include "Skill/Actions/SkillDashAction.h"

#include "AbilitySystem/Ability/SkillAbility.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Character/CharacterBase.h"
#include "Definition/AbilitySystem/SkillDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"

void USkillDashAction::OnStart()
{
	ACharacterBase* Character = GetAbility() ? GetAbility()->GetPdCharacterFromActorInfo() : nullptr;
	if (!Character || Speed <= 0.0f || Duration <= 0.0f)
	{
		Finish(false);
		return;
	}
	FVector Direction = Character->GetLastMovementInputVector();
	if (const auto* Movement = Character->GetCharacterMovement(); Movement && !Movement->GetCurrentAcceleration().IsNearlyZero())
	{
		Direction = Movement->GetCurrentAcceleration();
	}
	for (int32 Index = 0; Index < GetContext().EventData.TargetData.Num(); ++Index)
	{
		const auto* Data = GetContext().EventData.TargetData.Get(Index);
		const auto* Hit = Data ? Data->GetHitResult() : nullptr;
		if (Hit && !Hit->Location.IsNearlyZero())
		{
			// TargetData는 월드 위치를 담는다. 대시에는 캐릭터에서 그 위치로 가는 방향이 필요하다.
			const FVector ToTarget = (Hit->Location - Character->GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				Direction = ToTarget;
			}
			break;
		}
	}
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetActorForwardVector();
	}
	Task = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(GetAbility(), TEXT("Dash"),
		Direction.GetSafeNormal2D(), Speed, Duration, false, nullptr, ERootMotionFinishVelocityMode::ClampVelocity,
		FVector::ZeroVector, Character->GetCharacterMovement()->GetMaxSpeed(), bEnableGravity);
	if (!Task)
	{
		Finish(false);
		return;
	}
	GetAbility()->SpawnConfiguredCharacterDecal();
	ContactDamage.Start(*GetAbility(), *this);
	if (const auto* Skill = GetAbility()->GetSourceSkillDataAsset(); Skill && Skill->Niagara.GameplayCueTag.IsValid())
	{
		FGameplayCueParameters Cue;
		Cue.Location = Character->GetActorLocation();
		Cue.Instigator = Character;
		Cue.EffectCauser = Character;
		Cue.RawMagnitude = bHideCharacter ? 1.0f : -1.0f;
		GetAbility()->K2_AddGameplayCueWithParams(Skill->Niagara.GameplayCueTag, Cue, true);
	}
	Task->OnFinish.AddDynamic(this, &ThisClass::Completed);
	Task->ReadyForActivation();
}

void USkillDashAction::Completed()
{
	Finish();
}

void USkillDashAction::OnStop()
{
	ContactDamage.Stop();
	if (!Task)
	{
		return;
	}
	Task->OnFinish.RemoveAll(this);
	Task->EndTask();
	Task = nullptr;
}
