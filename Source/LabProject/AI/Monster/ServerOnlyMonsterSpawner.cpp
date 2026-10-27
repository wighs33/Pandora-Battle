#include "AI/Monster/ServerOnlyMonsterSpawner.h"

#include "AI/Monster/MonsterCharacter.h"
#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Definition/Character/EnemyBaseDefinition.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ServerOnlyMonsterSpawner)

DEFINE_LOG_CATEGORY_STATIC(LogServerOnlyMonsterSpawner, Log, All);

AServerOnlyMonsterSpawner::AServerOnlyMonsterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = false;
	bNetLoadOnClient = false;
	SetReplicateMovement(false);
}

void AServerOnlyMonsterSpawner::BeginPlay()
{
	// 네트워크 클라이언트가 로컬로 만든 비복제 액터도 ROLE_Authority를 보고하므로
	// HasAuthority()만으로는 이 경우를 가려낼 수 없다.
	if (GetNetMode() == NM_Client || !HasAuthority())
	{
		UE_LOG(LogServerOnlyMonsterSpawner, Error,
			TEXT("Destroying unauthorized client-side monster spawner '%s' before it can spawn a monster."),
			*GetPathName());
		Destroy();
		return;
	}

	Super::BeginPlay();
	if (MonsterClass)
	{
		SpawnMonster();
		return;
	}

	// 기본 몬스터 클래스 경로는 DA_EnemyBase를 읽어야 알 수 있다. 두 단계 모두 게임 스레드를 막지 않는다.
	DefaultMonsterClassLease = UContentDataSubsystem::AcquireContent({UEnemyBaseDefinition::GetDefaultDefinitionPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleEnemyDefinitionLoaded));
}

void AServerOnlyMonsterSpawner::HandleEnemyDefinitionLoaded()
{
	// 다음 lease가 정의를 넘겨받을 때까지 놓지 않는다.
	const TSharedPtr<FContentLease> DefinitionLease = MoveTemp(DefaultMonsterClassLease);
	const FSoftObjectPath DefinitionPath = UEnemyBaseDefinition::GetDefaultDefinitionPath();
	const UEnemyBaseDefinition* EnemyDefinition = Cast<UEnemyBaseDefinition>(DefinitionPath.ResolveObject());
	if (!EnemyDefinition)
	{
		UE_LOG(LogServerOnlyMonsterSpawner, Error, TEXT("Spawner '%s' could not load the default enemy definition."),
			*GetPathName());
		return;
	}

	DefaultMonsterClassLease = UContentDataSubsystem::AcquireContent(
		{DefinitionPath, EnemyDefinition->GetCombatSettings().DefaultMonsterClass.ToSoftObjectPath()},
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleDefaultMonsterClassLoaded));
}

void AServerOnlyMonsterSpawner::HandleDefaultMonsterClassLoaded()
{
	// MonsterClass가 클래스를 붙잡으므로 lease는 여기서 놓는다.
	const TSharedPtr<FContentLease> ClassLease = MoveTemp(DefaultMonsterClassLease);
	const UEnemyBaseDefinition* EnemyDefinition =
		Cast<UEnemyBaseDefinition>(UEnemyBaseDefinition::GetDefaultDefinitionPath().ResolveObject());
	MonsterClass = EnemyDefinition ? EnemyDefinition->GetCombatSettings().DefaultMonsterClass.Get() : nullptr;
	SpawnMonster();
}

void AServerOnlyMonsterSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	DefaultMonsterClassLease.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}

	CleanupSpawnedMonster();
	Super::EndPlay(EndPlayReason);
}

void AServerOnlyMonsterSpawner::SpawnMonster()
{
	if (bEndingPlay || GetNetMode() == NM_Client || !HasAuthority())
	{
		return;
	}

	if (IsValid(SpawnedMonster))
	{
		UE_LOG(LogServerOnlyMonsterSpawner, Warning,
			TEXT("Spawner '%s' ignored a duplicate spawn request while '%s' is still alive."),
			*GetPathName(),
			*SpawnedMonster->GetPathName());
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !MonsterClass)
	{
		UE_LOG(LogServerOnlyMonsterSpawner, Error,
			TEXT("Spawner '%s' cannot spawn because its world or Monster Class is invalid."),
			*GetPathName());
		return;
	}

	World->GetTimerManager().ClearTimer(RespawnTimerHandle);

	const FTransform SpawnTransform = GetActorTransform();
	AMonsterCharacter* NewMonster = World->SpawnActorDeferred<AMonsterCharacter>(MonsterClass, SpawnTransform, nullptr,
		nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn, ESpawnActorScaleMethod::MultiplyWithRoot);

	if (!NewMonster)
	{
		UE_LOG(LogServerOnlyMonsterSpawner, Error, TEXT("Spawner '%s' failed to create monster class '%s'."),
			*GetPathName(),
			*GetPathNameSafe(MonsterClass.Get()));
		ScheduleRespawn();
		return;
	}

	SpawnedMonster = NewMonster;
	NewMonster->OnDestroyed.AddUniqueDynamic(this, &AServerOnlyMonsterSpawner::HandleSpawnedMonsterDestroyed);

	if (!ApplyMonsterSpawnParameters(NewMonster))
	{
		UE_LOG(LogServerOnlyMonsterSpawner, Warning,
			TEXT("Monster class '%s' does not expose both expected leash properties; "
				"the monster will use its own defaults."),
			*GetPathNameSafe(MonsterClass.Get()));
	}

	UGameplayStatics::FinishSpawningActor(NewMonster, SpawnTransform, ESpawnActorScaleMethod::MultiplyWithRoot);
}

void AServerOnlyMonsterSpawner::ScheduleRespawn()
{
	if (bEndingPlay || GetNetMode() == NM_Client || !HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(RespawnTimerHandle);

	if (RespawnCooldown <= 0.0f)
	{
		RespawnTimerHandle = TimerManager.SetTimerForNextTick(this, &AServerOnlyMonsterSpawner::SpawnMonster);
		return;
	}

	TimerManager.SetTimer(RespawnTimerHandle, this, &AServerOnlyMonsterSpawner::SpawnMonster, RespawnCooldown, false);
}

void AServerOnlyMonsterSpawner::CleanupSpawnedMonster()
{
	AMonsterCharacter* MonsterToDestroy = SpawnedMonster.Get();
	SpawnedMonster = nullptr;

	if (!IsValid(MonsterToDestroy))
	{
		return;
	}

	MonsterToDestroy->OnDestroyed.RemoveAll(this);
	MonsterToDestroy->Destroy();
}

bool AServerOnlyMonsterSpawner::ApplyMonsterSpawnParameters(AMonsterCharacter* Monster) const
{
	return Monster->SetLeashDistances(MaxLeashDistanceFromSpawnPoint, MinLeashDistanceFromSpawnPointToResumeRoaming);
}

void AServerOnlyMonsterSpawner::HandleSpawnedMonsterDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor != SpawnedMonster)
	{
		return;
	}

	DestroyedActor->OnDestroyed.RemoveAll(this);
	SpawnedMonster = nullptr;
	ScheduleRespawn();
}
