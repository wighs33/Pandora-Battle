#include "AI/ServerOnlyMonsterSpawner.h"

#include "AI/MonsterCharacter.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ServerOnlyMonsterSpawner)

DEFINE_LOG_CATEGORY_STATIC(LogServerOnlyMonsterSpawner, Log, All);

namespace
{
	FString NormalizePropertyName(FString Name)
	{
		Name.ReplaceInline(TEXT(" "), TEXT(""));
		Name.ReplaceInline(TEXT("_"), TEXT(""));
		Name.ToLowerInline();
		return Name;
	}

	bool SetNumericProperty(UObject* Target, const TCHAR* RequestedName, const double Value)
	{
		if (!IsValid(Target))
		{
			return false;
		}

		const FString NormalizedRequestedName = NormalizePropertyName(RequestedName);
		for (TFieldIterator<FProperty> PropertyIt(Target->GetClass(), EFieldIterationFlags::IncludeSuper);
			 PropertyIt;
			 ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;
			const bool bNameMatches =
				NormalizePropertyName(Property->GetName()) == NormalizedRequestedName
				|| NormalizePropertyName(Property->GetAuthoredName()) == NormalizedRequestedName;

			if (!bNameMatches)
			{
				continue;
			}

			FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);
			if (!NumericProperty)
			{
				return false;
			}

			void* ValueAddress = Property->ContainerPtrToValuePtr<void>(Target);
			if (NumericProperty->IsFloatingPoint())
			{
				NumericProperty->SetFloatingPointPropertyValue(ValueAddress, Value);
				return true;
			}

			if (NumericProperty->IsInteger())
			{
				NumericProperty->SetIntPropertyValue(ValueAddress, FMath::RoundToInt64(Value));
				return true;
			}

			return false;
		}

		return false;
	}
}

AServerOnlyMonsterSpawner::AServerOnlyMonsterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = false;
	bNetLoadOnClient = false;
	SetReplicateMovement(false);

	static ConstructorHelpers::FClassFinder<AMonsterCharacter> DefaultMonsterClass(
		TEXT("/Game/StackOBot/AI/BP_Bug"));
	if (DefaultMonsterClass.Succeeded())
	{
		MonsterClass = DefaultMonsterClass.Class;
	}
}

void AServerOnlyMonsterSpawner::BeginPlay()
{
	// A non-replicated actor created locally on a network client reports
	// ROLE_Authority, so HasAuthority() alone cannot identify this case.
	if (GetNetMode() == NM_Client || !HasAuthority())
	{
		UE_LOG(
			LogServerOnlyMonsterSpawner,
			Error,
			TEXT("Destroying unauthorized client-side monster spawner '%s' before it can spawn a monster."),
			*GetPathName());
		Destroy();
		return;
	}

	Super::BeginPlay();
	SpawnMonster();
}

void AServerOnlyMonsterSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;

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
		UE_LOG(
			LogServerOnlyMonsterSpawner,
			Warning,
			TEXT("Spawner '%s' ignored a duplicate spawn request while '%s' is still alive."),
			*GetPathName(),
			*SpawnedMonster->GetPathName());
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !MonsterClass)
	{
		UE_LOG(
			LogServerOnlyMonsterSpawner,
			Error,
			TEXT("Spawner '%s' cannot spawn because its world or Monster Class is invalid."),
			*GetPathName());
		return;
	}

	World->GetTimerManager().ClearTimer(RespawnTimerHandle);

	const FTransform SpawnTransform = GetActorTransform();
	AMonsterCharacter* NewMonster = World->SpawnActorDeferred<AMonsterCharacter>(
		MonsterClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
		ESpawnActorScaleMethod::MultiplyWithRoot);

	if (!NewMonster)
	{
		UE_LOG(
			LogServerOnlyMonsterSpawner,
			Error,
			TEXT("Spawner '%s' failed to create monster class '%s'."),
			*GetPathName(),
			*GetPathNameSafe(MonsterClass.Get()));
		ScheduleRespawn();
		return;
	}

	SpawnedMonster = NewMonster;
	NewMonster->OnDestroyed.AddUniqueDynamic(
		this,
		&AServerOnlyMonsterSpawner::HandleSpawnedMonsterDestroyed);

	if (!ApplyMonsterSpawnParameters(NewMonster))
	{
		UE_LOG(
			LogServerOnlyMonsterSpawner,
			Warning,
			TEXT(
				"Monster class '%s' does not expose both expected leash properties; "
				"the monster will use its own defaults."),
			*GetPathNameSafe(MonsterClass.Get()));
	}

	UGameplayStatics::FinishSpawningActor(
		NewMonster,
		SpawnTransform,
		ESpawnActorScaleMethod::MultiplyWithRoot);
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
		RespawnTimerHandle = TimerManager.SetTimerForNextTick(
			this,
			&AServerOnlyMonsterSpawner::SpawnMonster);
		return;
	}

	TimerManager.SetTimer(
		RespawnTimerHandle,
		this,
		&AServerOnlyMonsterSpawner::SpawnMonster,
		RespawnCooldown,
		false);
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
	const bool bSetMaxLeash = SetNumericProperty(
		Monster,
		TEXT("Max Leash Distance From Spawn Point"),
		MaxLeashDistanceFromSpawnPoint);
	const bool bSetMinLeash = SetNumericProperty(
		Monster,
		TEXT("Min Leash Distance From Spawn Point To Resume Roaming"),
		MinLeashDistanceFromSpawnPointToResumeRoaming);

	return bSetMaxLeash && bSetMinLeash;
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
