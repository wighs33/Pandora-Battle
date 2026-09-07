#include "Definition/Experience/ExperienceDefinition.h"

#include "GameFeaturesSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExperienceDefinition)

// 파생 클래스를 사용해도 기존 ExperienceDefinition 식별자 형식을 유지한다.
FPrimaryAssetId UExperienceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ExperienceDefinition"), GetFName());
}

// 에디터와 실행 중에 같은 설정 규칙을 검사하고, 활성화에 필요한 플러그인 URL을 구한다.
bool UExperienceDefinition::ResolveGameFeaturePluginURLs(TArray<FString>& OutPluginURLs, FText& OutError) const
{
	OutPluginURLs.Reset();
	OutError = FText::GetEmpty();
	TArray<FString> PluginURLs;
	TSet<FPrimaryAssetId> UniqueGameFeatureIds;
	for (const FPrimaryAssetId& GameFeatureId : GameFeaturesToEnable)
	{
		if (!GameFeatureId.IsValid() || GameFeatureId.PrimaryAssetType != FPrimaryAssetType(TEXT("GameFeatureData")))
		{
			OutError = FText::Format(
				NSLOCTEXT("ExperienceDefinition", "InvalidFeatureId", "Expected a valid GameFeatureData asset id: {0}"),
				FText::FromString(GameFeatureId.ToString()));
			return false;
		}
		if (UniqueGameFeatureIds.Contains(GameFeatureId))
		{
			OutError = FText::Format(
				NSLOCTEXT("ExperienceDefinition", "DuplicateGameFeatureId", "GameFeaturesToEnable contains duplicate GameFeatureData id: {0}"),
				FText::FromString(GameFeatureId.ToString()));
			return false;
		}
		UniqueGameFeatureIds.Add(GameFeatureId);

		FString PluginURL;
		if (!UGameFeaturesSubsystem::Get().GetPluginURLByName(GameFeatureId.PrimaryAssetName.ToString(), PluginURL))
		{
			OutError = FText::Format(
				NSLOCTEXT("ExperienceDefinition", "PluginNameMismatch",
					"No GameFeature plugin matches {0}. The GameFeatureData asset name must match its plugin name."),
				FText::FromString(GameFeatureId.ToString()));
			return false;
		}
		PluginURLs.AddUnique(MoveTemp(PluginURL));
	}

	OutPluginURLs = MoveTemp(PluginURLs);
	return true;
}

#if WITH_EDITOR
// 저장된 Experience 설정을 검사한다. 기본 Pawn이나 추가 GameFeature가 없는 구성도 허용한다.
EDataValidationResult UExperienceDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	TArray<FString> PluginURLs;
	FText Error;
	if (!ResolveGameFeaturePluginURLs(PluginURLs, Error))
	{
		Context.AddError(Error);
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
