#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProjectTagConfigWorldSubsystem.generated.h"

class UProjectTagConfig;

DECLARE_MULTICAST_DELEGATE_OneParam(FPdProjectTagConfigChanged, const UProjectTagConfig*);

UCLASS()
class LABPROJECT_API UProjectTagConfigWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	void SetProjectTagConfig(UProjectTagConfig* NewProjectTagConfig);
	void ClearProjectTagConfig(const UProjectTagConfig* ConfigToClear);

	const UProjectTagConfig* GetProjectTagConfig() const { return ProjectTagConfig; }

	FPdProjectTagConfigChanged OnProjectTagConfigChanged;

private:
	UPROPERTY(Transient)
	TObjectPtr<UProjectTagConfig> ProjectTagConfig;
};
