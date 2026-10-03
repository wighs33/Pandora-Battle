#pragma once

#include "CoreMinimal.h"

class UContentDataSubsystem;
struct FStreamableHandle;

/** 소비자가 보유하는 동안 요청한 콘텐츠의 로드 핸들을 유지한다. */
class LABPROJECT_API FContentLease final : public TSharedFromThis<FContentLease>
{
public:
	explicit FContentLease(FSimpleDelegate InOnComplete = FSimpleDelegate());
	~FContentLease();
	FContentLease(const FContentLease&) = delete;
	FContentLease& operator=(const FContentLease&) = delete;

	bool IsReady() const { return State == EState::Ready; }
	bool IsLoading() const { return State == EState::Loading; }
	bool HasFailed() const { return State == EState::Failed; }

	/**
	 * 경로를 결정하는 선행 로드가 있는 경우에도 같은 lease를 반환하고 나중에 시작할 수 있다.
	 * 비어 있는 경로와 중복 경로는 로드할 것이 없으므로 뺀다.
	 */
	void Start(const TArray<FSoftObjectPath>& AssetPaths, UContentDataSubsystem* ContentSubsystem);
	/** 선행 로드 실패도 완료 콜백으로 전달한다. */
	void MarkFailed();
	/** 소유자의 종료 시 외부 참조가 남아 있어도 로드와 콜백을 취소한다. */
	void Release();

private:
	enum class EState : uint8 { Unloaded, Loading, Ready, Failed };
	void HandlePreloadComplete();
	void QueueCompletion();
	void DispatchCompletion();

	TSharedPtr<FStreamableHandle> StreamableHandle;
	TArray<FSoftObjectPath> ExpectedPaths;
	FSimpleDelegate OnComplete;
	EState State = EState::Unloaded;
	bool bCompletionQueued = false;
	bool bStarting = false;
};
