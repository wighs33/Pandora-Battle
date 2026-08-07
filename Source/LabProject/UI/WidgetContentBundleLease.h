#pragma once

#include "CoreMinimal.h"
#include "Definition/UI/WidgetContentBundle.h"

class UContentDataSubsystem;
class UUiSubsystem;
class UWidgetClassDefinition;
struct FStreamableHandle;

/**
 * One screen-lifetime hold on a DA_Widget content bundle.
 *
 * FStreamableHandle already provides shared residency semantics, so each
 * consumer only keeps this lease while its screen needs the bundle. Dropping
 * the last lease releases the corresponding streamable handle without a
 * second token/reference-count registry.
 */
class LABPROJECT_API FWidgetContentBundleLease final
	: public TSharedFromThis<FWidgetContentBundleLease>
{
public:
	~FWidgetContentBundleLease();
	FWidgetContentBundleLease(const FWidgetContentBundleLease&) = delete;
	FWidgetContentBundleLease& operator=(const FWidgetContentBundleLease&) = delete;

	EWidgetContentBundleState GetState() const { return State; }
	bool IsReady() const { return State == EWidgetContentBundleState::Ready; }
	UWidgetClassDefinition* GetDefinition() const { return Definition.Get(); }

	/** Idempotent explicit release; resetting the shared pointer does this too. */
	void Release();

private:
	friend class UUiSubsystem;

	FWidgetContentBundleLease(
		EWidgetContentBundle InBundle,
		FSimpleDelegate InOnComplete);

	void Start(
		UWidgetClassDefinition* InDefinition,
		UContentDataSubsystem* ContentSubsystem);
	void MarkFailed();
	void HandlePreloadComplete();
	void QueueCompletion();
	void DispatchCompletion();

	TWeakObjectPtr<UWidgetClassDefinition> Definition;
	TSharedPtr<FStreamableHandle> StreamableHandle;
	TArray<FSoftObjectPath> ExpectedPaths;
	FSimpleDelegate OnComplete;
	EWidgetContentBundle Bundle = EWidgetContentBundle::Core;
	EWidgetContentBundleState State = EWidgetContentBundleState::Unloaded;
	bool bCompletionQueued = false;
};
