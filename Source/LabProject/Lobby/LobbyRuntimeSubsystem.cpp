#include "Lobby/LobbyRuntimeSubsystem.h"

#include "Data/ContentDataSubsystem.h"
#include "Definition/Level/LevelDefinition.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Settings/GameSettingsSubsystem.h"
#include "Settings/ProjectBootstrapSettings.h"
#include "UI/UiSubsystem.h"
#include "UI/WidgetContentBundleLease.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyRuntimeSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogLobbyRuntimeSubsystem, Log, All);

namespace
{
	FString TrimNickname(FString Nickname)
	{
		Nickname.TrimStartAndEndInline();
		return Nickname;
	}

	bool IsUsableResolvedNickname(const FString& Nickname)
	{
		const FString TrimmedNickname = TrimNickname(Nickname);
		return !TrimmedNickname.IsEmpty() && !TrimmedNickname.Equals(TEXT("NullUser"), ESearchCase::IgnoreCase);
	}

	FText MakeFallbackNickname(const int32 FallbackIndex)
	{
		return FText::Format(
			NSLOCTEXT("Lobby", "DefaultNicknameFormat", "User{0}"),
			FallbackIndex > 0 ? FallbackIndex : 1);
	}

	bool IsSteamSubsystemName(const FName SubsystemName)
	{
		return SubsystemName.ToString().Equals(TEXT("STEAM"), ESearchCase::IgnoreCase);
	}
}

void ULobbyRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UContentDataSubsystem>();
	Collection.InitializeDependency<UGameSettingsSubsystem>();
	LoadedLevelDefinition = nullptr;
	bLevelDefinitionPreloadPending = false;
	bLevelDefinitionReady = false;
	GameEntryContentPreloadResult = ELobbyContentPreloadResult::NotStarted;
	MissingGameEntryPrimaryAssetIds.Reset();
}

void ULobbyRuntimeSubsystem::Deinitialize()
{
	ReleaseLobbyEntryContentPreload();
	ReleaseGameEntryContentPreload();
	LoadedLevelDefinition = nullptr;
	bLevelDefinitionPreloadPending = false;
	bLevelDefinitionReady = false;
	Super::Deinitialize();
}
void ULobbyRuntimeSubsystem::BeginLobbyEntryContentPreload()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		TArray<TWeakObjectPtr<UUiSubsystem>> InvalidUiSubsystems;
		for (const TPair<TWeakObjectPtr<UUiSubsystem>,
			TSharedPtr<FWidgetContentBundleLease>>& LeasePair :
			LobbyWidgetBundleLeases)
		{
			if (!LeasePair.Key.IsValid())
			{
				InvalidUiSubsystems.Add(LeasePair.Key);
			}
		}
		for (const TWeakObjectPtr<UUiSubsystem>& InvalidUiSubsystem :
			InvalidUiSubsystems)
		{
			LobbyWidgetBundleLeases.Remove(InvalidUiSubsystem);
		}

		for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			if (UUiSubsystem* UiSubsystem =
				LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr)
			{
				UiSubsystem->EnsureConfiguredWidgetContentPreload();
				TSharedPtr<FWidgetContentBundleLease>& BundleLease =
					LobbyWidgetBundleLeases.FindOrAdd(UiSubsystem);
				if (BundleLease.IsValid()
					&& BundleLease->GetState() == EWidgetContentBundleState::Failed)
				{
					BundleLease.Reset();
				}
				if (!BundleLease.IsValid())
				{
					BundleLease = UiSubsystem->AcquireConfiguredWidgetContentBundle(
						EWidgetContentBundle::Lobby);
				}
			}
		}
	}

	if (UGameSettingsSubsystem* GameSettingsSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UGameSettingsSubsystem>() : nullptr)
	{
		GameSettingsSubsystem->PreloadRuntimeContentAsync();
	}
	UContentDataSubsystem* ContentDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentDataSubsystem)
	{
		UE_LOG(
			LogLobbyRuntimeSubsystem,
			Error,
			TEXT("Lobby entry preload could not start because ContentDataSubsystem is unavailable."));
		return;
	}
	ContentDataSubsystem->EnsureSkillDataAssetsPreload();

	if (bLevelDefinitionReady || bLevelDefinitionPreloadPending)
	{
		return;
	}

	bLevelDefinitionPreloadPending = true;
	const FSoftObjectPath LevelDefinitionPath =
		ULevelDefinition::GetDefaultDefinitionPath();
	if (!LevelDefinitionPath.IsValid())
	{
		bLevelDefinitionPreloadPending = false;
		UE_LOG(
			LogLobbyRuntimeSubsystem,
			Error,
			TEXT("Lobby entry preload has no configured Level Definition."));
		return;
	}
	const TWeakObjectPtr<ThisClass> WeakThis(this);
	TArray<FSoftObjectPath> DefinitionPaths = { LevelDefinitionPath };
	const FSoftObjectPath MatchRulePath =
		UMatchRuleDefinition::GetDefaultDefinitionPath();
	if (MatchRulePath.IsValid())
	{
		DefinitionPaths.AddUnique(MatchRulePath);
	}
	TSharedPtr<FStreamableHandle> PreloadHandle =
		ContentDataSubsystem->PreloadSoftObjectPathsAsync(
			DefinitionPaths,
			FSimpleDelegate::CreateLambda(
				[WeakThis]()
				{
					if (ThisClass* This = WeakThis.Get())
					{
						This->HandleLevelDefinitionPreloadComplete();
					}
				}));
	if (PreloadHandle.IsValid())
	{
		LevelDefinitionPreloadHandle = MoveTemp(PreloadHandle);
	}
}

