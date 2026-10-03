#include "Settings/GameSettingsSubsystem.h"

#include "Data/ContentDataSubsystem.h"
#include "Data/ContentLease.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Definition/Mode/PdGameInstanceDefinition.h"
#include "Definition/Settings/GameSettingDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingsSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogGameSettingsSubsystem, Log, All);

void UGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UContentDataSubsystem>();
	GameSettingDefinition = TSoftObjectPtr<UGameSettingDefinition>(
		GetDefaultGameSettingDefinitionPath());
	bRuntimeContentReady = false;
	bRuntimeContentPreloadPending = false;
	CachedGameSettingDefinition = nullptr;
	PendingRuntimeContentCallbacks.Reset();
	PreloadRuntimeContentAsync();
}

void UGameSettingsSubsystem::Deinitialize()
{
	bRuntimeContentPreloadPending = false;
	bRuntimeContentReady = false;
	CachedGameSettingDefinition = nullptr;
	PendingRuntimeContentCallbacks.Reset();
	RuntimeContentLease.Reset();
	DefinitionLease.Reset();

	Super::Deinitialize();
}

FSoftObjectPath UGameSettingsSubsystem::GetDefaultGameSettingDefinitionPath()
{
	return UPdGameInstanceDefinition::GetConfiguredDefinitionReferences()
		.GameSetting.ToSoftObjectPath();
}

UGameSettingDefinition* UGameSettingsSubsystem::ResolveGameSettingDefinition(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (GameInstance)
	{
		if (UGameSettingsSubsystem* SettingsSubsystem = GameInstance->GetSubsystem<UGameSettingsSubsystem>())
		{
			if (UGameSettingDefinition* SettingDefinition = SettingsSubsystem->GetGameSettingDefinition())
			{
				return SettingDefinition;
			}
		}
	}

	if (UGameSettingDefinition* LoadedDefault = Cast<UGameSettingDefinition>(
		GetDefaultGameSettingDefinitionPath().ResolveObject()))
	{
		return LoadedDefault;
	}

	return GetMutableDefault<UGameSettingDefinition>();
}

UGameSettingDefinition* UGameSettingsSubsystem::ResolveLoadedGameSettingDefinition(
	const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UGameSettingsSubsystem* SettingsSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UGameSettingsSubsystem>() : nullptr;
	return SettingsSubsystem
		? SettingsSubsystem->GetLoadedGameSettingDefinition()
		: nullptr;
}

UGameSettingDefinition* UGameSettingsSubsystem::GetGameSettingDefinition()
{
	if (CachedGameSettingDefinition)
	{
		return CachedGameSettingDefinition;
	}

	if (!GameSettingDefinition.IsNull())
	{
		CachedGameSettingDefinition = GameSettingDefinition.LoadSynchronous();
	}

	if (!CachedGameSettingDefinition)
	{
		CachedGameSettingDefinition = Cast<UGameSettingDefinition>(
			GetDefaultGameSettingDefinitionPath().ResolveObject());
	}

	if (CachedGameSettingDefinition)
	{
		return CachedGameSettingDefinition;
	}

	// 설정 애셋을 동기 로드로도 찾지 못한 경우다. 클래스 기본값으로 계속 동작하되 설정 누락을 한 번 알린다.
	UE_CLOG(!bReportedMissingGameSettingDefinition, LogGameSettingsSubsystem, Warning,
		TEXT("Game setting definition %s could not be loaded, so class defaults are used."),
		*GameSettingDefinition.ToString());
	bReportedMissingGameSettingDefinition = true;
	PreloadRuntimeContentAsync();
	return GetMutableDefault<UGameSettingDefinition>();
}

UGameSettingDefinition* UGameSettingsSubsystem::GetLoadedGameSettingDefinition() const
{
	return bRuntimeContentReady ? CachedGameSettingDefinition.Get() : nullptr;
}

void UGameSettingsSubsystem::PreloadRuntimeContentAsync(
	FSimpleDelegate OnComplete)
{
	if (bRuntimeContentReady && CachedGameSettingDefinition)
	{
		OnComplete.ExecuteIfBound();
		return;
	}

	if (OnComplete.IsBound())
	{
		PendingRuntimeContentCallbacks.Add(MoveTemp(OnComplete));
	}

	if (bRuntimeContentPreloadPending)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		UE_LOG(
			LogGameSettingsSubsystem,
			Error,
			TEXT("GameSetting runtime preload could not start because ContentDataSubsystem is unavailable."));
		FinishRuntimeContentPreload(false);
		return;
	}

	const FSoftObjectPath DefinitionPath = !GameSettingDefinition.IsNull()
		? GameSettingDefinition.ToSoftObjectPath()
		: GetDefaultGameSettingDefinitionPath();

	RuntimeContentLease.Reset();
	DefinitionLease.Reset();
	bRuntimeContentPreloadPending = true;
	DefinitionLease = ContentSubsystem->AcquireContent(
		{DefinitionPath},
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleDefinitionPreloadComplete));
}

void UGameSettingsSubsystem::HandleDefinitionPreloadComplete()
{
	CachedGameSettingDefinition = !GameSettingDefinition.IsNull()
		? GameSettingDefinition.Get()
		: Cast<UGameSettingDefinition>(
			GetDefaultGameSettingDefinitionPath().ResolveObject());

	if (!CachedGameSettingDefinition)
	{
		UE_LOG(
			LogGameSettingsSubsystem,
			Error,
			TEXT("GameSetting runtime content preload completed without resolving '%s'."),
			*(!GameSettingDefinition.IsNull()
				? GameSettingDefinition.ToString()
				: GetDefaultGameSettingDefinitionPath().ToString()));
		FinishRuntimeContentPreload(false);
		return;
	}

	TArray<FSoftObjectPath> RuntimeAssetPaths;
	CachedGameSettingDefinition->GetRuntimePreloadAssetPaths(RuntimeAssetPaths);
	if (RuntimeAssetPaths.IsEmpty())
	{
		FinishRuntimeContentPreload(true);
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentSubsystem)
	{
		UE_LOG(
			LogGameSettingsSubsystem,
			Error,
			TEXT("GameSetting runtime dependencies could not preload because ContentDataSubsystem is unavailable."));
		FinishRuntimeContentPreload(false);
		return;
	}

	RuntimeContentLease = ContentSubsystem->AcquireContent(
		RuntimeAssetPaths,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRuntimeContentPreloadComplete));
}

void UGameSettingsSubsystem::HandleRuntimeContentPreloadComplete()
{
	FinishRuntimeContentPreload(
		RuntimeContentLease.IsValid()
		&& RuntimeContentLease->IsReady());
}

void UGameSettingsSubsystem::FinishRuntimeContentPreload(
	const bool bSucceeded)
{
	bRuntimeContentPreloadPending = false;
	bRuntimeContentReady = bSucceeded
		&& CachedGameSettingDefinition != nullptr;

	TArray<FSimpleDelegate> CompletionCallbacks =
		MoveTemp(PendingRuntimeContentCallbacks);
	PendingRuntimeContentCallbacks.Reset();
	for (FSimpleDelegate& CompletionCallback : CompletionCallbacks)
	{
		CompletionCallback.ExecuteIfBound();
	}
}
