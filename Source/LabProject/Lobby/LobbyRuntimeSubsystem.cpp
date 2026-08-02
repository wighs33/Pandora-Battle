#include "Lobby/LobbyRuntimeSubsystem.h"

#include "Data/ContentDataSubsystem.h"
#include "Definition/Match/MatchRuleDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Pandora/PandoraLoadoutTypes.h"
#include "Settings/GameSettingsSubsystem.h"
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
	LoadedLobbyMatchRuleDefinition = nullptr;
	bLobbyMatchRulePreloadPending = false;
	bLobbyMatchRuleReady = false;
}

void ULobbyRuntimeSubsystem::Deinitialize()
{
	ReleaseLobbyEntryContentPreload();
	LoadedLobbyMatchRuleDefinition = nullptr;
	bLobbyMatchRulePreloadPending = false;
	bLobbyMatchRuleReady = false;
	Super::Deinitialize();
}
void ULobbyRuntimeSubsystem::BeginLobbyEntryContentPreload()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (UGameSettingsSubsystem* GameSettingsSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UGameSettingsSubsystem>() : nullptr)
	{
		GameSettingsSubsystem->PreloadRuntimeContentAsync();
	}

	if (bLobbyMatchRuleReady || bLobbyMatchRulePreloadPending)
	{
		return;
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

	bLobbyMatchRulePreloadPending = true;
	const TWeakObjectPtr<ThisClass> WeakThis(this);
	TSharedPtr<FStreamableHandle> PreloadHandle =
		ContentDataSubsystem->PreloadSoftObjectPathsAsync(
			{ UMatchRuleDefinition::GetDefaultDefinitionPath() },
			FSimpleDelegate::CreateLambda(
				[WeakThis]()
				{
					if (ThisClass* This = WeakThis.Get())
					{
						This->HandleLobbyMatchRulePreloadComplete();
					}
				}));
	if (PreloadHandle.IsValid())
	{
		LobbyMatchRulePreloadHandle = MoveTemp(PreloadHandle);
	}
}

bool ULobbyRuntimeSubsystem::IsLobbyEntryContentReady() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UGameSettingsSubsystem* GameSettingsSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UGameSettingsSubsystem>() : nullptr;
	return GameSettingsSubsystem
		&& GameSettingsSubsystem->IsGameSettingDefinitionReady()
		&& bLobbyMatchRuleReady
		&& LoadedLobbyMatchRuleDefinition != nullptr;
}

void ULobbyRuntimeSubsystem::HandleLobbyMatchRulePreloadComplete()
{
	bLobbyMatchRulePreloadPending = false;
	LoadedLobbyMatchRuleDefinition = Cast<UMatchRuleDefinition>(
		UMatchRuleDefinition::GetDefaultDefinitionPath().ResolveObject());
	bLobbyMatchRuleReady = LoadedLobbyMatchRuleDefinition != nullptr
		&& !LoadedLobbyMatchRuleDefinition->LobbyMapOptions.IsEmpty();

	if (bLobbyMatchRuleReady)
	{
		for (const FLobbyMatchMapOption& MapOption :
			LoadedLobbyMatchRuleDefinition->LobbyMapOptions)
		{
			if (!IsValid(MapOption.Thumbnail))
			{
				bLobbyMatchRuleReady = false;
				UE_LOG(
					LogLobbyRuntimeSubsystem,
					Error,
					TEXT("Lobby entry preload completed without map thumbnail '%s'."),
					*MapOption.MapKey.ToString());
			}
		}
	}

	if (!bLobbyMatchRuleReady)
	{
		UE_LOG(
			LogLobbyRuntimeSubsystem,
			Error,
			TEXT("Lobby entry preload did not fully resolve '%s'."),
			*UMatchRuleDefinition::GetDefaultDefinitionPath().ToString());
	}
}

void ULobbyRuntimeSubsystem::ReleaseLobbyEntryContentPreload()
{
	if (LobbyMatchRulePreloadHandle.IsValid())
	{
		LobbyMatchRulePreloadHandle->CancelHandle();
		LobbyMatchRulePreloadHandle->ReleaseHandle();
		LobbyMatchRulePreloadHandle.Reset();
	}
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

bool ULobbyRuntimeSubsystem::ConsumePendingTitleGameResult(FGameResultPresentationData& OutGameResultData)
{
	if (!bHasPendingTitleGameResult)
	{
		OutGameResultData = FGameResultPresentationData();
		return false;
	}

	OutGameResultData = PendingTitleGameResult;
	PendingTitleGameResult = FGameResultPresentationData();
	bHasPendingTitleGameResult = false;
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
