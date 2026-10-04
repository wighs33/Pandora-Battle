#pragma once

#include "CoreMinimal.h"
#include "Common/InfoUiTypes.h"
#include "UObject/Interface.h"
#include "HudInputInterface.generated.h"

struct FInputActionValue;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class LABPROJECT_API UHudInputInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 플레이어 입력 중 화면이 처리할 명령의 계약.
 * 입력 컴포넌트는 HUD 클래스 대신 이 계약만 알고, 메뉴와 정보 창을 여닫는 일은 HUD가 맡는다.
 */
class LABPROJECT_API IHudInputInterface
{
	GENERATED_BODY()

public:
	// Public API ------------------------------------------------------------------------------------------------------
	virtual void OpenInfoUiFocused(EInfoUiSection Section) = 0;
	virtual void OnOpenSettingsMenuInputStarted(const FInputActionValue& InputValue) = 0;

	/** ESC 입력을 화면이 처리했으면 true. */
	virtual bool HandleEscapeInput() = 0;

	virtual void OnSelectPandoraInputStarted(const FInputActionValue& InputValue) = 0;
	virtual bool OnSelectPandoraInputEnded(const FInputActionValue& InputValue) = 0;
	virtual bool IsSelectPandoraUiOpen() const = 0;
	virtual void OnPandoraTreeInputStarted(const FInputActionValue& InputValue) = 0;

	/** 로비 화면을 다시 연다. 로비가 아닌 HUD는 무시한다. */
	virtual void OpenLobbyUi() {}

	/** 열린 화면이 게임플레이 입력을 막고 있는지. */
	virtual bool IsGameplayInputBlockedByUi() const = 0;
};
