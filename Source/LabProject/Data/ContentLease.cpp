#include "Data/ContentLease.h"

#include "Containers/Ticker.h"
#include "Data/ContentDataSubsystem.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogContentLease, Log, All);

FContentLease::FContentLease(FSimpleDelegate InOnComplete)
	: OnComplete(MoveTemp(InOnComplete))
{
}

FContentLease::~FContentLease()
{
	Release();
}

void FContentLease::Start(
	const TArray<FSoftObjectPath>& AssetPaths,
	UContentDataSubsystem* ContentSubsystem)
{
	if (State != EState::Unloaded)
	{
		return;
	}

	if (!ContentSubsystem)
	{
		UE_LOG(LogContentLease, Error, TEXT("Content preload could not start: ContentDataSubsystem is unavailable."));
		MarkFailed();
		return;
	}

	ExpectedPaths = AssetPaths;
	State = EState::Loading;
	if (ExpectedPaths.IsEmpty())
	{
		HandlePreloadComplete();
		return;
	}

	const TWeakPtr<FContentLease> WeakLease = AsShared();
	TSharedPtr<FStreamableHandle> NewHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			ExpectedPaths,
			FSimpleDelegate::CreateLambda(
				[WeakLease]()
				{
					if (const TSharedPtr<FContentLease> This = WeakLease.Pin())
					{
						This->HandlePreloadComplete();
					}
				}));

	if (State != EState::Unloaded)
	{
		StreamableHandle = MoveTemp(NewHandle);
	}
	else if (NewHandle.IsValid())
	{
		NewHandle->CancelHandle();
		NewHandle->ReleaseHandle();
	}
	if (!StreamableHandle.IsValid()
		&& State == EState::Loading)
	{
		MarkFailed();
	}
}

void FContentLease::MarkFailed()
{
	if (State == EState::Ready
		|| State == EState::Failed)
	{
		return;
	}
	State = EState::Failed;
	QueueCompletion();
}

void FContentLease::HandlePreloadComplete()
{
	if (State != EState::Loading)
	{
		return;
	}

	bool bResolvedAllAssets = true;
	for (const FSoftObjectPath& ExpectedPath : ExpectedPaths)
	{
		if (!ExpectedPath.ResolveObject())
		{
			bResolvedAllAssets = false;
			UE_LOG(
				LogContentLease,
				Error,
				TEXT("Content preload did not resolve '%s'."),
				*ExpectedPath.ToString());
		}
	}
	State = bResolvedAllAssets
		? EState::Ready
		: EState::Failed;
	QueueCompletion();
}

void FContentLease::QueueCompletion()
{
	if (bCompletionQueued || !OnComplete.IsBound())
	{
		return;
	}
	// 이미 로드된 콘텐츠도 호출자가 lease를 저장한 다음 완료 콜백을 받도록 지연한다.
	bCompletionQueued = true;
	const TWeakPtr<FContentLease> WeakLease = AsShared();
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakLease](float)
			{
				if (const TSharedPtr<FContentLease> This = WeakLease.Pin())
				{
					This->DispatchCompletion();
				}
				return false;
			}));
}

void FContentLease::DispatchCompletion()
{
	bCompletionQueued = false;
	if (State != EState::Ready
		&& State != EState::Failed)
	{
		return;
	}
	FSimpleDelegate Completion = MoveTemp(OnComplete);
	Completion.ExecuteIfBound();
}

void FContentLease::Release()
{
	if (StreamableHandle.IsValid())
	{
		StreamableHandle->CancelHandle();
		StreamableHandle->ReleaseHandle();
		StreamableHandle.Reset();
	}
	ExpectedPaths.Reset();
	OnComplete.Unbind();
	State = EState::Unloaded;
}
