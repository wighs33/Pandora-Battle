#pragma once

#include "CoreMinimal.h"
#include "Definition/Shop/ShopTypes.h"
#include "ShopEntryUiData.generated.h"

/** 상점 목록 한 칸에 표시할 값. 상품 정의와 현재 프로필 보유·잔액 상태를 합쳐 상점 화면이 만든다. */
USTRUCT(BlueprintType)
struct LABPROJECT_API FShopEntryUiData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	TObjectPtr<UObject> IconResource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	TObjectPtr<UObject> ProductObject = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	EShopProductType ProductType = EShopProductType::Pandora;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop", meta = (ClampMin = "0", UIMin = "0"))
	int32 GoldPrice = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	bool bOwned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	bool bCanAfford = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	bool bCanSell = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "!Shop")
	bool bValid = false;
};