bool ULobbyRuntimeSubsystem::IsLobbyEntryContentReady() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UGameSettingsSubsystem* GameSettingsSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UGameSettingsSubsystem>() : nullptr;
	const UContentDataSubsystem* ContentDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	return GameSettingsSubsystem
		&& GameSettingsSubsystem->IsRuntimeContentReady()
		&& ContentDataSubsystem
		&& ContentDataSubsystem->IsSkillDataAssetsReady()
		&& bLevelDefinitionReady
		&& LoadedLevelDefinition != nullptr
		&& IsLocalPlayerWidgetContentReady();
}

const UMatchRuleDefinition*
ULobbyRuntimeSubsystem::GetLoadedLobbyMatchRuleDefinition() const
{
	return Cast<UMatchRuleDefinition>(
		UMatchRuleDefinition::GetDefaultDefinitionPath().ResolveObject());
}

void ULobbyRuntimeSubsystem::BeginGameEntryContentPreload()
{
	if (GameEntryContentPreloadResult == ELobbyContentPreloadResult::Success
		|| GameEntryContentPreloadResult == ELobbyContentPreloadResult::Loading)
	{
		return;
	}

	ReleaseGameEntryContentPreload();
	GameEntryContentPreloadResult = ELobbyContentPreloadResult::Loading;

	UGameInstance* GameInstance = GetGameInstance();
	UContentDataSubsystem* ContentDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UContentDataSubsystem>() : nullptr;
	if (!ContentDataSubsystem)
	{
		UE_LOG(
			LogLobbyRuntimeSubsystem,
			Error,
			TEXT("Game entry preload could not start because ContentDataSubsystem is unavailable."));
		SetGameEntryContentPreloadResult(
			ELobbyContentPreloadResult::Failed);
		return;
	}

	TArray<FPrimaryAssetId> AssetIds;
	GetGameEntryPrimaryAssetIds(AssetIds);
	TArray<FPrimaryAssetId> UnregisteredAssetIds;
	FindUnregisteredGameEntryAssets(AssetIds, UnregisteredAssetIds);
	if (!UnregisteredAssetIds.IsEmpty())
	{
		SetGameEntryContentPreloadResult(
			ELobbyContentPreloadResult::MissingAssets,
			MoveTemp(UnregisteredAssetIds));
		return;
	}

	const uint32 RequestGeneration = GameEntryContentRequestGeneration;
	const TWeakObjectPtr<ThisClass> WeakThis(this);
	TSharedPtr<FStreamableHandle> PreloadHandle =
		ContentDataSubsystem->PreloadPrimaryAssetsAsync(
			AssetIds,
			FSimpleDelegate::CreateLambda(
				[WeakThis, RequestGeneration]()
				{
					if (ThisClass* This = WeakThis.Get())
					{
						This->HandleGameEntryContentPreloadComplete(
							RequestGeneration);
					}
				}));
	if (PreloadHandle.IsValid())
	{
		GameEntryContentPreloadHandle = MoveTemp(PreloadHandle);
	}
	else if (GameEntryContentPreloadResult
		== ELobbyContentPreloadResult::Loading)
	{
		UE_LOG(
			LogLobbyRuntimeSubsystem,
			Error,
			TEXT("Game entry preload request did not return a valid handle."));
		SetGameEntryContentPreloadResult(
			ELobbyContentPreloadResult::Failed);
	}
}

