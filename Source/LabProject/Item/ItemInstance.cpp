#include "Item/ItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ItemInstance)

DEFINE_LOG_CATEGORY(ItemInstanceLog);

FGuid UItemInstance::GetOrCreateItemId()
{
	EnsureItemId();
	return ItemId;
}

void UItemInstance::EnsureItemId()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (!ItemId.IsValid())
	{
		// 아이디 생성
		ItemId = FGuid::NewGuid();
	}
}
