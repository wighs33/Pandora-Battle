#include "Tests/Integration/IntegrationTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystem/Effects/EquipmentStatsEffect.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Character/CharacterBase.h"
#include "Character/EnemyBase.h"
#include "Common/InfoUiTypes.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/ControllerInputComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/EquipmentEffectComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Player/PlayerSpawnComponent.h"
#include "Definition/Item/ItemDefinition.h"
#include "Definition/Player/ControllerInputDefinition.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "InputAction.h"
#include "Mode/PdPlayerState.h"
#include "UI/HUD/PdHUD.h"
#include "UI/Info/InfoWidget.h"
#include "Weapon/Bow.h"

// 훈련장 게임 월드에서 입력·UI·장비·전투가 실제로 이어지는지 본다. -game 프로세스에서 돈다.

namespace
{
	const FString TrainingRoomMap = TEXT("/Game/Map/LV_TrainingRoom");
	const TCHAR* BowPath = TEXT("/Game/Item/Weapon/LoyalBow/DA_LoyalBow.DA_LoyalBow");
	const TCHAR* GreatswordPath = TEXT("/Game/Item/Weapon/Greatsword/DA_Greatsword.DA_Greatsword");

	ACharacterBase* GetPlayerCharacter()
	{
		const APlayerController* Controller = PdIntegrationTest::GetPlayerController();
		return Controller ? Cast<ACharacterBase>(Controller->GetPawn()) : nullptr;
	}

	UEquipmentComponent* GetPlayerEquipment()
	{
		const ACharacterBase* Player = GetPlayerCharacter();
		return Player ? Player->GetEquipmentComponent() : nullptr;
	}

	APdHUD* GetHud()
	{
		const APlayerController* Controller = PdIntegrationTest::GetPlayerController();
		return Controller ? Cast<APdHUD>(Controller->GetHUD()) : nullptr;
	}

	AEnemyBase* GetTrainingBot()
	{
		UWorld* World = PdIntegrationTest::GetGameWorld();
		TActorIterator<AEnemyBase> It(World);
		return World && It ? *It : nullptr;
	}

	/** 봇이 움직이거나 공격하지 않게 AI를 멈추고 공격도 끈다. AI가 나중에 다시 시작해도 공격하지 않는다. */
	void StopTrainingBot()
	{
		AEnemyBase* Bot = GetTrainingBot();
		AAIController* BotController = Bot ? Cast<AAIController>(Bot->GetController()) : nullptr;
		if (BotController && BotController->GetBrainComponent())
		{
			BotController->GetBrainComponent()->StopLogic(TEXT("Integration test"));
			BotController->StopMovement();
		}
		if (Bot)
		{
			Bot->SetAttackEnabled(false);
		}
	}

	/** 멈춰 둔 봇에게 공격을 한 번 시킨다. */
	void CommandBotAttack(AEnemyBase& Bot)
	{
		Bot.SetAttackEnabled(true);
		Bot.Attack();
	}

	void EquipWeapon(const TCHAR* DefinitionPath)
	{
		if (UEquipmentComponent* Equipment = GetPlayerEquipment())
		{
			Equipment->EquipWeaponDefinition(LoadObject<UItemDefinition>(nullptr, DefinitionPath));
		}
	}

	bool IsWeaponEquipped(const TCHAR* DefinitionPath)
	{
		const UEquipmentComponent* Equipment = GetPlayerEquipment();
		const UItemDefinition* Expected = DefinitionPath ? LoadObject<UItemDefinition>(nullptr, DefinitionPath) : nullptr;
		return Equipment && Equipment->GetCurrentWeaponDefinition() == Expected
			&& (Expected == nullptr) == (Equipment->GetCurrentWeaponActor() == nullptr);
	}

	void EquipAndWait(FAutomationTestBase* Test, const TCHAR* DefinitionPath)
	{
		PdIntegrationTest::Step(0.f, [DefinitionPath]() { EquipWeapon(DefinitionPath); });
		PdIntegrationTest::WaitUntil(Test, FString::Printf(TEXT("%s 장착"), DefinitionPath), 10.f,
			[DefinitionPath]() { return IsWeaponEquipped(DefinitionPath); });
	}

