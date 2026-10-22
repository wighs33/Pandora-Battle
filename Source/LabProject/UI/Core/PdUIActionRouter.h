#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Input/CommonUIActionRouterBase.h"
#include "PdUIActionRouter.generated.h"

enum class EPdGameplayInputPolicy : uint8 { Allow, Block };

/** CommonUI 입력 정책을 적용한다. 채팅 입력란만 별도 어댑터로 지원한다. */
UCLASS()
class LABPROJECT_API UPdUIActionRouter : public UCommonUIActionRouterBase
{
	GENERATED_BODY()

public:
	// Engine Overrides ------------------------------------------------------------------------------------------------
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	virtual void Deinitialize() override;
	virtual ERouteUIInputResult ProcessInput(FKey Key, EInputEvent InputEvent) const override;

	// Public API ------------------------------------------------------------------------------------------------------
	void BeginChatInput(UWidget* InputWidget);
	void EndChatInput(const UWidget* InputWidget);
	bool IsGameplayInputBlocked() const;

	// 다른 화면이 없을 때의 입력 설정. 게임 중 기본 화면(UGameplayScreen)도 이 설정을 쓴다.
	FUIInputConfig MakeDefaultInputConfig() const;

protected:
	// Internal Helpers ------------------------------------------------------------------------------------------------
	virtual void SetActiveRoot(FActivatableTreeRootPtr NewActiveRoot) override;
	virtual void ApplyUIInputConfig(const FUIInputConfig& NewConfig, bool bForceRefresh) override;
	void ApplyDefaultInput();
	// 활성 화면이 없거나 게임 중 기본 화면뿐인지.
	bool IsDefaultScreenActive() const;

private:
	FTSTicker::FDelegateHandle PendingInputConfigTickerHandle;
	TWeakObjectPtr<UWidget> ChatInputWidget;
	EPdGameplayInputPolicy AppliedGameplayPolicy = EPdGameplayInputPolicy::Allow;
	mutable TSet<FKey> PressedKeys;
	mutable TSet<FKey> KeysAwaitingRelease;
};