void ULobbyRuntimeSubsystem::CancelGameEntryContentPreload()
{
	ReleaseGameEntryContentPreload();
}

void ULobbyRuntimeSubsystem::GetGameEntryPrimaryAssetIds(
	TArray<FPrimaryAssetId>& OutAssetIds)
{
	OutAssetIds.Reset();
	const UProjectBootstrapSettings* BootstrapSettings =
		GetDefault<UProjectBootstrapSettings>();
	if (!BootstrapSettings)
	{
		return;
	}

	for (const FPrimaryAssetId& AssetId :
		BootstrapSettings->GetGameEntryRequiredPrimaryAssets())
	{
		if (AssetId.IsValid())
		{
			OutAssetIds.AddUnique(AssetId);
		}
	}

	for (const FPrimaryAssetType& AssetType :
		BootstrapSettings->GetGameEntryRequiredPrimaryAssetTypes())
	{
		if (!AssetType.IsValid())
		{
			continue;
		}

		TArray<FPrimaryAssetId> TypeAssetIds;
		UAssetManager::Get().GetPrimaryAssetIdList(AssetType, TypeAssetIds);
		for (const FPrimaryAssetId& AssetId : TypeAssetIds)
		{
			if (AssetId.IsValid())
			{
				OutAssetIds.AddUnique(AssetId);
			}
		}
	}
	OutAssetIds.Sort(
		[](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
		{
			return Left.ToString() < Right.ToString();
		});
}

void ULobbyRuntimeSubsystem::HandleGameEntryContentPreloadComplete(
	const uint32 RequestGeneration)
{
	if (RequestGeneration != GameEntryContentRequestGeneration)
	{
		return;
	}

	TArray<FPrimaryAssetId> AssetIds;
	GetGameEntryPrimaryAssetIds(AssetIds);
	TArray<FPrimaryAssetId> MissingAssetIds;
	FindUnresolvedGameEntryAssets(AssetIds, MissingAssetIds);
	if (!MissingAssetIds.IsEmpty())
	{
		SetGameEntryContentPreloadResult(
			ELobbyContentPreloadResult::MissingAssets,
			MoveTemp(MissingAssetIds));
		return;
	}

	SetGameEntryContentPreloadResult(
		ELobbyContentPreloadResult::Success);
}

void ULobbyRuntimeSubsystem::ReleaseGameEntryContentPreload()
{
	++GameEntryContentRequestGeneration;
	GameEntryContentPreloadResult = ELobbyContentPreloadResult::NotStarted;
	MissingGameEntryPrimaryAssetIds.Reset();
	if (GameEntryContentPreloadHandle.IsValid())
	{
		GameEntryContentPreloadHandle->CancelHandle();
		GameEntryContentPreloadHandle->ReleaseHandle();
		GameEntryContentPreloadHandle.Reset();
	}
}

void ULobbyRuntimeSubsystem::SetGameEntryContentPreloadResult(
	const ELobbyContentPreloadResult Result,
	TArray<FPrimaryAssetId> MissingAssetIds)
{
	GameEntryContentPreloadResult = Result;
	MissingGameEntryPrimaryAssetIds = MoveTemp(MissingAssetIds);

	for (const FPrimaryAssetId& MissingAssetId :
		MissingGameEntryPrimaryAssetIds)
	{
		UE_LOG(
			LogLobbyRuntimeSubsystem,
			Error,
			TEXT("Game entry preload is missing required data asset '%s'."),
			*MissingAssetId.ToString());
	}
}

void ULobbyRuntimeSubsystem::FindUnregisteredGameEntryAssets(
	const TArray<FPrimaryAssetId>& AssetIds,
	TArray<FPrimaryAssetId>& OutMissingAssetIds)
{
	OutMissingAssetIds.Reset();
	const UAssetManager& AssetManager = UAssetManager::Get();
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (!AssetId.IsValid()
			|| !AssetManager.GetPrimaryAssetPath(AssetId).IsValid())
		{
			OutMissingAssetIds.AddUnique(AssetId);
		}
	}
}