	/** 장비 효과 컴포넌트가 계산한 보너스 합계, ASC에 걸린 능력치, ASC에 걸린 장비 능력치 효과 개수. */
	struct FEquipmentState
	{
		TMap<FGameplayTag, float> BonusStats;
		TMap<FString, float> Attributes;
		int32 EquipmentEffects = 0;
	};

	/** 체력·자원처럼 시간에 따라 바뀌는 값을 뺀, 장비가 정하는 능력치. */
	TMap<FString, float> CaptureEquipmentDrivenAttributes()
	{
		static const TSet<FString> ChangingValues = {
			TEXT("Health"), TEXT("Shield"), TEXT("Mana"), TEXT("Stamina"), TEXT("Experience"), TEXT("IncomingDamage"), TEXT("OutgoingDamage")};
		TMap<FString, float> Values;
		const ACharacterBase* Player = GetPlayerCharacter();
		UPdAbilitySystemComponent* AbilitySystem = Player ? Player->GetPdAbilitySystemComponent() : nullptr;
		if (!AbilitySystem)
		{
			return Values;
		}
		TArray<FGameplayAttribute> Attributes;
		AbilitySystem->GetAllAttributes(Attributes);
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			if (!ChangingValues.Contains(Attribute.GetName()))
			{
				Values.Add(Attribute.GetName(), AbilitySystem->GetNumericAttribute(Attribute));
			}
		}
		return Values;
	}

	FEquipmentState CaptureEquipmentState()
	{
		FEquipmentState State;
		const ACharacterBase* Player = GetPlayerCharacter();
		if (const UEquipmentEffectComponent* Effects = Player ? Player->GetEquipmentEffectComponent() : nullptr)
		{
			Effects->GetEquipmentBonusStatMagnitudes(State.BonusStats);
		}
		if (const UPdAbilitySystemComponent* AbilitySystem = Player ? Player->GetPdAbilitySystemComponent() : nullptr)
		{
			// 쿨다운이나 맵 진입 뒤에 붙는 다른 효과는 재는 순간에 따라 달라지므로, 장비 능력치 효과만 센다.
			for (const FActiveGameplayEffectHandle& Handle : AbilitySystem->GetActiveEffects(FGameplayEffectQuery()))
			{
				const FActiveGameplayEffect* Effect = AbilitySystem->GetActiveGameplayEffect(Handle);
				State.EquipmentEffects += Effect && Effect->Spec.Def && Effect->Spec.Def->IsA<UEquipmentStatsEffect>() ? 1 : 0;
			}
		}
		State.Attributes = CaptureEquipmentDrivenAttributes();
		return State;
	}

	void ExpectSameEquipmentState(FAutomationTestBase* Test, const FString& What, const FEquipmentState& Expected, const FEquipmentState& Actual)
	{
		Test->TestTrue(What + TEXT(": 장비 보너스 합계가 같다"), Actual.BonusStats.OrderIndependentCompareEqual(Expected.BonusStats));
		Test->TestEqual(What + TEXT(": 장비 효과 개수가 같다(쌓이거나 빠지지 않음)"), Actual.EquipmentEffects, Expected.EquipmentEffects);
		Test->TestEqual(What + TEXT(": 능력치 개수"), Actual.Attributes.Num(), Expected.Attributes.Num());
		for (const TPair<FString, float>& Pair : Expected.Attributes)
		{
			const float* Value = Actual.Attributes.Find(Pair.Key);
			Test->TestTrue(FString::Printf(TEXT("%s: %s %.2f"), *What, *Pair.Key, Pair.Value), Value && FMath::IsNearlyEqual(*Value, Pair.Value, 0.01f));
		}
	}

	UInputAction* GetInfoInputAction(UControllerInputDefinition& Definition, const EInfoUiSection Section)
	{
		switch (Section)
		{
		case EInfoUiSection::Profile: return Definition.GetOpenInfoProfileInputAction().Get();
		case EInfoUiSection::Item: return Definition.GetOpenInfoItemInputAction().Get();
		case EInfoUiSection::Skin: return Definition.GetOpenInfoSkinInputAction().Get();
		case EInfoUiSection::Pandora: return Definition.GetOpenInfoPandoraInputAction().Get();
		case EInfoUiSection::Map: return Definition.GetOpenInfoMapInputAction().Get();
		}
		return nullptr;
	}

	/** 지금 입력 매핑에서 Action에 연결된 첫 키. 키 설정을 바꿔도 테스트가 따라간다. */
	FKey FindMappedKey(const UInputAction* Action)
	{
		const APlayerController* Controller = PdIntegrationTest::GetPlayerController();
		const UEnhancedInputLocalPlayerSubsystem* Input = Controller && Controller->GetLocalPlayer()
			? Controller->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		const TArray<FKey> Keys = Input && Action ? Input->QueryKeysMappedToAction(Action) : TArray<FKey>();
		return Keys.IsEmpty() ? EKeys::Invalid : Keys[0];
	}

	/** 지금 든 무기로 공격자 쪽 피해를 정한다(치명타 0일 때 비교용). */
	float BuildCurrentWeaponDamage()
	{
		const ACharacterBase* Player = GetPlayerCharacter();
		const UEquipmentComponent* Equipment = GetPlayerEquipment();
		const UCombatComponent* Combat = Player ? Player->GetCombatComponent() : nullptr;
		PdDamageRules::FOutgoingDamage Damage;
		return Combat && Equipment && Equipment->GetCurrentWeaponActor()
			&& Combat->BuildWeaponDamage(*Equipment->GetCurrentWeaponActor(), Damage) ? Damage.Damage : 0.f;
	}

	float GetHealthAndShield(const ACharacterBase* Character)
	{
		const UPdAbilitySystemComponent* Abilities = Character ? Character->GetPdAbilitySystemComponent() : nullptr;
		return Abilities
			? Abilities->GetNumericAttribute(UBasicAttributeSet::GetHealthAttribute()) + Abilities->GetNumericAttribute(UBasicAttributeSet::GetShieldAttribute())
			: -1.f;
	}

	/** Source의 전투 컴포넌트로 Target에게 큰 피해를 같은 프레임에 두 번 준다. 사망이 한 번만 처리되는지 볼 때 쓴다. */
	void ApplyLethalDamageTwice(ACharacterBase& Source, ACharacterBase& Target)
	{
		PdDamageRules::FOutgoingDamage Lethal;
		Lethal.Damage = 100000.f;
		Lethal.bAllowHitReact = false;
		if (UCombatComponent* Combat = Source.GetCombatComponent())
		{
			Combat->ApplyOutgoingDamageToTarget(&Target, Lethal, &Source, &Source);
			Combat->ApplyOutgoingDamageToTarget(&Target, Lethal, &Source, &Source);
		}
	}

	/** 훈련 봇 앞에 플레이어를 세우고 서로 바라보게 한다. */
	void PlaceFacingEachOther()
	{
		ACharacterBase* Player = GetPlayerCharacter();
		AEnemyBase* Bot = GetTrainingBot();
		if (!Player || !Bot)
		{
			return;
		}
		Bot->SetActorLocation(FVector(930.f, 640.f, 218.15f), false, nullptr, ETeleportType::TeleportPhysics);
		Player->SetActorLocation(FVector(930.f, 740.f, 218.15f), false, nullptr, ETeleportType::TeleportPhysics);
		Player->SetActorRotation((Bot->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D().Rotation());
		Bot->SetActorRotation((Player->GetActorLocation() - Bot->GetActorLocation()).GetSafeNormal2D().Rotation());
	}

	int32 GetPlayerDeathCount()
	{
		const APlayerController* Controller = PdIntegrationTest::GetPlayerController();
		const APdPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APdPlayerState>() : nullptr;
		const UPlayerMatchComponent* Match = PlayerState ? PlayerState->GetPlayerMatchComponent() : nullptr;
		return Match ? Match->GetDeathCount() : -1;
	}

	UControllerInputDefinition* GetInputDefinition()
	{
		APlayerController* Controller = PdIntegrationTest::GetPlayerController();
		UControllerInputComponent* Input = Controller ? Controller->FindComponentByClass<UControllerInputComponent>() : nullptr;
		return Input ? Input->GetLoadedInputDefinition() : nullptr;
	}

	void TapMappedKey(FAutomationTestBase* Test, const FString& What, TFunction<UInputAction*()> ResolveAction)
	{
		TSharedRef<FKey> Key = MakeShared<FKey>(EKeys::Invalid);
		PdIntegrationTest::Step(0.2f, [Test, What, Key, ResolveAction]()
		{
			*Key = FindMappedKey(ResolveAction());
			Test->TestTrue(What + TEXT(" 키가 입력 매핑에 있다"), Key->IsValid());
			PdIntegrationTest::PressKey(*Key, true);
		});
		PdIntegrationTest::Step(0.1f, [Key]() { PdIntegrationTest::PressKey(*Key, false); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdInfoShortcutsTest, "LabProject.Integration.TrainingRoom.UiShortcuts",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 정보창 다섯 탭의 단축키와 ESC가 입력 매핑 → 입력 컴포넌트 → HUD → 화면 열기까지 이어진다.
bool FPdInfoShortcutsTest::RunTest(const FString& Parameters)
{
	PdIntegrationTest::OpenMap(this, TrainingRoomMap);
	PdIntegrationTest::Step(0.f, []() { StopTrainingBot(); });
	for (const EInfoUiSection Section : {EInfoUiSection::Profile, EInfoUiSection::Item, EInfoUiSection::Skin, EInfoUiSection::Pandora,
		EInfoUiSection::Map})
	{
		const FString What = FString::Printf(TEXT("정보창 %d번 탭"), static_cast<int32>(Section));
		TapMappedKey(this, What, [Section]()
		{
			UControllerInputDefinition* Definition = GetInputDefinition();
			return Definition ? GetInfoInputAction(*Definition, Section) : nullptr;
		});
		// 훈련장은 지도 탭을 끈 맵이라, 지도 단축키는 정보창이 열리는지만 본다.
		PdIntegrationTest::WaitUntil(this, What + TEXT(" 열기"), 5.f, [Section]()
		{
			const APdHUD* Hud = GetHud();
			const UInfoWidget* Info = Hud ? Hud->GetInfoWidget() : nullptr;
			return Info && Info->IsVisible() && (Section == EInfoUiSection::Map || Info->GetFocusedSection() == Section);
		});
		PdIntegrationTest::Step(0.2f, []()
		{
			if (APdHUD* Hud = GetHud())
			{
				Hud->CloseInfoUi();
			}
		});
		PdIntegrationTest::WaitUntil(this, What + TEXT(" 닫기"), 5.f, []()
		{
			const APdHUD* Hud = GetHud();
			return Hud && !Hud->GetInfoWidget();
		});
	}

	TapMappedKey(this, TEXT("ESC 메뉴"), []()
	{
		UControllerInputDefinition* Definition = GetInputDefinition();
		return Definition ? Definition->GetEscapeInputAction().Get() : nullptr;
	});
	PdIntegrationTest::WaitUntil(this, TEXT("ESC 메뉴 열기"), 5.f, []()
	{
		const APdHUD* Hud = GetHud();
		return Hud && !Hud->GetInfoWidget() && Hud->IsGameplayInputBlockedByUi();
	});
	PdIntegrationTest::Step(0.2f, []()
	{
		if (APdHUD* Hud = GetHud())
		{
			Hud->ToggleEscapeMenu();
		}
	});
	PdIntegrationTest::WaitUntil(this, TEXT("ESC 메뉴 닫기"), 5.f, []()
	{
		const APdHUD* Hud = GetHud();
		return Hud && !Hud->IsGameplayInputBlockedByUi();
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdWeaponSwapAttributesTest, "LabProject.Integration.TrainingRoom.WeaponSwapAttributes",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 무기를 바꾸고 해제하고 다시 들어도 장비 효과가 쌓이거나 빠지지 않는다(장비 → 장비 효과 → ASC).
bool FPdWeaponSwapAttributesTest::RunTest(const FString& Parameters)
{
	struct FSnapshots
	{
		FEquipmentState Bow;
		FEquipmentState Greatsword;
	};
	TSharedRef<FSnapshots> Snapshots = MakeShared<FSnapshots>();

	PdIntegrationTest::OpenMap(this, TrainingRoomMap);
	PdIntegrationTest::Step(0.f, []() { StopTrainingBot(); });
	EquipAndWait(this, BowPath);
	PdIntegrationTest::Step(1.f, [Snapshots]() { Snapshots->Bow = CaptureEquipmentState(); });
	EquipAndWait(this, GreatswordPath);
	PdIntegrationTest::Step(1.f, [this, Snapshots]()
	{
		Snapshots->Greatsword = CaptureEquipmentState();
		TestFalse(TEXT("무기를 바꾸면 장비 보너스 합계가 바뀐다"), Snapshots->Greatsword.BonusStats.OrderIndependentCompareEqual(Snapshots->Bow.BonusStats));
	});
	EquipAndWait(this, BowPath);
	PdIntegrationTest::Step(1.f, [this, Snapshots]()
	{
		ExpectSameEquipmentState(this, TEXT("대검 → 활"), Snapshots->Bow, CaptureEquipmentState());
		if (UEquipmentComponent* Equipment = GetPlayerEquipment())
		{
			Equipment->UnequipCurrentWeapon();
		}
	});
	PdIntegrationTest::WaitUntil(this, TEXT("무기 해제"), 10.f, []() { return IsWeaponEquipped(nullptr); });
	EquipAndWait(this, GreatswordPath);
	PdIntegrationTest::Step(1.f, [this, Snapshots]()
	{
		ExpectSameEquipmentState(this, TEXT("해제 → 대검"), Snapshots->Greatsword, CaptureEquipmentState());
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdArrowDamageSnapshotTest, "LabProject.Integration.TrainingRoom.ArrowDamageSnapshot",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 화살은 발사할 때 정한 피해를 준다. 날아가는 동안 무기를 해제하거나 대검으로 바꿔도 피해가 같다.
bool FPdArrowDamageSnapshotTest::RunTest(const FString& Parameters)
{
	struct FHit
	{
		float Damage = 0.f;
		const UItemDefinition* WeaponAtImpact = nullptr;
	};
	struct FShots
	{
		TArray<FHit> Hits;
		int32 Fired = 0;
		float BowOutgoing = 0.f;
		float GreatswordOutgoing = 0.f;
	};
	TSharedRef<FShots> Shots = MakeShared<FShots>();

	// 쏜 같은 프레임에 무기를 바꾼다. 활이 쏠 수 있는 상태(발사 간격·착지)가 될 때까지 다시 시도한다.
	auto FireWhenReady = [this, Shots](const int32 ShotNumber, TFunction<void(UEquipmentComponent&)> AfterLaunch)
	{
		PdIntegrationTest::WaitUntil(this, FString::Printf(TEXT("%d번째 화살 발사"), ShotNumber), 5.f, [Shots, AfterLaunch]()
		{
			ACharacterBase* Player = GetPlayerCharacter();
			AEnemyBase* Bot = GetTrainingBot();
			UEquipmentComponent* Equipment = GetPlayerEquipment();
			ABow* Bow = Equipment ? Cast<ABow>(Equipment->GetCurrentWeaponActor()) : nullptr;
			if (!Player || !Bot || !Bow || !Bow->HandleAIPrimaryAttack(Player, Bot))
			{
				return false;
			}
			++Shots->Fired;
			AfterLaunch(*Equipment);
			return true;
		});
	};

	PdIntegrationTest::OpenMap(this, TrainingRoomMap);
	PdIntegrationTest::Step(0.f, [this, Shots]()
	{
		StopTrainingBot();
		const ACharacterBase* Player = GetPlayerCharacter();
		UPdAbilitySystemComponent* PlayerAbilities = Player ? Player->GetPdAbilitySystemComponent() : nullptr;
		AEnemyBase* Bot = GetTrainingBot();
		UPdAbilitySystemComponent* BotAbilities = Bot ? Bot->GetPdAbilitySystemComponent() : nullptr;
		if (!TestNotNull(TEXT("플레이어 ASC"), PlayerAbilities) || !TestNotNull(TEXT("훈련 봇 ASC"), BotAbilities))
		{
			return;
		}
		// 치명타 확률을 0으로 둬 세 발의 피해를 같은 값으로 비교한다.
		PlayerAbilities->SetNumericAttributeBase(UBasicAttributeSet::GetCriticalAttribute(), 0.f);
		for (const FGameplayAttribute& Attribute : {UBasicAttributeSet::GetHealthAttribute(), UBasicAttributeSet::GetShieldAttribute()})
		{
			BotAbilities->GetGameplayAttributeValueChangeDelegate(Attribute).AddLambda([Shots](const FOnAttributeChangeData& Data)
			{
				const UEquipmentComponent* Equipment = GetPlayerEquipment();
				if (Data.NewValue < Data.OldValue)
				{
					const bool bHoldsWeapon = Equipment && Equipment->GetCurrentWeaponActor();
					Shots->Hits.Add({Data.OldValue - Data.NewValue, bHoldsWeapon ? Equipment->GetCurrentWeaponDefinition() : nullptr});
				}
			});
		}
	});
	// 대검을 한 번 들어 표현 애셋을 불러 둬야 쏜 프레임 안에 바로 바뀐다.
	EquipAndWait(this, GreatswordPath);
	PdIntegrationTest::Step(0.5f, [Shots]() { Shots->GreatswordOutgoing = BuildCurrentWeaponDamage(); });
	// 화살이 맞은 뒤에 다음 단계로 넘어가야, 맞는 순간의 무기가 테스트가 바꾼 무기 그대로다.
	auto WaitForHit = [this, Shots](const int32 HitCount)
	{
		PdIntegrationTest::WaitUntil(this, FString::Printf(TEXT("%d번째 화살 명중"), HitCount), 5.f,
			[Shots, HitCount]() { return Shots->Hits.Num() >= HitCount; });
	};
	EquipAndWait(this, BowPath);
	// 화살이 몇 프레임 날아가도록 거리를 고정하고, 두 캐릭터가 바닥에 선 뒤에 쏜다.
	PdIntegrationTest::Step(0.f, []()
	{
		AEnemyBase* Bot = GetTrainingBot();
		ACharacterBase* Player = GetPlayerCharacter();
		if (Bot && Player)
		{
			Bot->SetActorLocation(FVector(930.f, 640.f, 218.15f), false, nullptr, ETeleportType::TeleportPhysics);
			Player->SetActorLocation(FVector(912.f, 917.f, 218.15f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	});
	PdIntegrationTest::WaitUntil(this, TEXT("두 캐릭터 착지"), 5.f, []()
	{
		const ACharacterBase* Player = GetPlayerCharacter();
		const AEnemyBase* Bot = GetTrainingBot();
		return Player && Bot && !Player->GetCharacterMovement()->IsFalling() && !Bot->GetCharacterMovement()->IsFalling();
	});
	PdIntegrationTest::Step(0.5f, [Shots]() { Shots->BowOutgoing = BuildCurrentWeaponDamage(); });
	FireWhenReady(1, [](UEquipmentComponent&) {});
	WaitForHit(1);
	FireWhenReady(2, [](UEquipmentComponent& Equipment) { Equipment.UnequipCurrentWeapon(); });
	WaitForHit(2);
	EquipAndWait(this, BowPath);
	FireWhenReady(3, [](UEquipmentComponent& Equipment)
	{
		Equipment.EquipWeaponDefinition(LoadObject<UItemDefinition>(nullptr, GreatswordPath));
	});
	WaitForHit(3);
	PdIntegrationTest::Step(0.5f, [this, Shots]()
	{
		TestEqual(TEXT("세 발 모두 쐈다"), Shots->Fired, 3);
		if (!TestEqual(TEXT("세 발 모두 봇에 피해를 줬다"), Shots->Hits.Num(), 3))
		{
			return;
		}
		const float BowDamage = Shots->Hits[0].Damage;
		TestTrue(TEXT("활 피해가 있다"), BowDamage > 0.f);
		TestEqual(TEXT("해제한 뒤 맞은 화살도 활 피해"), Shots->Hits[1].Damage, BowDamage);
		TestNull(TEXT("두 번째 화살이 맞을 때 무기가 없었다"), Shots->Hits[1].WeaponAtImpact);
		TestEqual(TEXT("대검으로 바꾼 뒤 맞은 화살도 활 피해"), Shots->Hits[2].Damage, BowDamage);
		TestTrue(TEXT("세 번째 화살이 맞을 때 대검을 들고 있었다"),
			Shots->Hits[2].WeaponAtImpact == LoadObject<UItemDefinition>(nullptr, GreatswordPath));
		TestNotEqual(TEXT("대검이 내는 피해는 활과 달라 이 비교가 의미 있다"), Shots->GreatswordOutgoing, Shots->BowOutgoing);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdPlayerRespawnRebindTest, "LabProject.Integration.TrainingRoom.PlayerRespawnRebind",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 같은 프레임에 치명상을 두 번 받아도 사망·리스폰은 한 번이고, 리스폰 뒤 장비 효과가 남거나 쌓이지 않으며 다시 공격할 수 있다.
// ASC는 PlayerState에 남고 캐릭터 쪽 컴포넌트가 다시 연결되는 경로를 본다.
bool FPdPlayerRespawnRebindTest::RunTest(const FString& Parameters)
{
	struct FRespawnState
	{
		FEquipmentState BeforeDeath;
		int32 Respawns = 0;
		int32 DeathsBefore = 0;
		float BotHealthBefore = 0.f;
	};
	TSharedRef<FRespawnState> State = MakeShared<FRespawnState>();

	PdIntegrationTest::OpenMap(this, TrainingRoomMap);
	PdIntegrationTest::Step(0.f, []() { StopTrainingBot(); });
	EquipAndWait(this, GreatswordPath);
	PdIntegrationTest::Step(1.f, [this, State]()
	{
		State->BeforeDeath = CaptureEquipmentState();
		AGameModeBase* GameMode = UGameplayStatics::GetGameMode(PdIntegrationTest::GetGameWorld());
		UPlayerSpawnComponent* Spawn = GameMode ? GameMode->FindComponentByClass<UPlayerSpawnComponent>() : nullptr;
		if (!TestNotNull(TEXT("리스폰 컴포넌트"), Spawn))
		{
			return;
		}
		Spawn->OnPlayerRespawned.AddLambda([State](APlayerController*, bool) { ++State->Respawns; });

		ACharacterBase* Player = GetPlayerCharacter();
		AEnemyBase* Bot = GetTrainingBot();
		if (Player && Bot)
		{
			TestFalse(TEXT("치명상 전에 플레이어는 살아 있다"), Player->IsDead() || GetHealthAndShield(Player) <= 0.f);
			State->DeathsBefore = GetPlayerDeathCount();
			ApplyLethalDamageTwice(*Bot, *Player);
			TestTrue(TEXT("치명상을 받은 플레이어는 죽었다"), Player->IsDead());
			// 훈련장은 리스폰할 때 사망 수를 다시 0으로 돌리므로, 기록은 사망한 그 자리에서 본다.
			TestEqual(TEXT("치명상 두 번에도 사망은 한 번만 기록된다"), GetPlayerDeathCount() - State->DeathsBefore, 1);
		}
	});
	PdIntegrationTest::WaitUntil(this, TEXT("플레이어 리스폰"), 20.f, [State]()
	{
		const ACharacterBase* Player = GetPlayerCharacter();
		return State->Respawns > 0 && Player && !Player->IsDead() && GetHealthAndShield(Player) > 0.f;
	});
	PdIntegrationTest::Step(1.f, [this, State]()
	{
		TestEqual(TEXT("리스폰은 한 번만 일어난다"), State->Respawns, 1);
	});
	EquipAndWait(this, GreatswordPath);
	PdIntegrationTest::Step(1.f, [this, State]()
	{
		ExpectSameEquipmentState(this, TEXT("리스폰 뒤 대검"), State->BeforeDeath, CaptureEquipmentState());
		PlaceFacingEachOther();
		State->BotHealthBefore = GetHealthAndShield(GetTrainingBot());
		if (UCombatComponent* Combat = GetPlayerCharacter() ? GetPlayerCharacter()->GetCombatComponent() : nullptr)
		{
			Combat->StartPrimaryAttack();
		}
	});
	PdIntegrationTest::Step(0.3f, []()
	{
		if (UCombatComponent* Combat = GetPlayerCharacter() ? GetPlayerCharacter()->GetCombatComponent() : nullptr)
		{
			Combat->StopPrimaryAttack();
		}
	});
	PdIntegrationTest::WaitUntil(this, TEXT("리스폰 뒤 공격이 봇에 피해를 준다"), 5.f,
		[State]() { return GetHealthAndShield(GetTrainingBot()) < State->BotHealthBefore; });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdAttackInterruptedByDeathTest, "LabProject.Integration.TrainingRoom.AttackInterruptedByDeath",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

// 공격을 시작한 봇이 같은 프레임에 치명상을 두 번 받으면 공격이 바로 끊기고, 끊긴 공격은 피해를 주지 않는다.
// 다시 태어난 봇은 다시 공격할 수 있다(판정·타이머가 남지 않고 공격 상태가 처음으로 돌아감).
bool FPdAttackInterruptedByDeathTest::RunTest(const FString& Parameters)
{
	struct FAttackState
	{
		float PlayerHealthBefore = 0.f;
	};
	TSharedRef<FAttackState> State = MakeShared<FAttackState>();

	PdIntegrationTest::OpenMap(this, TrainingRoomMap);
	PdIntegrationTest::Step(0.f, []()
	{
		StopTrainingBot();
		PlaceFacingEachOther();
	});
	PdIntegrationTest::Step(0.5f, [this, State]()
	{
		ACharacterBase* Player = GetPlayerCharacter();
		AEnemyBase* Bot = GetTrainingBot();
		if (!TestNotNull(TEXT("플레이어"), Player) || !TestNotNull(TEXT("훈련 봇"), Bot))
		{
			return;
		}
		State->PlayerHealthBefore = GetHealthAndShield(Player);
		CommandBotAttack(*Bot);
		TestTrue(TEXT("봇이 공격을 시작했다"), Bot->IsAttackInProgress());
		ApplyLethalDamageTwice(*Player, *Bot);
		TestTrue(TEXT("치명상을 받은 봇은 죽었다"), Bot->IsDead());
		TestFalse(TEXT("죽은 봇의 공격은 바로 끊긴다"), Bot->IsAttackInProgress());
	});
	PdIntegrationTest::Step(2.f, [this, State]()
	{
		TestEqual(TEXT("끊긴 공격은 플레이어에게 피해를 주지 않는다"), GetHealthAndShield(GetPlayerCharacter()), State->PlayerHealthBefore);
	});
	PdIntegrationTest::WaitUntil(this, TEXT("훈련 봇 리스폰"), 20.f, []()
	{
		const AEnemyBase* Bot = GetTrainingBot();
		return Bot && !Bot->IsDead() && GetHealthAndShield(Bot) > 0.f;
	});
	PdIntegrationTest::Step(1.f, [State]()
	{
		StopTrainingBot();
		PlaceFacingEachOther();
		State->PlayerHealthBefore = GetHealthAndShield(GetPlayerCharacter());
		if (AEnemyBase* Bot = GetTrainingBot())
		{
			CommandBotAttack(*Bot);
		}
	});
	PdIntegrationTest::WaitUntil(this, TEXT("리스폰한 봇의 공격이 플레이어에게 피해를 준다"), 5.f,
		[State]() { return GetHealthAndShield(GetPlayerCharacter()) < State->PlayerHealthBefore; });
	return true;
}

#endif
