#include "Mode/PdGameInstance.h"

#include "Lobby/LobbyRuntimeSubsystem.h"

void UPdGameInstance::SetLobbyGameConfig(
	const FName MapKey,
	const FString& TravelMapName,
	const int32 MaxPlayerCount,
	const int32 MaxBotCount)
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->SetLobbyGameConfig(
			MapKey,
			TravelMapName,
			MaxPlayerCount,
			MaxBotCount);
	}
}

FName UPdGameInstance::GetLobbySelectedMapKey() const
{
	if (const ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->GetLobbySelectedMapKey();
	}

	return NAME_None;
}

FString UPdGameInstance::GetLobbyTravelMapName() const
{
	if (const ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->GetLobbyTravelMapName();
	}

	return FString();
}

int32 UPdGameInstance::GetLobbyMaxPlayerCount() const
{
	if (const ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->GetLobbyMaxPlayerCount();
	}

	return 1;
}

int32 UPdGameInstance::GetLobbyMaxBotCount() const
{
	if (const ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->GetLobbyMaxBotCount();
	}

	return 0;
}

void UPdGameInstance::ResetCachedPlayerMatchIdentities()
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->ResetCachedPlayerMatchIdentities();
	}
}

void UPdGameInstance::CachePlayerMatchIdentityForPlayerState(
	const APlayerState* PlayerState,
	const FPlayerMatchIdentity& MatchIdentity)
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->CachePlayerMatchIdentityForPlayerState(
			PlayerState,
			MatchIdentity);
	}
}

bool UPdGameInstance::TryGetCachedPlayerMatchIdentityForPlayerState(
	const APlayerState* PlayerState,
	FPlayerMatchIdentity& OutMatchIdentity) const
{
	if (const ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->TryGetCachedPlayerMatchIdentityForPlayerState(
			PlayerState,
			OutMatchIdentity);
	}

	return false;
}

FText UPdGameInstance::ResolveDefaultPlayerNickname(
	const APlayerController* PlayerController,
	const APlayerState* PlayerState,
	const int32 FallbackIndex) const
{
	if (const ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->ResolveDefaultPlayerNickname(
			PlayerController,
			PlayerState,
			FallbackIndex);
	}

	return FText::Format(
		NSLOCTEXT("Lobby", "DefaultNicknameFormat", "User{0}"),
		FallbackIndex > 0 ? FallbackIndex : 1);
}

void UPdGameInstance::ResetCachedLobbyEquippedSkinSlots()
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->ResetCachedLobbyEquippedSkinSlots();
	}
}

void UPdGameInstance::CacheLobbyEquippedSkinSlotsForPlayerState(
	const APlayerState* PlayerState,
	const TMap<FGameplayTag, FName>& EquippedSkinNamesBySlot)
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->CacheLobbyEquippedSkinSlotsForPlayerState(
			PlayerState,
			EquippedSkinNamesBySlot);
	}
}

bool UPdGameInstance::TryGetCachedLobbyEquippedSkinSlotsForPlayerState(
	const APlayerState* PlayerState,
	TMap<FGameplayTag, FName>& OutEquippedSkinNamesBySlot) const
{
	if (const ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->TryGetCachedLobbyEquippedSkinSlotsForPlayerState(
			PlayerState,
			OutEquippedSkinNamesBySlot);
	}

	OutEquippedSkinNamesBySlot.Reset();
	return false;
}

void UPdGameInstance::ResetCachedLobbyPandoraLoadouts()
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->ResetCachedLobbyPandoraLoadouts();
	}
}

void UPdGameInstance::CacheLobbyPandoraLoadoutForPlayerState(
	const APlayerState* PlayerState,
	const TMap<EEnum_Direction, FName>& PandoraNamesByDirection)
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->CacheLobbyPandoraLoadoutForPlayerState(
			PlayerState,
			PandoraNamesByDirection);
	}
}

bool UPdGameInstance::TryGetCachedLobbyPandoraLoadoutForPlayerState(
	const APlayerState* PlayerState,
	TMap<EEnum_Direction, FName>& OutPandoraNamesByDirection) const
{
	if (const ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->TryGetCachedLobbyPandoraLoadoutForPlayerState(
			PlayerState,
			OutPandoraNamesByDirection);
	}

	OutPandoraNamesByDirection.Reset();
	return false;
}

void UPdGameInstance::ResetLocalLobbyPaintCanvasCache()
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->ResetLocalLobbyPaintCanvasCache();
	}
}

void UPdGameInstance::CacheLocalLobbyPaintCanvasStroke(
	UTexture2D* BrushTexture,
	const double BrushSize,
	const FVector2D& DrawLocation)
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->CacheLocalLobbyPaintCanvasStroke(
			BrushTexture,
			BrushSize,
			DrawLocation);
	}
}

void UPdGameInstance::CacheLocalLobbyPaintCanvasFaceDecal(
	UMaterialInterface* FaceDecalMaterial,
	const FName AttachSocketName,
	const FTransform& FaceDecalTransformOffset,
	const FVector FaceDecalSize,
	const FName TextureParameterName)
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->CacheLocalLobbyPaintCanvasFaceDecal(
			FaceDecalMaterial,
			AttachSocketName,
			FaceDecalTransformOffset,
			FaceDecalSize,
			TextureParameterName);
	}
}

bool UPdGameInstance::ConsumeLocalLobbyPaintCanvasFaceDecalCache(
	FLobbyPaintCanvasFaceDecalCache& OutFaceDecalCache)
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->ConsumeLocalLobbyPaintCanvasFaceDecalCache(
			OutFaceDecalCache);
	}

	OutFaceDecalCache = FLobbyPaintCanvasFaceDecalCache();
	return false;
}

void UPdGameInstance::SetPendingTitleGameResult(
	const FGameResultPresentationData& GameResultData)
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		LobbySubsystem->SetPendingTitleGameResult(GameResultData);
	}
}

bool UPdGameInstance::ConsumePendingTitleGameResult(
	FGameResultPresentationData& OutGameResultData)
{
	if (ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->ConsumePendingTitleGameResult(OutGameResultData);
	}

	OutGameResultData = FGameResultPresentationData();
	return false;
}

bool UPdGameInstance::HasPendingTitleGameResult() const
{
	if (const ULobbyRuntimeSubsystem* LobbySubsystem =
		GetSubsystem<ULobbyRuntimeSubsystem>())
	{
		return LobbySubsystem->HasPendingTitleGameResult();
	}

	return false;
}
