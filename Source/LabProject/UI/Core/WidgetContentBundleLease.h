#pragma once

#include "CoreMinimal.h"
#include "Definition/UI/WidgetContentBundle.h"

class UContentDataSubsystem;
class UUiSubsystem;
class UWidgetClassDefinition;
struct FStreamableHandle;

/**
 * 화면이 열려 있는 동안 DA_Widget 콘텐츠 번들의 로드를 유지한다.
 *
 * FStreamableHandle이 공유 로드 수명을 관리하므로 각 사용자는 화면에서 번들이 필요한 동안만
 * 이 참조를 유지한다. 마지막 참조가 해제되면 별도 토큰·참조 횟수 관리 없이
 * 해당 스트리밍 핸들이 해제된다.
 */
class LABPROJECT_API FWidgetContentBundleLease final
	: public TSharedFromThis<FWidgetContentBundleLease>
{

public:
	// Public API ------------------------------------------------------------------------------------------------------
	~FWidgetContentBundleLease();
	FWidgetContentBundleLease(const FWidgetContentBundleLease&) = delete;
	FWidgetContentBundleLease& operator=(const FWidgetContentBundleLease&) = delete;

	EWidgetContentBundleState GetState() const { return State; }
	bool IsReady() const { return State == EWidgetContentBundleState::Ready; }
	UWidgetClassDefinition* GetDefinition() const { return Definition.Get(); }

	/** Idempotent explicit release; resetting the shared pointer does this too. */
	void Release();

private:
	// Event Handlers --------------------------------------------------------------------------------------------------
	void HandlePreloadComplete();

	// Internal Helpers ------------------------------------------------------------------------------------------------
	FWidgetContentBundleLease(
		EWidgetContentBundle InBundle,
		FSimpleDelegate InOnComplete);

	void Start(
		UWidgetClassDefinition* InDefinition,
		UContentDataSubsystem* ContentSubsystem);
	void MarkFailed();
	void QueueCompletion();
	void DispatchCompletion();

private:
	friend class UUiSubsystem;

	TWeakObjectPtr<UWidgetClassDefinition> Definition;
	TSharedPtr<FStreamableHandle> StreamableHandle;
	TArray<FSoftObjectPath> ExpectedPaths;
	FSimpleDelegate OnComplete;
	EWidgetContentBundle Bundle = EWidgetContentBundle::Core;
	EWidgetContentBundleState State = EWidgetContentBundleState::Unloaded;
	bool bCompletionQueued = false;
};
