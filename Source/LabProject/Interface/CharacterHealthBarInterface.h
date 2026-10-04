#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CharacterHealthBarInterface.generated.h"

class APlayerController;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class LABPROJECT_API UCharacterHealthBarInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 캐릭터 머리 위 체력바의 계약.
 * 캐릭터와 캐릭터 컴포넌트는 체력바가 어떤 위젯으로 그려지는지 모르고, 이 계약으로 생명주기와 표시만 요청한다.
 */
class LABPROJECT_API ICharacterHealthBarInterface
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	virtual void InitializeHealthBar() = 0;
	virtual void ShutdownHealthBar() = 0;

	/** ASC나 체력 데이터가 준비·변경되었을 때 표시 값을 다시 연결한다. */
	virtual void RefreshViewModel() = 0;

	virtual void SetVisibleForLocalViewer(bool bRequestedVisible) = 0;
	virtual void UpdateVisibilityForLocalViewer(
		APlayerController* LocalPlayerController,
		const FVector& CameraLocation,
		const FRotator& CameraRotation,
		float MaxDistanceSquared) = 0;
};
