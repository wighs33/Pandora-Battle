#include "UI/WidgetContentBundleLease.h"

#include "Containers/Ticker.h"
#include "Data/ContentDataSubsystem.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetContentBundleLease, Log, All);

FWidgetContentBundleLease::FWidgetContentBundleLease(
	const EWidgetContentBundle InBundle,
	FSimpleDelegate InOnComplete)
	: OnComplete(MoveTemp(InOnComplete))
	, Bundle(InBundle)
{
}

FWidgetContentBundleLease::~FWidgetContentBundleLease()
{
	Release();
}

void FWidgetContentBundleLease::Start(
	UWidgetClassDefinition* InDefinition,
	UContentDataSubsystem* ContentSubsystem)
{
	if (State != EWidgetContentBundleState::Unloaded)
	{
		return;
	}

	Definition = InDefinition;
	if (!IsValid(InDefinition) || !ContentSubsystem)
	{
		UE_LOG(
			LogWidgetContentBundleLease,
			Error,
			TEXT("Widget content bundle %d could not start because its definition or ContentDataSubsystem is unavailable."),
			static_cast<int32>(Bundle));
		MarkFailed();
		return;
	}

	ExpectedPaths.Reset();
	InDefinition->GetRuntimePreloadAssetPaths(Bundle, ExpectedPaths);
	State = EWidgetContentBundleState::Loading;
	if (ExpectedPaths.IsEmpty())
	{
		HandlePreloadComplete();
		return;
	}

	const TWeakPtr<FWidgetContentBundleLease> WeakLease = AsShared();
	TSharedPtr<FStreamableHandle> NewHandle =
		ContentSubsystem->PreloadSoftObjectPathsAsync(
			ExpectedPaths,
			FSimpleDelegate::CreateLambda(
				[WeakLease]()
				{
					if (const TSharedPtr<FWidgetContentBundleLease> This = WeakLease.Pin())
					{
						This->HandlePreloadComplete();
					}
				}));

	if (State != EWidgetContentBundleState::Unloaded)
	{
		StreamableHandle = MoveTemp(NewHandle);
	}
	else if (NewHandle.IsValid())
	{
		NewHandle->CancelHandle();
		NewHandle->ReleaseHandle();
	}
	if (!StreamableHandle.IsValid()
		&& State == EWidgetContentBundleState::Loading)
	{
		MarkFailed();
	}
}

void FWidgetContentBundleLease::MarkFailed()
{
	if (State == EWidgetContentBundleState::Ready
		|| State == EWidgetContentBundleState::Failed)
	{
		return;
	}
	State = EWidgetContentBundleState::Failed;
	QueueCompletion();
}

void FWidgetContentBundleLease::HandlePreloadComplete()
{
	if (State != EWidgetContentBundleState::Loading)
	{
		return;
	}

	bool bResolvedAllAssets = Definition.IsValid();
	for (const FSoftObjectPath& ExpectedPath : ExpectedPaths)
	{
		if (!ExpectedPath.ResolveObject())
		{
			bResolvedAllAssets = false;
			UE_LOG(
				LogWidgetContentBundleLease,
				Error,
				TEXT("Widget content bundle %d for '%s' did not resolve '%s'."),
				static_cast<int32>(Bundle),
				*GetNameSafe(Definition.Get()),
				*ExpectedPath.ToString());
		}
	}
	State = bResolvedAllAssets
		? EWidgetContentBundleState::Ready
		: EWidgetContentBundleState::Failed;
	QueueCompletion();
}

void FWidgetContentBundleLease::QueueCompletion()
{
	if (bCompletionQueued || !OnComplete.IsBound())
	{
		return;
	}
	bCompletionQueued = true;
	const TWeakPtr<FWidgetContentBundleLease> WeakLease = AsShared();
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakLease](float)
			{
				if (const TSharedPtr<FWidgetContentBundleLease> This = WeakLease.Pin())
				{
					This->DispatchCompletion();
				}
				return false;
			}));
}

void FWidgetContentBundleLease::DispatchCompletion()
{
	bCompletionQueued = false;
	if (State != EWidgetContentBundleState::Ready
		&& State != EWidgetContentBundleState::Failed)
	{
		return;
	}
	FSimpleDelegate Completion = MoveTemp(OnComplete);
	Completion.ExecuteIfBound();
}

void FWidgetContentBundleLease::Release()
{
	if (StreamableHandle.IsValid())
	{
		StreamableHandle->CancelHandle();
		StreamableHandle->ReleaseHandle();
		StreamableHandle.Reset();
	}
	ExpectedPaths.Reset();
	Definition.Reset();
	OnComplete.Unbind();
	State = EWidgetContentBundleState::Unloaded;
}
