#include "LabProject.h"

#include "Localization/MenuLocalizationSubsystem.h"
#include "Modules/ModuleManager.h"

class FLabProjectModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// 데이터 애셋과 위젯이 메뉴 문구를 찾기 전에 문자열 테이블이 있어야 한다.
		UMenuLocalizationSubsystem::RegisterMenuTextTable();
	}

	virtual void ShutdownModule() override
	{
		UMenuLocalizationSubsystem::UnregisterMenuTextTable();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FLabProjectModule, LabProject, "LabProject");
