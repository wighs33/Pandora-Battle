#include "Definition/Lobby/LobbyPreviewDefinition.h"

#include "Engine/AssetManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(LobbyPreviewDefinition)

namespace
{
	constexpr int32 LobbyPreviewPandoraLevel = 3;

	const TArray<FLobbyPreviewItemStackGrant>& DefaultLobbyPreviewItemStackGrants()
	{
		static const TArray<FLobbyPreviewItemStackGrant> Grants = {
			FLobbyPreviewItemStackGrant(
				FPrimaryAssetId(FPrimaryAssetType(TEXT("ItemDefinition")), TEXT("DA_HealPotion")),
				500,
				0),
			FLobbyPreviewItemStackGrant(
				FPrimaryAssetId(FPrimaryAssetType(TEXT("ItemDefinition")), TEXT("DA_ManaPotion")),
				500,
				1),
			FLobbyPreviewItemStackGrant(
				FPrimaryAssetId(FPrimaryAssetType(TEXT("ItemDefinition")), TEXT("DA_StaminaPotion")),
				500,
				2)
		};
		return Grants;
	}

	const TArray<FLobbyPreviewGestureSlotGrant>& DefaultLobbyPreviewGestureSlotGrants()
	{
		static const TArray<FLobbyPreviewGestureSlotGrant> Grants = {
			FLobbyPreviewGestureSlotGrant(
				FPrimaryAssetId(FPrimaryAssetType(TEXT("SkinDefinition")), TEXT("DA_HandRaising")),
				0)
		};
		return Grants;
	}

	const TArray<FName>& DefaultLobbyPreviewPandoraKeys()
	{
		static const TArray<FName> Keys = {
			TEXT("Fire"),
			TEXT("Light"),
			TEXT("Machine"),
			TEXT("Freeze"),
			TEXT("Lightning"),
			TEXT("Darkness")
		};
		return Keys;
	}

	FString NormalizeLobbyPreviewPandoraKey(const FName PandoraKeyName)
	{
		FString PandoraKey = PandoraKeyName.ToString();
		PandoraKey.RemoveFromStart(TEXT("DA_Pandora_"), ESearchCase::IgnoreCase);
		PandoraKey.RemoveFromStart(TEXT("Pandora_"), ESearchCase::IgnoreCase);
		return PandoraKey;
	}

