#include "Tests/Integration/IntegrationTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "GameFramework/Actor.h"
#include "PreviewScene.h"

// 콘텐츠 로딩과 그 소비자의 연결. 에디터 프로세스에서 돈다(GameInstance가 없는 경우를 함께 본다).

namespace
{
	const TCHAR* LeaseAssetPath = TEXT("/Game/Data/DA_EnemyTrainingBot.DA_EnemyTrainingBot");
	const TCHAR* ReleasedLeaseAssetPath = TEXT("/Game/Data/DA_EnemyStickman.DA_EnemyStickman");

	struct FLeaseCallbackCounts
	{
		int32 Loaded = 0;
		int32 Released = 0;
		int32 Empty = 0;
		TSharedPtr<FContentLease> LoadedLease;
		TSharedPtr<FContentLease> EmptyLease;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdContentLeaseLifecycleTest, "LabProject.Integration.Content.LeaseLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// lease는 완료 콜백을 한 번만 보내고, 소유자가 먼저 놓은 lease는 늦게 끝난 로딩이 콜백을 부르지 않으며,
// 빈 요청도 호출자가 lease를 받은 다음에 완료를 알린다.
bool FPdContentLeaseLifecycleTest::RunTest(const FString& Parameters)
{
	TSharedRef<FLeaseCallbackCounts> Counts = MakeShared<FLeaseCallbackCounts>();
	Counts->LoadedLease = UContentDataSubsystem::AcquireContent({FSoftObjectPath(LeaseAssetPath)},
		FSimpleDelegate::CreateLambda([Counts]() { ++Counts->Loaded; }));
	TSharedPtr<FContentLease> ReleasedLease = UContentDataSubsystem::AcquireContent({FSoftObjectPath(ReleasedLeaseAssetPath)},
		FSimpleDelegate::CreateLambda([Counts]() { ++Counts->Released; }));
	ReleasedLease.Reset();
	Counts->EmptyLease = UContentDataSubsystem::AcquireContent({}, FSimpleDelegate::CreateLambda([Counts]() { ++Counts->Empty; }));
	TestEqual(TEXT("빈 요청의 완료는 lease를 돌려받기 전에 오지 않는다"), Counts->Empty, 0);

	PdIntegrationTest::WaitUntil(this, TEXT("lease 완료 콜백"), 15.f, [Counts]() { return Counts->Loaded > 0 && Counts->Empty > 0; });
	PdIntegrationTest::Step(0.5f, [this, Counts]()
	{
		TestTrue(TEXT("완료된 lease는 준비 상태다"), Counts->LoadedLease.IsValid() && Counts->LoadedLease->IsReady());
		TestNotNull(TEXT("lease가 붙잡은 애셋이 메모리에 있다"), FSoftObjectPath(LeaseAssetPath).ResolveObject());
		TestEqual(TEXT("완료 콜백은 한 번만 온다"), Counts->Loaded, 1);
		TestEqual(TEXT("빈 요청의 완료 콜백도 한 번만 온다"), Counts->Empty, 1);
		TestEqual(TEXT("먼저 놓은 lease는 콜백을 부르지 않는다"), Counts->Released, 0);
	});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPdPreviewWorldEnemyLoadTest, "LabProject.Integration.Content.EnemyInPreviewWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 블루프린트 뷰포트·썸네일처럼 GameInstance 없이 액터를 초기화하는 미리보기 월드에서도 적이 정의 애셋을 불러온다.
// 정의를 못 불러오면 적이 오류 로그를 남기고, 자동화 테스트는 오류 로그를 실패로 센다.
bool FPdPreviewWorldEnemyLoadTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FPreviewScene> Scene = MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues());
	UWorld* World = Scene->GetWorld();
	if (!TestNotNull(TEXT("미리보기 월드"), World))
	{
		return false;
	}
	TestNull(TEXT("미리보기 월드에는 GameInstance가 없다"), World->GetGameInstance());
	TestTrue(TEXT("미리보기 월드는 액터를 초기화한다"), World->AreActorsInitialized());

	for (const TCHAR* ClassPath : {TEXT("/Game/StackOBot/AI/BP_Bug.BP_Bug_C"), TEXT("/Game/Actor/Stickman/BP_Enemy.BP_Enemy_C"),
		TEXT("/Game/Actor/Stickman/BP_TrainingBot.BP_TrainingBot_C")})
	{
		UClass* EnemyClass = LoadClass<AActor>(nullptr, ClassPath);
		TestNotNull(FString::Printf(TEXT("%s 생성"), ClassPath), EnemyClass ? World->SpawnActor<AActor>(EnemyClass, FTransform::Identity) : nullptr);
	}

	// 정의 로딩은 비동기라, 완료 콜백이 돌 때까지 미리보기 월드를 살려 둔다.
	PdIntegrationTest::Step(3.f, [Scene]() {});
	return true;
}

#endif
