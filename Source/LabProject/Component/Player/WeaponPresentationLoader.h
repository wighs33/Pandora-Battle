#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

class UItemDefinition;
struct FStreamableHandle;

/**
 * 무기 외형 에셋(Actor 클래스·몽타주·애니메이션 레이어 등)을 비동기로 불러 두고, 소유자가 놓을 때까지 잡아 둔다.
 * 같은 무기를 여러 번 요청해도 로딩은 한 번만 하고, 끝나면 기다리던 콜백을 모두 부른다.
 */
class LABPROJECT_API FWeaponPresentationLoader final
{
public:
	/** 장착 몽타주 없이 장착하는 설정(bRequiresEquipMontage = false)이면 장착 몽타주는 기다리지 않는다. */
	static bool IsLoaded(const UItemDefinition* ItemDefinition, bool bRequiresEquipMontage);

	/**
	 * 무기 외형을 불러 둔다. 이미 불려 있으면 OnLoaded를 바로 부르고, 아니면 로딩이 끝날 때 부른다.
	 * Owner가 사라진 뒤에는 콜백을 부르지 않는다. 로딩을 시작하지 못하면 false.
	 */
	bool Request(UObject& Owner, const UItemDefinition* ItemDefinition, bool bRequiresEquipMontage, FSimpleDelegate OnLoaded);

	/** 기다리던 콜백을 버리고 잡아 둔 에셋을 놓는다. */
	void Reset();

private:
	void HandleLoaded(FPrimaryAssetId ItemDefinitionId, bool bRequiresEquipMontage);

	TMap<FPrimaryAssetId, TSharedPtr<FStreamableHandle>> LoadHandles;
	TMap<FPrimaryAssetId, TArray<FSimpleDelegate>> PendingCallbacks;
};