	bool ContainsNormalizedPandoraKey(const TArray<FName>& PandoraKeys, const FString& NormalizedPandoraKey)
	{
		for (const FName PandoraKey : PandoraKeys)
		{
			if (NormalizedPandoraKey.Equals(NormalizeLobbyPreviewPandoraKey(PandoraKey), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	void AddUniqueNormalizedPandoraKey(TArray<FName>& PandoraKeys, const FName PandoraKey)
	{
		if (PandoraKey.IsNone())
		{
			return;
		}

		const FString NormalizedPandoraKey = NormalizeLobbyPreviewPandoraKey(PandoraKey);
		if (!NormalizedPandoraKey.IsEmpty() && !ContainsNormalizedPandoraKey(PandoraKeys, NormalizedPandoraKey))
		{
			PandoraKeys.Add(PandoraKey);
		}
	}

#if WITH_EDITOR
	void MarkLobbyPreviewInvalid(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Result = EDataValidationResult::Invalid;
		Context.AddError(Message);
	}

	FText LobbyPreviewFieldText(const TCHAR* FieldName)
	{
		return FText::FromString(FString(FieldName));
	}

	void ValidateGrantTiming(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const float GrantRetryDelay,
		const int32 GrantMaxAttempts)
	{
		if (!FMath::IsFinite(GrantRetryDelay) || GrantRetryDelay <= 0.0f)
		{
			MarkLobbyPreviewInvalid(Context, Result, NSLOCTEXT(
				"LobbyPreviewDefinition",
				"InvalidGrantRetryDelay",
				"GrantRetryDelay must be a positive finite value."));
		}
		else if (GrantRetryDelay < 0.05f)
		{
			Context.AddWarning(NSLOCTEXT(
				"LobbyPreviewDefinition",
				"ClampedGrantRetryDelay",
				"GrantRetryDelay is below 0.05 and will be clamped at runtime."));
		}

		if (GrantMaxAttempts < 1)
		{
			MarkLobbyPreviewInvalid(Context, Result, NSLOCTEXT(
				"LobbyPreviewDefinition",
				"InvalidGrantMaxAttempts",
				"GrantMaxAttempts must be at least 1."));
		}
	}

	void ValidatePreviewPandoraLevel(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const int32 PreviewPandoraLevel)
	{
		if (PreviewPandoraLevel < 1)
		{
			MarkLobbyPreviewInvalid(Context, Result, NSLOCTEXT(
				"LobbyPreviewDefinition",
				"InvalidPreviewPandoraLevel",
				"PreviewPandoraLevel must be at least 1."));
		}
	}

	void ValidatePreviewAttributes(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FLobbyPreviewAttributeSettings& Attributes)
	{
		auto ValidateAttribute = [&Context, &Result](
			const float Value,
			const TCHAR* FieldName)
		{
			if (!FMath::IsFinite(Value) || Value < 0.0f)
			{
				MarkLobbyPreviewInvalid(Context, Result, FText::Format(
					NSLOCTEXT(
						"LobbyPreviewDefinition",
						"InvalidPreviewAttribute",
						"PreviewAttributes.{0} must be a non-negative finite value."),
					LobbyPreviewFieldText(FieldName)));
			}
		};

		ValidateAttribute(Attributes.MaxHealth, TEXT("MaxHealth"));
		ValidateAttribute(Attributes.Health, TEXT("Health"));
		ValidateAttribute(Attributes.Shield, TEXT("Shield"));
		ValidateAttribute(Attributes.MaxMana, TEXT("MaxMana"));
		ValidateAttribute(Attributes.Mana, TEXT("Mana"));
		ValidateAttribute(Attributes.MaxStamina, TEXT("MaxStamina"));
		ValidateAttribute(Attributes.Stamina, TEXT("Stamina"));

		if (Attributes.Health > Attributes.MaxHealth)
		{
			Context.AddWarning(NSLOCTEXT(
				"LobbyPreviewDefinition",
				"HealthExceedsMaximum",
				"PreviewAttributes.Health exceeds MaxHealth."));
		}
		if (Attributes.Mana > Attributes.MaxMana)
		{
			Context.AddWarning(NSLOCTEXT(
				"LobbyPreviewDefinition",
				"ManaExceedsMaximum",
				"PreviewAttributes.Mana exceeds MaxMana."));
		}
		if (Attributes.Stamina > Attributes.MaxStamina)
		{
			Context.AddWarning(NSLOCTEXT(
				"LobbyPreviewDefinition",
				"StaminaExceedsMaximum",
				"PreviewAttributes.Stamina exceeds MaxStamina."));
		}
	}

	TSet<FString> BuildKnownPandoraKeySet()
	{
		TArray<FPrimaryAssetId> PandoraDefinitionIds;
		UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("PandoraDefinition")), PandoraDefinitionIds);

		TSet<FString> KnownKeys;
		for (const FPrimaryAssetId& PandoraDefinitionId : PandoraDefinitionIds)
		{
			const FString NormalizedKey = NormalizeLobbyPreviewPandoraKey(PandoraDefinitionId.PrimaryAssetName);
			if (!NormalizedKey.IsEmpty())
			{
				KnownKeys.Add(NormalizedKey.ToLower());
			}
		}

		return KnownKeys;
	}

	void ValidatePandoraKeyArray(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const TArray<FName>& PandoraKeys,
		const TCHAR* FieldName,
		const TSet<FString>& KnownPandoraKeys,
		TSet<FString>& OutNormalizedKeys)
	{
		for (int32 Index = 0; Index < PandoraKeys.Num(); ++Index)
		{
			const FName PandoraKey = PandoraKeys[Index];
			if (PandoraKey.IsNone())
			{
				MarkLobbyPreviewInvalid(Context, Result, FText::Format(
					NSLOCTEXT("LobbyPreviewDefinition", "InvalidPandoraKeyNone", "{0}[{1}] must not be None."),
					LobbyPreviewFieldText(FieldName),
					FText::AsNumber(Index)));
				continue;
			}

			const FString NormalizedKey = NormalizeLobbyPreviewPandoraKey(PandoraKey);
			if (NormalizedKey.IsEmpty())
			{
				MarkLobbyPreviewInvalid(Context, Result, FText::Format(
					NSLOCTEXT("LobbyPreviewDefinition", "InvalidPandoraKeyEmpty", "{0}[{1}] resolves to an empty Pandora key."),
					LobbyPreviewFieldText(FieldName),
					FText::AsNumber(Index)));
				continue;
			}

			const FString NormalizedKeyLower = NormalizedKey.ToLower();
			if (OutNormalizedKeys.Contains(NormalizedKeyLower))
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("LobbyPreviewDefinition", "DuplicatePandoraKey", "{0} contains a duplicate Pandora key: {1}"),
					LobbyPreviewFieldText(FieldName),
					FText::FromString(NormalizedKey)));
			}
			OutNormalizedKeys.Add(NormalizedKeyLower);

			if (!KnownPandoraKeys.IsEmpty() && !KnownPandoraKeys.Contains(NormalizedKeyLower))
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("LobbyPreviewDefinition", "UnknownPandoraKey", "{0}[{1}] does not match a discovered PandoraDefinition: {2}"),
					LobbyPreviewFieldText(FieldName),
					FText::AsNumber(Index),
					FText::FromName(PandoraKey)));
			}
		}
	}

	void ValidatePandoraPreviewPolicy(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const bool bGrantPreviewPandoras,
		const TArray<FName>& PreviewPandoraKeys)
	{
		if (bGrantPreviewPandoras && PreviewPandoraKeys.IsEmpty())
		{
			Context.AddWarning(NSLOCTEXT(
				"LobbyPreviewDefinition",
				"PreviewPandorasEnabledWithoutKeys",
				"bGrantPreviewPandoras is true, but PreviewPandoraKeys is empty. No preview Pandoras will be granted."));
		}

		const TSet<FString> KnownPandoraKeys = BuildKnownPandoraKeySet();
		TSet<FString> IncludedKeys;
		ValidatePandoraKeyArray(Context, Result, PreviewPandoraKeys, TEXT("PreviewPandoraKeys"), KnownPandoraKeys, IncludedKeys);
	}
