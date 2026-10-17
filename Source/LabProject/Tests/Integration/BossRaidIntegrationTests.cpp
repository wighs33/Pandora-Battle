#include "Tests/Integration/IntegrationTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AI/Monster/MonsterCharacter.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Character/CharacterBase.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Common/LabGameplayTags.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "EngineUtils.h"
#include "GameplayEffect.h"
#include "Mode/WorldBossRaid.h"
#include "UI/HUD/Player/WorldBossBarWidget.h"
#include "UObject/UObjectIterator.h"

// 보스 레이드 월드에서 월드 보스가 받은 피해가 플레이어의 도전 기록과 HUD 체력바로 이어지는지 본다.
// -game 프로세스에서 백엔드 없이 돈다. 백엔드가 없으면 보스는 이 월드의 체력으로 싸운다.

namespace
{
	const FString ColosseumMap = TEXT("/Game/Map/LV_Colosseum");

	AWorldBossRaid* GetWorldBossRaid()
	{
		return AWorldBossRaid::Find(PdIntegrationTest::GetGameWorld());
	}

	AMonsterCharacter* GetLivingWorldBoss()
	{
		UWorld* World = PdIntegrationTest::GetGameWorld();
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<AMonsterCharacter> It(World); It; ++It)
		{
			if (It->IsWorldBoss() && It->IsMonsterReadyForAI())
			{
				return *It;
			}
		}
		return nullptr;
	}

	const FWorldBossAttempt* GetPlayerAttempt()
	{
		const AWorldBossRaid* Raid = GetWorldBossRaid();
		const APlayerController* Controller = PdIntegrationTest::GetPlayerController();
		return Raid && Controller ? Raid->FindAttempt(Controller->PlayerState) : nullptr;
	}

	/** 보스가 플레이어를 공격하거나 움직이지 않게 한다. */
	void StopBoss(AMonsterCharacter& Boss)
	{
		const AAIController* BossController = Cast<AAIController>(Boss.GetController());
		if (BossController && BossController->GetBrainComponent())
		{
			BossController->GetBrainComponent()->StopLogic(TEXT("Integration test"));
		}
		Boss.SetAttackEnabled(false);
	}

	/** 플레이어 ASC가 보스에게 피해 효과를 건다. 효과 맥락의 공격자가 플레이어라 보스가 피해를 플레이어 것으로 센다. */
	void DamageBossAsPlayer(const float Damage)
	{
		AMonsterCharacter* Boss = GetLivingWorldBoss();
		const APlayerController* Controller = PdIntegrationTest::GetPlayerController();
		const ACharacterBase* Player = Controller ? Cast<ACharacterBase>(Controller->GetPawn()) : nullptr;
		UAbilitySystemComponent* PlayerAbilities = Player ? Player->GetPdAbilitySystemComponent() : nullptr;
		const UEnemyBaseDefinition* Definition = Boss ? Boss->GetMonsterDefinition() : nullptr;
		const TSubclassOf<UGameplayEffect> DamageEffect =
			Definition ? Definition->GetMonsterPresentationSettings().ContactDamageEffectClass : nullptr;
		if (!PlayerAbilities || !DamageEffect)
		{
			return;
		}
		const FGameplayEffectSpecHandle Spec = PlayerAbilities->MakeOutgoingSpec(DamageEffect, 1.f, PlayerAbilities->MakeEffectContext());
		Spec.Data->SetSetByCallerMagnitude(LabGameplayTags::Data_Damage, Damage);
		PlayerAbilities->ApplyGameplayEffectSpecToTarget(*Spec.Data, Boss->GetAbilitySystemComponent());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdWorldBossDamageTest, "LabProject.Integration.BossRaid.WorldBossDamage",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 들어오면 도전 시간이 흐르고, 보스가 실제로 잃은 체력만큼 이번 도전 딜이 쌓이며(남은 체력을 넘는 피해는 빼고), HUD에 긴 체력바가 뜬다.
bool FPdWorldBossDamageTest::RunTest(const FString& Parameters)
{
	struct FHealth
	{
		float Max = 0.f;
	};
	TSharedRef<FHealth> Health = MakeShared<FHealth>();

	PdIntegrationTest::OpenMap(this, ColosseumMap, 0.f, TEXT("BossRaid=1"));
	PdIntegrationTest::WaitUntil(this, TEXT("월드 보스 등장"), 60.f, []() { return GetLivingWorldBoss() && GetPlayerAttempt(); });
	PdIntegrationTest::Step(0.f, [this, Health]()
	{
		AMonsterCharacter* Boss = GetLivingWorldBoss();
		const AWorldBossRaid* Raid = GetWorldBossRaid();
		const FWorldBossAttempt* Attempt = GetPlayerAttempt();
		if (!TestNotNull(TEXT("월드 보스"), Boss) || !TestNotNull(TEXT("레이드"), Raid) || !TestNotNull(TEXT("플레이어 도전"), Attempt))
		{
			return;
		}
		StopBoss(*Boss);
		Health->Max = Raid->GetBossMaxHealth();
		TestTrue(TEXT("HUD 체력바가 보스 최대 체력을 안다"), Health->Max > 0.f);
		TestEqual(TEXT("들어온 직후에는 보스가 다치지 않았다"), Raid->GetBossHealth(), Health->Max);
		TestTrue(TEXT("도전 시간이 흐른다"), Raid->GetAttemptSecondsLeft(*Attempt) > 0.0 && !Attempt->bEnded);
		TestEqual(TEXT("아직 넣은 딜이 없다"), Attempt->Damage, 0.0);

		bool bBarVisible = false;
		for (TObjectIterator<UWorldBossBarWidget> It; It; ++It)
		{
			bBarVisible |= It->GetWorld() == Boss->GetWorld() && It->GetParent() && It->IsVisible();
		}
		TestTrue(TEXT("플레이어 HUD에 월드 보스 체력바가 있다"), bBarVisible);
		DamageBossAsPlayer(100.f);
	});
	PdIntegrationTest::WaitUntil(this, TEXT("첫 피해 집계"), 5.f, []()
	{
		const FWorldBossAttempt* Attempt = GetPlayerAttempt();
		return Attempt && Attempt->Damage > 0.0;
	});
	PdIntegrationTest::Step(0.f, [this, Health]()
	{
		const AWorldBossRaid* Raid = GetWorldBossRaid();
		const FWorldBossAttempt* Attempt = GetPlayerAttempt();
		if (!Raid || !Attempt)
		{
			return;
		}
		// 방어력이 피해를 줄이므로, 딜은 건 피해가 아니라 보스가 실제로 잃은 체력이다.
		TestEqual(TEXT("이번 도전 딜 = 보스가 잃은 체력"), Attempt->Damage, double(Health->Max - Raid->GetBossHealth()), 0.01);
		TestTrue(TEXT("방어력을 거친 딜은 건 피해 이하"), Attempt->Damage <= 100.0);
		DamageBossAsPlayer(1.0e6f);
	});
	PdIntegrationTest::WaitUntil(this, TEXT("마지막 일격 집계"), 5.f, []()
	{
		const AWorldBossRaid* Raid = GetWorldBossRaid();
		return Raid && Raid->GetBossHealth() <= 0.f;
	});
	PdIntegrationTest::Step(0.f, [this, Health]()
	{
		const FWorldBossAttempt* Attempt = GetPlayerAttempt();
		if (TestNotNull(TEXT("플레이어 도전"), Attempt))
		{
			TestEqual(TEXT("남은 체력을 넘는 피해는 딜에 들어가지 않는다"), Attempt->Damage, double(Health->Max), 0.01);
		}
	});
	return true;
}

#endif