void ULobbyRuntimeSubsystem::FindUnresolvedGameEntryAssets(
	const TArray<FPrimaryAssetId>& AssetIds,
	TArray<FPrimaryAssetId>& OutMissingAssetIds)
{
	OutMissingAssetIds.Reset();
	const UAssetManager& AssetManager = UAssetManager::Get();
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		const FSoftObjectPath AssetPath =
			AssetManager.GetPrimaryAssetPath(AssetId);
		if (!AssetPath.IsValid()
			|| (!AssetManager.GetPrimaryAssetObject(AssetId)
				&& !AssetPath.ResolveObject()))
		{
			OutMissingAssetIds.AddUnique(AssetId);
		}
	}
}

bool ULobbyRuntimeSubsystem::IsLocalPlayerWidgetContentReady() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		UUiSubsystem* UiSubsystem =
			LocalPlayer ? LocalPlayer->GetSubsystem<UUiSubsystem>() : nullptr;
		const TSharedPtr<FWidgetContentBundleLease>* BundleLease =
			UiSubsystem ? LobbyWidgetBundleLeases.Find(UiSubsystem) : nullptr;
		if (!UiSubsystem
			|| !UiSubsystem->IsConfiguredWidgetContentReady()
			|| !BundleLease
			|| !BundleLease->IsValid()
			|| !(*BundleLease)->IsReady())
		{
			return false;
		}
	}

	return true;
}

void ULobbyRuntimeSubsystem::HandleLevelDefinitionPreloadComplete()
{
	bLevelDefinitionPreloadPending = false;
	LoadedLevelDefinition = Cast<ULevelDefinition>(
		ULevelDefinition::GetDefaultDefinitionPath().ResolveObject());
	bLevelDefinitionReady = LoadedLevelDefinition != nullptr
		&& !LoadedLevelDefinition->IngameLevels.IsEmpty();

	if (bLevelDefinitionReady)
	{
		for (const FLobbyMatchMapOption& MapOption :
			LoadedLevelDefinition->IngameLevels)
		{
			if (!IsValid(MapOption.Thumbnail))
			{
				bLevelDefinitionReady = false;
				UE_LOG(
					LogLobbyRuntimeSubsystem,
					Error,
					TEXT("Lobby entry preload completed without map thumbnail '%s'."),
					*MapOption.MapKey.ToString());
			}
		}
	}

	if (!bLevelDefinitionReady)
	{
		UE_LOG(
			LogLobbyRuntimeSubsystem,
			Error,
			TEXT("Lobby entry preload did not fully resolve '%s'."),
			*ULevelDefinition::GetDefaultDefinitionPath().ToString());
	}
}

void ULobbyRuntimeSubsystem::ReleaseLobbyEntryContentPreload()
{
	LobbyWidgetBundleLeases.Reset();

	if (LevelDefinitionPreloadHandle.IsValid())
	{
		LevelDefinitionPreloadHandle->CancelHandle();
		LevelDefinitionPreloadHandle->ReleaseHandle();
		LevelDefinitionPreloadHandle.Reset();
	}
	LoadedLevelDefinition = nullptr;
	bLevelDefinitionPreloadPending = false;
	bLevelDefinitionReady = false;
}

void ULobbyRuntimeSubsystem::SetLobbyGameConfig(
	const FName MapKey,
	const FString& TravelMapName,
	const int32 MaxPlayerCount,
	const int32 MaxBotCount)
{
	LobbyRuntimeConfig.SelectedMapKey = MapKey;
	LobbyRuntimeConfig.TravelMapName = TravelMapName;
	LobbyRuntimeConfig.MaxPlayerCount = FMath::Max(MaxPlayerCount, 1);
	LobbyRuntimeConfig.MaxBotCount = FMath::Clamp(MaxBotCount, 0, 100);
}

void ULobbyRuntimeSubsystem::SetLobbyRuntimeConfig(const FLobbyRuntimeConfig& InLobbyRuntimeConfig)
{
	SetLobbyGameConfig(
		InLobbyRuntimeConfig.SelectedMapKey,
		InLobbyRuntimeConfig.TravelMapName,
		InLobbyRuntimeConfig.MaxPlayerCount,
		InLobbyRuntimeConfig.MaxBotCount);
}