#endif
}

ULobbyPreviewDefinition::ULobbyPreviewDefinition()
{
	PreviewPandoraKeys = DefaultLobbyPreviewPandoraKeys();
	PreviewItemStackGrants = DefaultLobbyPreviewItemStackGrants();
	PreviewGestureSlotGrants = DefaultLobbyPreviewGestureSlotGrants();
}

FPrimaryAssetId ULobbyPreviewDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("LobbyPreviewDefinition"), GetFName());
}

bool ULobbyPreviewDefinition::ShouldGrantAllWeapons() const
{
	return bGrantAllWeapons;
}

bool ULobbyPreviewDefinition::ShouldGrantSavedSkins() const
{
	return bGrantSavedSkins;
}

bool ULobbyPreviewDefinition::ShouldGrantPreviewPandoras() const
{
	return bGrantPreviewPandoras;
}

float ULobbyPreviewDefinition::GetGrantRetryDelay() const
{
	return FMath::Max(GrantRetryDelay, 0.05f);
}

int32 ULobbyPreviewDefinition::GetGrantMaxAttempts() const
{
	return FMath::Max(GrantMaxAttempts, 1);
}

int32 ULobbyPreviewDefinition::GetPreviewPandoraLevel() const
{
	return FMath::Max(PreviewPandoraLevel, LobbyPreviewPandoraLevel);
}

void ULobbyPreviewDefinition::GetEffectivePreviewItemStackGrants(
	TArray<FLobbyPreviewItemStackGrant>& OutItemStackGrants) const
{
	OutItemStackGrants = PreviewItemStackGrants;

	for (FLobbyPreviewItemStackGrant& ConfiguredGrant : OutItemStackGrants)
	{
		if (ConfiguredGrant.QuickSlotIndex != INDEX_NONE)
		{
			continue;
		}

		const FLobbyPreviewItemStackGrant* DefaultGrant =
			DefaultLobbyPreviewItemStackGrants().FindByPredicate(
				[&ConfiguredGrant](const FLobbyPreviewItemStackGrant& Candidate)
				{
					return Candidate.ItemDefinitionId == ConfiguredGrant.ItemDefinitionId;
				});
		if (DefaultGrant)
		{
			ConfiguredGrant.QuickSlotIndex = DefaultGrant->QuickSlotIndex;
		}
	}
}

void ULobbyPreviewDefinition::GetEffectivePreviewGestureSlotGrants(
	TArray<FLobbyPreviewGestureSlotGrant>& OutGestureSlotGrants) const
{
	OutGestureSlotGrants = DefaultLobbyPreviewGestureSlotGrants();

	for (const FLobbyPreviewGestureSlotGrant& ConfiguredGrant : PreviewGestureSlotGrants)
	{
		const int32 ExistingIndex = OutGestureSlotGrants.IndexOfByPredicate(
			[&ConfiguredGrant](const FLobbyPreviewGestureSlotGrant& Candidate)
			{
				return Candidate.GestureSlotIndex == ConfiguredGrant.GestureSlotIndex;
			});
		if (ExistingIndex != INDEX_NONE)
		{
			OutGestureSlotGrants[ExistingIndex] = ConfiguredGrant;
		}
		else
		{
			OutGestureSlotGrants.Add(ConfiguredGrant);
		}
	}
}

