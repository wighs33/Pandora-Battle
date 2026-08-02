#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/PrimaryAssetId.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "LobbyPreviewDefinition.generated.h"

USTRUCT(BlueprintType)
struct LABPROJECT_API FLobbyPreviewAttributeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Attributes",
		meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Attributes",
		meta = (ClampMin = "0.0"))
	float Health = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Attributes",
		meta = (ClampMin = "0.0"))
	float Shield = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Attributes",
		meta = (ClampMin = "0.0"))
	float MaxMana = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Attributes",
		meta = (ClampMin = "0.0"))
	float Mana = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Attributes",
		meta = (ClampMin = "0.0"))
	float MaxStamina = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Attributes",
		meta = (ClampMin = "0.0"))
	float Stamina = 100.0f;
};

USTRUCT(BlueprintType)
struct FLobbyPreviewItemStackGrant
{
	GENERATED_BODY()

	FLobbyPreviewItemStackGrant() = default;

	FLobbyPreviewItemStackGrant(
		const FPrimaryAssetId& InItemDefinitionId,
		const int32 InQuantity,
		const int32 InQuickSlotIndex = INDEX_NONE)
		: ItemDefinitionId(InItemDefinitionId)
		, Quantity(InQuantity)
		, QuickSlotIndex(InQuickSlotIndex)
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Item", meta = (AllowedTypes = "ItemDefinition"))
	FPrimaryAssetId ItemDefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Item", meta = (ClampMin = "0"))
	int32 Quantity = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Item",
		meta = (ClampMin = "-1", ClampMax = "3", ToolTip = "-1 leaves the item unassigned; 0-3 map to quick-slot keys 1-4."))
	int32 QuickSlotIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FLobbyPreviewGestureSlotGrant
{
	GENERATED_BODY()

	FLobbyPreviewGestureSlotGrant() = default;

	FLobbyPreviewGestureSlotGrant(
		const FPrimaryAssetId& InSkinDefinitionId,
		const int32 InGestureSlotIndex)
		: SkinDefinitionId(InSkinDefinitionId)
		, GestureSlotIndex(InGestureSlotIndex)
	{
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Gesture", meta = (AllowedTypes = "SkinDefinition"))
	FPrimaryAssetId SkinDefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Gesture",
		meta = (ClampMin = "0", ClampMax = "3", ToolTip = "0-3 map to gesture keys 5-8."))
	int32 GestureSlotIndex = 0;
};

UCLASS(BlueprintType, Const)
class LABPROJECT_API ULobbyPreviewDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	ULobbyPreviewDefinition();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UFUNCTION()
	bool ShouldGrantAllWeapons() const;

	UFUNCTION()
	bool ShouldGrantSavedSkins() const;

	UFUNCTION()
	bool ShouldGrantPreviewPandoras() const;

	UFUNCTION()
	float GetGrantRetryDelay() const;

	UFUNCTION()
	int32 GetGrantMaxAttempts() const;

	UFUNCTION()
	int32 GetPreviewPandoraLevel() const;

	const TArray<FLobbyPreviewItemStackGrant>& GetPreviewItemStackGrants() const
	{
		return PreviewItemStackGrants;
	}
	const FLobbyPreviewAttributeSettings& GetPreviewAttributeSettings() const
	{
		return PreviewAttributes;
	}

	void GetEffectivePreviewItemStackGrants(TArray<FLobbyPreviewItemStackGrant>& OutItemStackGrants) const;
	void GetEffectivePreviewGestureSlotGrants(TArray<FLobbyPreviewGestureSlotGrant>& OutGestureSlotGrants) const;
	void GetEffectivePreviewPandoraKeys(TArray<FName>& OutPreviewPandoraKeys) const;

	UFUNCTION()
	bool IsPandoraKeyAllowed(FName PandoraKeyName) const;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby Preview|Grant")
	bool bGrantAllWeapons = true;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby Preview|Grant")
	bool bGrantSavedSkins = true;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby Preview|Grant")
	bool bGrantPreviewPandoras = true;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby Preview|Grant", meta = (ClampMin = "0.05", UIMin = "0.05", ForceUnits = "s"))
	float GrantRetryDelay = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby Preview|Grant", meta = (ClampMin = "1", UIMin = "1"))
	int32 GrantMaxAttempts = 40;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby Preview|Pandora", meta = (ClampMin = "1", UIMin = "1"))
	int32 PreviewPandoraLevel = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "!Lobby Preview|Attributes")
	FLobbyPreviewAttributeSettings PreviewAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby Preview|Pandora")
	TArray<FName> PreviewPandoraKeys;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby Preview|Item", meta = (TitleProperty = "ItemDefinitionId"))
	TArray<FLobbyPreviewItemStackGrant> PreviewItemStackGrants;

	UPROPERTY(EditDefaultsOnly, Category = "!Lobby Preview|Gesture", meta = (TitleProperty = "SkinDefinitionId"))
	TArray<FLobbyPreviewGestureSlotGrant> PreviewGestureSlotGrants;
};
