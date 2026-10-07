#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

// 머티리얼 Custom 노드가 저장소의 Shaders 폴더를 /LabProject 가상 경로로 include하게 한다.
// 셰이더를 컴파일하기 전에 경로가 있어야 하므로, 게임 모듈보다 먼저(PostConfigInit) 올라오는 별도 모듈로 둔다.
class FLabProjectShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		AddShaderSourceDirectoryMapping(TEXT("/LabProject"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Shaders")));
	}
};

IMPLEMENT_MODULE(FLabProjectShadersModule, LabProjectShaders)
