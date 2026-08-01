#include "Modules/ModuleManager.h"

class FLabProjectEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// UStateTree::PostLoad compiles editor assets immediately. Ensure the
		// compiler delegate exists before any project StateTree can be loaded.
		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("StateTreeEditorModule"));
	}
};

IMPLEMENT_MODULE(FLabProjectEditorModule, LabProjectEditor)
