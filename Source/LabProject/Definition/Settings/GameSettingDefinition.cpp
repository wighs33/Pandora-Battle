#include "Definition/Settings/GameSettingDefinition.h"

#include "GameplayEffect.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingDefinition)

FPrimaryAssetId UGameSettingDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("GameSetting"), GetFName());
}

void UGameSettingDefinition::GetRuntimePreloadAssetPaths(
	TArray<FSoftObjectPath>& OutAssetPaths) const
{
	TSet<FSoftObjectPath> UniquePaths;
	const auto AddPath =
		[&UniquePaths](const FSoftObjectPath& AssetPath)
		{
			if (AssetPath.IsValid() && !AssetPath.IsNull())
			{
				UniquePaths.Add(AssetPath);
			}
		};

	AddPath(MouseCursorTexture.ToSoftObjectPath());
	AddPath(SoundEnabledButtonTexture.ToSoftObjectPath());
	AddPath(SoundMutedButtonTexture.ToSoftObjectPath());
	AddPath(StartupBgm.ToSoftObjectPath());
	AddPath(LobbyBgm.ToSoftObjectPath());
	AddPath(RoomListBgm.ToSoftObjectPath());
	AddPath(ShopBgm.ToSoftObjectPath());
	AddPath(GuideBgm.ToSoftObjectPath());
	AddPath(TrainingRoomBgm.ToSoftObjectPath());
	AddPath(GameplayBgm.ToSoftObjectPath());

	OutAssetPaths = UniquePaths.Array();
	OutAssetPaths.Sort(
		[](const FSoftObjectPath& Left, const FSoftObjectPath& Right)
		{
			return Left.ToString() < Right.ToString();
		});
}

#if WITH_EDITOR
EDataValidationResult UGameSettingDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	if (!EquippedItemGameplayEffectClass)
	{
		Context.AddError(NSLOCTEXT(
			"GameSettingDefinition",
			"MissingEquippedItemGameplayEffect",
			"EquippedItemGameplayEffectClass is required for equipment gameplay tags."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
