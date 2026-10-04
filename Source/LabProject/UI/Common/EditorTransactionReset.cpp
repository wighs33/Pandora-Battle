#include "UI/Common/EditorTransactionReset.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/TransBuffer.h"
#endif

void PdEditorTransaction::ResetIfContainsPieObjects()
{
#if WITH_EDITOR
	if (GEditor && GEditor->Trans && GEditor->Trans->ContainsPieObjects())
	{
		GEditor->ResetTransaction(NSLOCTEXT("PdEditorTransaction", "TransactionContainedPieUiObject",
			"A PIE UI object was in the transaction buffer and had to be destroyed"));
	}
#endif
}