void ULobbyPreviewDefinition::GetEffectivePreviewPandoraKeys(TArray<FName>& OutPreviewPandoraKeys) const
{
	OutPreviewPandoraKeys.Reset();

	for (const FName DefaultPandoraKey : DefaultLobbyPreviewPandoraKeys())
	{
		AddUniqueNormalizedPandoraKey(OutPreviewPandoraKeys, DefaultPandoraKey);
	}

	for (const FName ConfiguredPandoraKey : PreviewPandoraKeys)
	{
		AddUniqueNormalizedPandoraKey(OutPreviewPandoraKeys, ConfiguredPandoraKey);
	}
}

bool ULobbyPreviewDefinition::IsPandoraKeyAllowed(const FName PandoraKeyName) const
{
	if (PandoraKeyName.IsNone() || !ShouldGrantPreviewPandoras())
	{
		return false;
	}

	const FString NormalizedPandoraKey = NormalizeLobbyPreviewPandoraKey(PandoraKeyName);
	TArray<FName> EffectivePreviewPandoraKeys;
	GetEffectivePreviewPandoraKeys(EffectivePreviewPandoraKeys);
	return ContainsNormalizedPandoraKey(EffectivePreviewPandoraKeys, NormalizedPandoraKey);
}

#if WITH_EDITOR
EDataValidationResult ULobbyPreviewDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	ValidateGrantTiming(Context, Result, GrantRetryDelay, GrantMaxAttempts);
	ValidatePreviewPandoraLevel(Context, Result, PreviewPandoraLevel);
	ValidatePreviewAttributes(Context, Result, PreviewAttributes);
	TArray<FName> EffectivePreviewPandoraKeys;
	GetEffectivePreviewPandoraKeys(EffectivePreviewPandoraKeys);
	ValidatePandoraPreviewPolicy(Context, Result, bGrantPreviewPandoras, EffectivePreviewPandoraKeys);

	for (int32 Index = 0; Index < PreviewItemStackGrants.Num(); ++Index)
	{
		const FLobbyPreviewItemStackGrant& StackGrant = PreviewItemStackGrants[Index];
		if (!StackGrant.ItemDefinitionId.IsValid())
		{
			MarkLobbyPreviewInvalid(Context, Result, FText::Format(
				NSLOCTEXT("LobbyPreviewDefinition", "InvalidItemStackGrant", "PreviewItemStackGrants[{0}] must have a valid ItemDefinitionId."),
				FText::AsNumber(Index)));
		}
		if (StackGrant.Quantity < 0)
		{
			MarkLobbyPreviewInvalid(Context, Result, FText::Format(
				NSLOCTEXT("LobbyPreviewDefinition", "InvalidItemStackQuantity", "PreviewItemStackGrants[{0}].Quantity must not be negative."),
				FText::AsNumber(Index)));
		}
		if (StackGrant.QuickSlotIndex < INDEX_NONE || StackGrant.QuickSlotIndex > 3)
		{
			MarkLobbyPreviewInvalid(Context, Result, FText::Format(
				NSLOCTEXT("LobbyPreviewDefinition", "InvalidItemStackQuickSlot", "PreviewItemStackGrants[{0}].QuickSlotIndex must be between -1 and 3."),
				FText::AsNumber(Index)));
		}
	}

	for (int32 Index = 0; Index < PreviewGestureSlotGrants.Num(); ++Index)
	{
		const FLobbyPreviewGestureSlotGrant& GestureGrant = PreviewGestureSlotGrants[Index];
		if (!GestureGrant.SkinDefinitionId.IsValid())
		{
			MarkLobbyPreviewInvalid(Context, Result, FText::Format(
				NSLOCTEXT("LobbyPreviewDefinition", "InvalidGestureGrant", "PreviewGestureSlotGrants[{0}] must have a valid SkinDefinitionId."),
				FText::AsNumber(Index)));
		}
		if (GestureGrant.GestureSlotIndex < 0 || GestureGrant.GestureSlotIndex > 3)
		{
			MarkLobbyPreviewInvalid(Context, Result, FText::Format(
				NSLOCTEXT("LobbyPreviewDefinition", "InvalidGestureSlot", "PreviewGestureSlotGrants[{0}].GestureSlotIndex must be between 0 and 3."),
				FText::AsNumber(Index)));
		}
	}

	if (!bGrantAllWeapons && !bGrantSavedSkins && !bGrantPreviewPandoras && PreviewItemStackGrants.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT(
			"LobbyPreviewDefinition",
			"NoPreviewContentEnabled",
			"All lobby preview content grant options are disabled."));
	}

	return Result;
}
#endif
