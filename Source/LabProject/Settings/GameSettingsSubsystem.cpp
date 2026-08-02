#include "Settings/GameSettingsSubsystem.h"

#include "Data/ContentDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Definition/Settings/GameSettingDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingsSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogGameSettingsSubsystem, Log, All);

UGameSettingsSubsystem::UGameSettingsSubsystem()
	: GameSettingDefinition(GetDefaultGameSettingDefinitionPath())
{
}

void UGameSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UContentDataSubsystem>();
	bGameSettingDefinitionReady = false;
	bRuntimeContentReady = false;
	bRuntimeContentPreloadPending = false;
	CachedGameSettingDefinition = nullptr;
	PendingRuntimeContentCallbacks.Reset();
	PreloadRuntimeContentAsync();
}

void UGameSettingsSubsystem::Deinitialize()
{
	bRuntimeContentPreloadPending = false;
	bGameSettingDefinitionReady = false;
	bRuntimeContentReady = false;
	CachedGameSettingDefinition = nullptr;
	PendingRuntimeContentCallbacks.Reset();
	ReleaseRuntimeContentPreloadHandles();

	Super::Deinitialize();
}

const FSoftObjectPath& UGameSettingsSubsystem::GetDefaultGameSettingDefinitionPath()
{
	static const FSoftObjectPath DefaultSettingDefinitionPath(TEXT("/Game/Data/DA_Setting.DA_Setting"));
	return DefaultSettingDefinitionPath;
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
		CachedGameSettingDefinition = GameSettingDefinition.Get();
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
		FinishRuntimeContentPreload();
		return;
	}

	const FSoftObjectPath DefinitionPath = !GameSettingDefinition.IsNull()
		? GameSettingDefinition.ToSoftObjectPath()
		: GetDefaultGameSettingDefinitionPath();

	ReleaseRuntimeContentPreloadHandles();
	bRuntimeContentPreloadPending = true;
	const TWeakObjectPtr<ThisClass> WeakThis(this);
	TSharedPtr<FStreamableHandle> PreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
		{DefinitionPath},
		FSimpleDelegate::CreateLambda(
			[WeakThis]()
			{
				if (ThisClass* This = WeakThis.Get())
				{
					This->HandleDefinitionPreloadComplete();
				}
			}));

	if (PreloadHandle.IsValid())
	{
		DefinitionPreloadHandle = MoveTemp(PreloadHandle);
	}
}

void UGameSettingsSubsystem::HandleDefinitionPreloadComplete()
{
	if (!bRuntimeContentPreloadPending)
	{
		return;
	}

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
		FinishRuntimeContentPreload();
		return;
	}

	bGameSettingDefinitionReady = true;

	TArray<FSoftObjectPath> RuntimeAssetPaths;
	CachedGameSettingDefinition->GetRuntimePreloadAssetPaths(RuntimeAssetPaths);
	if (RuntimeAssetPaths.IsEmpty())
	{
		FinishRuntimeContentPreload();
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
		FinishRuntimeContentPreload();
		return;
	}

	const TWeakObjectPtr<ThisClass> WeakThis(this);
	TArray<FSoftObjectPath> ExpectedAssetPaths = RuntimeAssetPaths;
	TSharedPtr<FStreamableHandle> PreloadHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			RuntimeAssetPaths,
			FSimpleDelegate::CreateLambda(
				[WeakThis, ExpectedAssetPaths = MoveTemp(ExpectedAssetPaths)]() mutable
				{
					if (ThisClass* This = WeakThis.Get())
					{
						This->HandleRuntimeContentPreloadComplete(
							MoveTemp(ExpectedAssetPaths));
					}
				}));

	if (PreloadHandle.IsValid())
	{
		RuntimeContentPreloadHandle = MoveTemp(PreloadHandle);
	}
}

void UGameSettingsSubsystem::HandleRuntimeContentPreloadComplete(
	TArray<FSoftObjectPath> ExpectedAssetPaths)
{
	if (!bRuntimeContentPreloadPending)
	{
		return;
	}

	for (const FSoftObjectPath& AssetPath : ExpectedAssetPaths)
	{
		if (!AssetPath.ResolveObject())
		{
			UE_LOG(
				LogGameSettingsSubsystem,
				Error,
				TEXT("GameSetting runtime preload completed without resolving '%s'."),
				*AssetPath.ToString());
		}
	}

	FinishRuntimeContentPreload();
}

void UGameSettingsSubsystem::FinishRuntimeContentPreload()
{
	bRuntimeContentPreloadPending = false;
	bRuntimeContentReady = CachedGameSettingDefinition != nullptr;

	TArray<FSimpleDelegate> CompletionCallbacks =
		MoveTemp(PendingRuntimeContentCallbacks);
	PendingRuntimeContentCallbacks.Reset();
	for (FSimpleDelegate& CompletionCallback : CompletionCallbacks)
	{
		CompletionCallback.ExecuteIfBound();
	}
}

void UGameSettingsSubsystem::ReleaseRuntimeContentPreloadHandles()
{
	auto ReleaseHandle = [](TSharedPtr<FStreamableHandle>& Handle)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
			Handle->ReleaseHandle();
			Handle.Reset();
		}
	};

	ReleaseHandle(RuntimeContentPreloadHandle);
	ReleaseHandle(DefinitionPreloadHandle);
}
