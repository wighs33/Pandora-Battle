#pragma once

#include "CoreMinimal.h"

class UAchievementSubsystem;
class UGameInstance;

/**
 * 업적 서브시스템 알림 구독 하나를 보관한다.
 *
 * Steam 업적 상태가 바뀔 때 콜백을 부르고, 원하면 업적 아이콘 로딩이 끝날 때도 같은 콜백을 부른다.
 * 구독한 서브시스템을 기억해 두므로 해제할 때 다시 찾지 않는다. 소유자는 종료 시 Reset으로 구독을 해제한다.
 */
class LABPROJECT_API FAchievementNotificationSubscription final
{
public:
	FAchievementNotificationSubscription() = default;
	FAchievementNotificationSubscription(const FAchievementNotificationSubscription&) = delete;
	FAchievementNotificationSubscription& operator=(const FAchievementNotificationSubscription&) = delete;

	/** 이전 구독을 끊고 게임 인스턴스의 업적 서브시스템을 구독한다. 구독한 서브시스템을 돌려주고, 없으면 nullptr이다. */
	UAchievementSubsystem* Subscribe(UGameInstance* GameInstance, const FSimpleDelegate& OnChanged, bool bIncludePresentationReady = false);

	void Reset();

	bool IsSubscribed() const { return StateChangedHandle.IsValid(); }

private:
	TWeakObjectPtr<UAchievementSubsystem> Subsystem;
	FDelegateHandle StateChangedHandle;
	FDelegateHandle PresentationReadyHandle;
};