void ULobbyRuntimeSubsystem::ResetCachedPlayerMatchIdentities()
{
	CachedPlayerMatchIdentitiesByPlayerKey.Reset();
}

void ULobbyRuntimeSubsystem::CachePlayerMatchIdentityForPlayerState(
	const APlayerState* PlayerState,
	const FPlayerMatchIdentity& MatchIdentity)
{
	if (!PlayerState)
	{
		return;
	}

	const TArray<FString> Keys = MakeLobbyPlayerCacheKeys(PlayerState);
	for (const FString& Key : Keys)
	{
		CachedPlayerMatchIdentitiesByPlayerKey.Add(Key, MatchIdentity);
	}
}

bool ULobbyRuntimeSubsystem::TryGetCachedPlayerMatchIdentityForPlayerState(
	const APlayerState* PlayerState,
	FPlayerMatchIdentity& OutMatchIdentity) const
{
	const TArray<FString> Keys = MakeLobbyPlayerCacheKeys(PlayerState);
	for (const FString& Key : Keys)
	{
		if (const FPlayerMatchIdentity* FoundMatchIdentity = CachedPlayerMatchIdentitiesByPlayerKey.Find(Key))
		{
			OutMatchIdentity = *FoundMatchIdentity;
			return true;
		}
	}

	return false;
}

