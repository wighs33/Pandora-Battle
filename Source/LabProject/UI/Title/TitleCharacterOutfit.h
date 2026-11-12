#pragma once

#include "CoreMinimal.h"
#include "TitleCharacterOutfit.generated.h"

class USkeletalMesh;
class UTexture2D;

/**
 * 타이틀 Luna가 갈아입는 옷 한 벌. 몸 메시의 자세를 그대로 따르는(리더 포즈) 옷 메시와 타이틀 버튼 아이콘이다.
 * BlueprintType은 에디터 스크립트가 BP_TitleHUD의 옷 목록을 채울 수 있게 한다.
 */
USTRUCT(BlueprintType)
struct FTitleCharacterOutfit
{
	GENERATED_BODY()

	/** 고른 옷을 저장할 때 쓰는 이름. 바꾸면 저장해 둔 선택이 첫 옷으로 돌아간다. */
	UPROPERTY(EditAnywhere)
	FName Id;

	/** Luna의 몸과 같은 골격에 얹은 옷 메시 */
	UPROPERTY(EditAnywhere)
	TObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTexture2D> Icon;

	/** 버튼에 마우스를 올리면 보이는 옷 이름(MenuText 키) */
	UPROPERTY(EditAnywhere)
	FName NameKey;
};
