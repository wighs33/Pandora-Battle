#if WITH_DEV_AUTOMATION_TESTS

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// 같은 능력을 반복 지급해도 하나만 남고, 회수와 변경 알림도 중복되지 않아야 한다.
// 로컬 ASC의 부여·회수 규칙만 검사하며, 능력 실행이나 네트워크 전달은 검사하지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPdAbilitySystemGrantTest,
	"LabProject.AbilitySystem.Component.GrantDeduplicationAndRemoval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPdAbilitySystemGrantTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Temporary world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("ASC owner"), Owner))
	{
		return false;
	}
	UPdAbilitySystemComponent* ASC = NewObject<UPdAbilitySystemComponent>(Owner);
	Owner->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	ASC->InitAbilityActorInfo(Owner, Owner);

	int32 ChangeCount = 0;
	const FDelegateHandle ChangeHandle = ASC->OnAbilitiesChangedNative.AddLambda([&ChangeCount]() { ++ChangeCount; });
	ON_SCOPE_EXIT { ASC->OnAbilitiesChangedNative.Remove(ChangeHandle); };
	const TArray<TSubclassOf<UGameplayAbility>> Classes = {nullptr, UGameplayAbility::StaticClass(), UGameplayAbility::StaticClass()};
	const TArray<FGameplayAbilitySpecHandle> Handles = ASC->GrantAbilities(Classes, 0, Owner);
	if (!TestEqual(TEXT("Invalid and duplicate classes are skipped"), Handles.Num(), 1))
	{
		return false;
	}
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handles[0]);
	if (!TestNotNull(TEXT("Granted spec"), Spec))
	{
		return false;
	}
	TestEqual(TEXT("Ability level retains the minimum of one"), Spec->Level, 1);
	TestTrue(TEXT("Source object is preserved"), Spec->SourceObject.Get() == Owner);
	TestEqual(TEXT("Giving an ability notifies listeners"), ChangeCount, 1);
	TestEqual(TEXT("Already granted classes are skipped on later calls"), ASC->GrantAbilities(Classes).Num(), 0);
	TestEqual(TEXT("Skipped grants do not notify listeners"), ChangeCount, 1);
	TestEqual(TEXT("Repeated grants leave exactly one ability"), ASC->GetActivatableAbilities().Num(), 1);
	TestFalse(TEXT("Granted but inactive ability is not reported as active"), ASC->HasActiveAbilityOfClass(UGameplayAbility::StaticClass()));
	ASC->RemoveAbilities(Handles);
	TestNull(TEXT("Granted ability can be removed"), ASC->FindAbilitySpecFromHandle(Handles[0]));
	TestEqual(TEXT("Removal notifies listeners"), ChangeCount, 2);
	ASC->RemoveAbilities(Handles);
	TestEqual(TEXT("Removing stale handles is harmless"), ChangeCount, 2);
	return true;
}

#endif
