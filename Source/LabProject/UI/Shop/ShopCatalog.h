#pragma once

#include "CoreMinimal.h"
#include "Definition/Shop/ShopTypes.h"

class UContentDataSubsystem;

// 상점에 늘어놓을 상품의 출처. 수동 목록을 먼저 넣고, 켜진 종류는 불러온 정의를 모두 더한다.
struct FShopCatalogSource
{
	TConstArrayView<FShopCatalogProductReference> ManualProducts;
	bool bIncludeAllPandoras = false;
	bool bIncludeAllSkins = false;
};

namespace PdShopCatalog
{
	// 같은 상품은 처음 한 번만 넣고, 상점 정렬 순서·상품 종류·표시 이름 순으로 정렬한다.
	// 정의가 아직 메모리에 없는 상품은 뺀다.
	TArray<FShopCatalogEntry> Build(const FShopCatalogSource& Source, const UContentDataSubsystem* ContentData);

	UObject* ResolveProductObject(const FShopCatalogProductReference& ProductReference);
}
