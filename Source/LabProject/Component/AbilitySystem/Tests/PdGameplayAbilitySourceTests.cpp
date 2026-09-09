#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Ability/PdGameplayAbility.h"

#include "AbilitySystem/Ability/ProjectileAbility.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/Pandora/PandoraComponent.h"
#include "Definition/Pandora/PandoraDefinition.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Mode/PdPlayerState.h"
#include "Pandora/PandoraSkillSource.h"

// 판도라를 교체해도 이미 부여된 능력의 스킬 출처는 새 선택으로 바뀌면 안 된다.
// 출처 조회와 입력 해제만 검사하며, 실제 능력 실행·쿨다운 UI·네트워크 전달은 검사하지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAbilitySourceSelectionTest,
	"LabProject.AbilitySystem.Source.SelectionDoesNotChangeGrantedSkill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAbilitySourceSelectionTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Temporary world"), World))
	{
		return false;
	}
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	APdPlayerState* PlayerState = World->SpawnActor<APdPlayerState>();
	if (!TestNotNull(TEXT("PlayerState"), PlayerState))
	{
		return false;
	}
	UPdAbilitySystemComponent* ASC = Cast<UPdAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent());
	UPandoraComponent* Pandora = PlayerState->GetPandoraComponent();
	if (!TestNotNull(TEXT("ASC"), ASC) || !TestNotNull(TEXT("Pandora component"), Pandora))
	{
		return false;
	}
	ASC->InitAbilityActorInfo(PlayerState, PlayerState);

	// 준비: 판도라 두 개를 지급하고 첫 번째 판도라의 스킬 출처로 능력을 부여한다.
	UPandoraDefinition* FirstPandora = NewObject<UPandoraDefinition>();
	UPandoraDefinition* SecondPandora = NewObject<UPandoraDefinition>();
	FirstPandora->Skill.SetNum(1);
	SecondPandora->Skill.SetNum(1);
	USkillDefinition* FirstSkill = NewObject<USkillDefinition>();
	USkillDefinition* SecondSkill = NewObject<USkillDefinition>();
	FirstPandora->Skill[0].SkillDefinition = FirstSkill;
	SecondPandora->Skill[0].SkillDefinition = SecondSkill;
	if (!TestTrue(TEXT("First Pandora is owned"), Pandora->GrantPandoraDefinition(FirstPandora))
		|| !TestTrue(TEXT("Second Pandora is owned"), Pandora->GrantPandoraDefinition(SecondPandora))
		|| !TestTrue(TEXT("First Pandora can be selected"), Pandora->RequestPandoraSelection(FirstPandora)))
	{
		return false;
	}
	TestTrue(TEXT("First Pandora is selected before the change"), Pandora->GetCurrentPandoraDefinition() == FirstPandora);

	UPandoraSkillSource* Source = NewObject<UPandoraSkillSource>(ASC);
	Source->Initialize(FirstPandora, FirstSkill, 0, 1, EEnum_Direction::Left);
	FGameplayAbilitySpec GrantSpec(UProjectileAbility::StaticClass(), 1, INDEX_NONE, Source);
	GrantSpec.GetDynamicSpecSourceTags().AddTag(LabGameplayTags::Input_Ability_Skill1);
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(GrantSpec);
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	if (!TestNotNull(TEXT("Granted ability"), Spec))
	{
		return false;
	}
	UPdGameplayAbility* Ability = Cast<UPdGameplayAbility>(Spec->GetPrimaryInstance());
	if (!TestNotNull(TEXT("Ability instance"), Ability))
	{
		return false;
	}
	TestTrue(TEXT("Ability starts with the first skill"), Ability->GetSourceSkillDataAsset() == FirstSkill);

	// 실행: 내부 프로퍼티를 강제로 바꾸지 않고 실제 선택 요청으로 두 번째 판도라를 선택한다.
	if (!TestTrue(TEXT("Second Pandora can be selected"), Pandora->RequestPandoraSelection(SecondPandora)))
	{
		return false;
	}
	Spec = ASC->FindAbilitySpecFromHandle(Handle);
	if (!TestNotNull(TEXT("Previously granted ability is retained"), Spec))
	{
		return false;
	}

	// 확인: 새 선택은 반영되지만 기존 능력의 출처는 유지되고, 새 입력만 받지 않게 된다.
	TestTrue(TEXT("Selection changes to the second Pandora"), Pandora->GetCurrentPandoraDefinition() == SecondPandora);
	TestFalse(TEXT("Previous ability loses the skill input binding"), Spec->GetDynamicSpecSourceTags().HasTagExact(LabGameplayTags::Input_Ability_Skill1));
	TestTrue(TEXT("Previous ability retains its source context"), Ability->GetPandoraSkillSource() == Source);
	TestTrue(TEXT("Previous ability still resolves the first skill"), Ability->GetSourceSkillDataAsset() == FirstSkill);
	TestTrue(TEXT("Source still identifies the first Pandora"), Source->GetPandoraDefinition() == FirstPandora);
	TestEqual(TEXT("Source direction remains unchanged"), Source->GetLoadoutDirection(), EEnum_Direction::Left);
	return true;
}

#endif
