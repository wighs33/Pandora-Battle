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

	ExpectedPaths.Reset();
	for (const FSoftObjectPath& AssetPath : AssetPaths)
	{
		if (!AssetPath.IsNull())
		{
			ExpectedPaths.AddUnique(AssetPath);
		}
	}
	State = EState::Loading;
	TGuardValue<bool> StartingGuard(bStarting, true);
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
	// 엔진은 로드 완료를 이미 다음 프레임에 알리므로 그때는 바로 전달한다.
	// 시작하는 도중에 끝난 경우(빈 목록·시작 실패)만 호출자가 lease를 저장할 때까지 미룬다.
	if (bStarting)
	{
		QueueCompletion();
	}
	else
	{
		DispatchCompletion();
	}
}

void FContentLease::QueueCompletion()
{
	if (bCompletionQueued || !OnComplete.IsBound())
	{
		return;
	}
	// 시작하는 도중에 끝났거나 실패로 표시된 lease도 호출자가 저장한 다음에 완료 콜백을 받도록 지연한다.
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
