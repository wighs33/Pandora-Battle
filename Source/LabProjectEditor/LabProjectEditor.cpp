#include "Algo/Find.h"
#include "Misc/CoreMisc.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

namespace
{
	const FName ModelContextProtocolModules[] = {TEXT("ModelContextProtocolEngine"), TEXT("ModelContextProtocolEditor")};

	/** MCP 플러그인의 서버 자동 시작을 끈다. 플러그인을 끈 환경에서도 빌드되도록 설정 클래스를 리플렉션으로 찾는다. */
	void DisableModelContextProtocolAutoStart()
	{
		const UClass* SettingsClass = FindObject<UClass>(nullptr, TEXT("/Script/ModelContextProtocolEngine.ModelContextProtocolSettings"));
		const FBoolProperty* AutoStart = SettingsClass ? FindFProperty<FBoolProperty>(SettingsClass, TEXT("bAutoStartServer")) : nullptr;
		if (AutoStart)
		{
			AutoStart->SetPropertyValue_InContainer(SettingsClass->GetDefaultObject(), false);
		}
	}
}

class FLabProjectEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// UStateTree::PostLoad compiles editor assets immediately. Ensure the
		// compiler delegate exists before any project StateTree can be loaded.
		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("StateTreeEditorModule"));

		// 에디터에서 켠 MCP 서버 자동 시작(bAutoStartServer)은 쿡 같은 커맨드릿도 그대로 읽는다. 열려 있는 에디터가 이미 쓰는
		// 포트에 서버를 또 열려다 실패하면 오류가 로그에 남고, UAT는 그 오류 하나로 쿡 전체를 실패로 처리한다.
		// 커맨드릿에는 MCP 서버가 필요 없으므로, 플러그인 모듈이 로드되면 서버를 띄우기(엔진 초기화 뒤) 전에 자동 시작만 끈다.
		if (IsRunningCommandlet())
		{
			ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddLambda([](const FName ModuleName, const EModuleChangeReason Reason)
			{
				if (Reason == EModuleChangeReason::ModuleLoaded && Algo::Find(ModelContextProtocolModules, ModuleName))
				{
					DisableModelContextProtocolAutoStart();
				}
			});
			DisableModelContextProtocolAutoStart();
		}
	}

	virtual void ShutdownModule() override
	{
		FModuleManager::Get().OnModulesChanged().Remove(ModulesChangedHandle);
	}

private:
	FDelegateHandle ModulesChangedHandle;
};

IMPLEMENT_MODULE(FLabProjectEditorModule, LabProjectEditor)
