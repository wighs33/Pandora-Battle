#include "GameFeature/ProjectTagConfigWorldSubsystem.h"

#include "Common/ProjectTagConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProjectTagConfigWorldSubsystem)

bool UProjectTagConfigWorldSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UProjectTagConfigWorldSubsystem::SetProjectTagConfig(UProjectTagConfig* NewProjectTagConfig)
{
	if (ProjectTagConfig == NewProjectTagConfig)
	{
		return;
	}

	ProjectTagConfig = NewProjectTagConfig;
	OnProjectTagConfigChanged.Broadcast(ProjectTagConfig);
}

void UProjectTagConfigWorldSubsystem::ClearProjectTagConfig(const UProjectTagConfig* ConfigToClear)
{
	if (ConfigToClear && ProjectTagConfig != ConfigToClear)
	{
		return;
	}

	if (!ProjectTagConfig)
	{
		return;
	}

	ProjectTagConfig = nullptr;
	OnProjectTagConfigChanged.Broadcast(nullptr);
}