FText ULobbyRuntimeSubsystem::ResolveDefaultPlayerNickname(
	const APlayerController* PlayerController,
	const APlayerState* PlayerState,
	const int32 FallbackIndex) const
{
	const IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	const FName SubsystemName = OnlineSubsystem ? OnlineSubsystem->GetSubsystemName() : NAME_None;
	const bool bShouldUseOnlineIdentity = OnlineSubsystem && IsSteamSubsystemName(SubsystemName);

	if (bShouldUseOnlineIdentity)
	{
		const IOnlineIdentityPtr IdentityInterface = OnlineSubsystem->GetIdentityInterface();
		if (IdentityInterface.IsValid())
		{
			if (PlayerState)
			{
				const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();
				if (UniqueId.IsValid())
				{
					if (const FUniqueNetIdPtr UniqueNetId = UniqueId.GetUniqueNetId(); UniqueNetId.IsValid())
					{
						const FString OnlineNickname = TrimNickname(IdentityInterface->GetPlayerNickname(*UniqueNetId));
						if (IsUsableResolvedNickname(OnlineNickname))
						{
							return FText::FromString(OnlineNickname);
						}
					}
				}
			}

			if (PlayerController && PlayerController->IsLocalController())
			{
				if (const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
				{
					const FUniqueNetIdRepl PreferredUniqueId = LocalPlayer->GetPreferredUniqueNetId();
					if (PreferredUniqueId.IsValid())
					{
						if (const FUniqueNetIdPtr PreferredNetId = PreferredUniqueId.GetUniqueNetId(); PreferredNetId.IsValid())
						{
							const FString OnlineNickname = TrimNickname(IdentityInterface->GetPlayerNickname(*PreferredNetId));
							if (IsUsableResolvedNickname(OnlineNickname))
							{
								return FText::FromString(OnlineNickname);
							}
						}
					}
				}

				const FString LocalNickname = TrimNickname(IdentityInterface->GetPlayerNickname(0));
				if (IsUsableResolvedNickname(LocalNickname))
				{
					return FText::FromString(LocalNickname);
				}
			}
		}
	}

	if (bShouldUseOnlineIdentity && PlayerState)
	{
		const FString PlayerName = TrimNickname(PlayerState->GetPlayerName());
		if (IsUsableResolvedNickname(PlayerName))
		{
			return FText::FromString(PlayerName);
		}
	}

	return MakeFallbackNickname(FallbackIndex);
}

void ULobbyRuntimeSubsystem::ResetCachedLobbyEquippedSkinSlots()
{
	CachedLobbyEquippedSkinNamesByPlayerKey.Reset();
}

void ULobbyRuntimeSubsystem::CacheLobbyEquippedSkinSlotsForPlayerState(
	const APlayerState* PlayerState,
	const TMap<FGameplayTag, FName>& EquippedSkinNamesBySlot)
{
	if (!PlayerState)
	{
		return;
	}

	TMap<FGameplayTag, FName> CleanEquippedSkinNamesBySlot;
	for (const TPair<FGameplayTag, FName>& EquippedSkinPair : EquippedSkinNamesBySlot)
	{
		if (EquippedSkinPair.Key.IsValid() && !EquippedSkinPair.Value.IsNone())
		{
			CleanEquippedSkinNamesBySlot.Add(EquippedSkinPair.Key, EquippedSkinPair.Value);
		}
	}

	const TArray<FString> Keys = MakeLobbyPlayerCacheKeys(PlayerState);
	for (const FString& Key : Keys)
	{
		CachedLobbyEquippedSkinNamesByPlayerKey.Add(Key, CleanEquippedSkinNamesBySlot);
	}
}

bool ULobbyRuntimeSubsystem::TryGetCachedLobbyEquippedSkinSlotsForPlayerState(
	const APlayerState* PlayerState,
	TMap<FGameplayTag, FName>& OutEquippedSkinNamesBySlot) const
{
	const TArray<FString> Keys = MakeLobbyPlayerCacheKeys(PlayerState);
	for (const FString& Key : Keys)
	{
		if (const TMap<FGameplayTag, FName>* FoundEquippedSkinNames = CachedLobbyEquippedSkinNamesByPlayerKey.Find(Key))
		{
			OutEquippedSkinNamesBySlot = *FoundEquippedSkinNames;
			return true;
		}
	}

	OutEquippedSkinNamesBySlot.Reset();
	return false;
}

void ULobbyRuntimeSubsystem::ResetCachedLobbyPandoraLoadouts()
{
	CachedLobbyPandoraNamesByPlayerKey.Reset();
}

void ULobbyRuntimeSubsystem::CacheLobbyPandoraLoadoutForPlayerState(
	const APlayerState* PlayerState,
	const TMap<EEnum_Direction, FName>& PandoraNamesByDirection)
{
	if (!PlayerState)
	{
		return;
	}

	TMap<EEnum_Direction, FName> CleanPandoraNamesByDirection;
	for (const TPair<EEnum_Direction, FName>& LoadoutPair : PandoraNamesByDirection)
	{
		if (PandoraLoadout::IsLoadoutDirection(LoadoutPair.Key) && !LoadoutPair.Value.IsNone())
		{
			CleanPandoraNamesByDirection.Add(LoadoutPair.Key, LoadoutPair.Value);
		}
	}

	const TArray<FString> Keys = MakeLobbyPlayerCacheKeys(PlayerState);
	for (const FString& Key : Keys)
	{
		CachedLobbyPandoraNamesByPlayerKey.Add(Key, CleanPandoraNamesByDirection);
	}
}

bool ULobbyRuntimeSubsystem::TryGetCachedLobbyPandoraLoadoutForPlayerState(
	const APlayerState* PlayerState,
	TMap<EEnum_Direction, FName>& OutPandoraNamesByDirection) const
{
	const TArray<FString> Keys = MakeLobbyPlayerCacheKeys(PlayerState);
	for (const FString& Key : Keys)
	{
		if (const TMap<EEnum_Direction, FName>* FoundPandoraNames = CachedLobbyPandoraNamesByPlayerKey.Find(Key))
		{
			OutPandoraNamesByDirection = *FoundPandoraNames;
			return true;
		}
	}

	OutPandoraNamesByDirection.Reset();
	return false;
}

void ULobbyRuntimeSubsystem::ResetLocalLobbyPaintCanvasCache()
{
	LocalLobbyPaintCanvasFaceDecalCache = FLobbyPaintCanvasFaceDecalCache();
}

void ULobbyRuntimeSubsystem::CacheLocalLobbyPaintCanvasStroke(
	UTexture2D* BrushTexture,
	const double BrushSize,
	const FVector2D& DrawLocation)
{
	if (BrushSize <= 0.0)
	{
		return;
	}

	FLobbyPaintCanvasStrokeCache& Stroke = LocalLobbyPaintCanvasFaceDecalCache.Strokes.AddDefaulted_GetRef();
	Stroke.BrushTexture = BrushTexture;
	Stroke.BrushSize = BrushSize;
	Stroke.DrawLocation = DrawLocation;
}

void ULobbyRuntimeSubsystem::CacheLocalLobbyPaintCanvasFaceDecal(
	UMaterialInterface* FaceDecalMaterial,
	const FName AttachSocketName,
	const FTransform& FaceDecalTransformOffset,
	const FVector FaceDecalSize,
	const FName TextureParameterName)
{
	if (!FaceDecalMaterial || LocalLobbyPaintCanvasFaceDecalCache.Strokes.IsEmpty())
	{
		return;
	}

	LocalLobbyPaintCanvasFaceDecalCache.bHasFaceDecal = true;
	LocalLobbyPaintCanvasFaceDecalCache.FaceDecalMaterial = FaceDecalMaterial;
	LocalLobbyPaintCanvasFaceDecalCache.AttachSocketName = AttachSocketName;
	LocalLobbyPaintCanvasFaceDecalCache.FaceDecalTransformOffset = FaceDecalTransformOffset;
	LocalLobbyPaintCanvasFaceDecalCache.FaceDecalSize = FaceDecalSize;
	LocalLobbyPaintCanvasFaceDecalCache.TextureParameterName = TextureParameterName;
	LocalLobbyPaintCanvasFaceDecalCache.FaceDecalStrokeCount = LocalLobbyPaintCanvasFaceDecalCache.Strokes.Num();
}

bool ULobbyRuntimeSubsystem::ConsumeLocalLobbyPaintCanvasFaceDecalCache(
	FLobbyPaintCanvasFaceDecalCache& OutFaceDecalCache)
{
	if (!LocalLobbyPaintCanvasFaceDecalCache.bHasFaceDecal
		|| !LocalLobbyPaintCanvasFaceDecalCache.FaceDecalMaterial
		|| LocalLobbyPaintCanvasFaceDecalCache.Strokes.IsEmpty())
	{
		OutFaceDecalCache = FLobbyPaintCanvasFaceDecalCache();
		return false;
	}

	OutFaceDecalCache = LocalLobbyPaintCanvasFaceDecalCache;
	if (OutFaceDecalCache.FaceDecalStrokeCount > 0
		&& OutFaceDecalCache.FaceDecalStrokeCount < OutFaceDecalCache.Strokes.Num())
	{
		OutFaceDecalCache.Strokes.SetNum(OutFaceDecalCache.FaceDecalStrokeCount);
	}

	ResetLocalLobbyPaintCanvasCache();
	return true;
}

void ULobbyRuntimeSubsystem::SetPendingTitleGameResult(const FGameResultPresentationData& GameResultData)
{
	PendingTitleGameResult = GameResultData;
	bHasPendingTitleGameResult = true;
}

void ULobbyRuntimeSubsystem::ClearPendingTitleGameResult()
{
	PendingTitleGameResult = FGameResultPresentationData();
	bHasPendingTitleGameResult = false;
}

bool ULobbyRuntimeSubsystem::ConsumePendingTitleGameResult(FGameResultPresentationData& OutGameResultData)
{
	if (!bHasPendingTitleGameResult)
	{
		OutGameResultData = FGameResultPresentationData();
		return false;
	}

	OutGameResultData = PendingTitleGameResult;
	ClearPendingTitleGameResult();
	return true;
}

TArray<FString> ULobbyRuntimeSubsystem::MakeLobbyPlayerCacheKeys(const APlayerState* PlayerState) const
{
	TArray<FString> Keys;
	if (!PlayerState)
	{
		return Keys;
	}

	const FUniqueNetIdRepl& UniqueId = PlayerState->GetUniqueId();
	if (UniqueId.IsValid())
	{
		if (const FUniqueNetIdPtr UniqueNetId = UniqueId.GetUniqueNetId(); UniqueNetId.IsValid())
		{
			Keys.Add(FString::Printf(TEXT("NetId:%s"), *UniqueNetId->ToString()));
		}
	}

	if (!PlayerState->SavedNetworkAddress.IsEmpty())
	{
		Keys.Add(FString::Printf(TEXT("Addr:%s"), *PlayerState->SavedNetworkAddress));
	}

	if (PlayerState->GetPlayerId() != INDEX_NONE)
	{
		Keys.Add(FString::Printf(TEXT("PlayerId:%d"), PlayerState->GetPlayerId()));
	}

	const FString PlayerName = PlayerState->GetPlayerName();
	if (!PlayerName.IsEmpty())
	{
		Keys.Add(FString::Printf(TEXT("Name:%s"), *PlayerName));
	}

	return Keys;
}
