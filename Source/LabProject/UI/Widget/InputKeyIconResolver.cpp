#include "UI/Widget/InputKeyIconResolver.h"

#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Settings/LocalPlayerSettingsSubsystem.h"
#include "Definition/UI/WidgetClassDefinition.h"

namespace
{
	UObject* LoadMappedIcon(const TSoftObjectPtr<UObject>& IconObject)
	{
		return IconObject.Get();
	}

}

UObject* PdInputKeyIconResolver::ResolveMappedIconObject(
	const FPdInputKeyIconSettings& Settings,
	const FString& Candidate)
{
	if (Candidate.IsEmpty())
	{
		return nullptr;
	}

	if (const TSoftObjectPtr<UObject>* IconObject = Settings.StringToIconMapping.Find(Candidate))
	{
		return LoadMappedIcon(*IconObject);
	}

	for (const TPair<FString, TSoftObjectPtr<UObject>>& Pair : Settings.StringToIconMapping)
	{
		if (Pair.Key.Equals(Candidate, ESearchCase::IgnoreCase))
		{
			return LoadMappedIcon(Pair.Value);
		}
	}

	return nullptr;
}

FString PdInputKeyIconResolver::GetFixedSkillSlotKeyName(const int32 SkillSlotIndex)
{
	switch (SkillSlotIndex)
	{
	case 0:
		return TEXT("Q");
	case 1:
		return TEXT("E");
	case 2:
		return TEXT("R");
	default:
		return FString();
	}
}

FString PdInputKeyIconResolver::GetFixedQuickSlotKeyName(const int32 QuickSlotIndex)
{
	if (QuickSlotIndex < 0 || QuickSlotIndex >= 8)
	{
		return FString();
	}

	return FString::FromInt(QuickSlotIndex + 1);
}

UObject* PdInputKeyIconResolver::ResolveIconObject(
	APlayerController* PlayerController,
	const UInputAction* InputAction,
	const FPdInputKeyIconSettings& Settings)
{
	const ULocalPlayerSettingsSubsystem* LocalPlayerSettings = ULocalPlayerSettingsSubsystem::Get(PlayerController);
	if (!InputAction || !LocalPlayerSettings || Settings.StringToIconMapping.IsEmpty())
	{
		return nullptr;
	}

	const TArray<FKey> MappedKeys = LocalPlayerSettings->QueryKeysMappedToAction(InputAction);
	for (const FKey& MappedKey : MappedKeys)
	{
		TArray<FString> Candidates;
		Candidates.Add(MappedKey.GetDisplayName(false).ToString());
		Candidates.Add(MappedKey.GetDisplayName(true).ToString());
		Candidates.Add(MappedKey.GetFName().ToString());

		const int32 InitialCandidateCount = Candidates.Num();
		for (int32 Index = 0; Index < InitialCandidateCount; ++Index)
		{
			Candidates.Add(Candidates[Index].Replace(TEXT(" "), TEXT("")));
		}

		for (const FString& Candidate : Candidates)
		{
			if (UObject* IconObject = ResolveMappedIconObject(Settings, Candidate))
			{
				return IconObject;
			}
		}
	}

	return nullptr;
}

FSlateBrush PdInputKeyIconResolver::MakeImageBrush(UObject* ResourceObject, const FVector2D ImageSize)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = ImageSize;
	Brush.SetResourceObject(ResourceObject);
	return Brush;
}

FSlateBrush PdInputKeyIconResolver::MakeImageBrushFromExisting(
	const FSlateBrush& ExistingBrush,
	UObject* ResourceObject,
	const FVector2D ImageSize)
{
	FSlateBrush Brush = ExistingBrush;
	if (ImageSize.X > 0.0f && ImageSize.Y > 0.0f)
	{
		Brush.ImageSize = ImageSize;
	}
	Brush.SetResourceObject(ResourceObject);
	return Brush;
}
