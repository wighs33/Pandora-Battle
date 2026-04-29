#include "Character/PdPlayer.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Mode/PdPlayerState.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdPlayer)

/** 플레이어 기본 상태를 초기화합니다. */
APdPlayer::APdPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================
	// === 상호작용 박스 설정

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(GetRootComponent());
	InteractionBox->SetBoxExtent(FVector(50.0f, 50.0f, 100.0f));
	InteractionBox->SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));
	InteractionBox->SetRelativeRotation(FRotator::ZeroRotator);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionBox->SetGenerateOverlapEvents(true);
	InteractionBox->SetCanEverAffectNavigation(false);
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &APdPlayer::HandleInteractionBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &APdPlayer::HandleInteractionBoxEndOverlap);

	// =================================================================================================================
	// === 카메라 붐 설정

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// =================================================================================================================
	// === 추적 카메라 설정

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

/** PlayerState 기준 ASC를 반환합니다. */
UAbilitySystemComponent* APdPlayer::GetAbilitySystemComponent() const
{
	if (const APdPlayerState* PdPlayerState = GetPdPlayerState())
	{
		return PdPlayerState->GetAbilitySystemComponent();
	}

	return nullptr;
}

/** ASC 소유 액터를 반환합니다. */
AActor* APdPlayer::GetAbilitySystemOwnerActor() const
{
	return GetPdPlayerState();
}

/** PlayerState를 프로젝트 타입으로 반환합니다. */
APdPlayerState* APdPlayer::GetPdPlayerState() const
{
	return GetPlayerState<APdPlayerState>();
}

/** 현재 상호작용 대상이 있는지 반환합니다. */
bool APdPlayer::HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const
{
	OutCurrentInteractActors = CurrentInteractActors;
	return OutCurrentInteractActors.Num() > 0;
}

bool APdPlayer::CanInteractWithActor(AActor* InteractableActor) const
{
	TScriptInterface<IInteractableInterface> InteractableEntry;
	if (!TryMakeInteractableEntry(InteractableActor, InteractableEntry))
	{
		return false;
	}

	if (!IsValid(InteractionBox) || !InteractionBox->IsOverlappingActor(InteractableActor))
	{
		return false;
	}

	return true;
}

/** 액터를 상호작용 엔트리로 변환합니다. */
bool APdPlayer::TryMakeInteractableEntry(AActor* OtherActor, TScriptInterface<IInteractableInterface>& OutInteractableActor) const
{
	// =================================================================================================================
	// === 유효성 검사

	if (!IsValid(OtherActor) || OtherActor == this || !OtherActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return false;
	}

	// =================================================================================================================
	// === 인터페이스 엔트리 구성

	OutInteractableActor.SetObject(OtherActor);
	OutInteractableActor.SetInterface(Cast<IInteractableInterface>(OtherActor));
	return true;
}

/** 상호작용 박스 진입을 처리합니다. */
void APdPlayer::HandleInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	// =================================================================================================================
	// === 상호작용 엔트리 생성

	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		return;
	}

	// =================================================================================================================
	// === 중복 추적 방지

	const bool bAlreadyTracked = CurrentInteractActors.ContainsByPredicate(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});

	if (bAlreadyTracked)
	{
		return;
	}

	// =================================================================================================================
	// === 목록 추가

	CurrentInteractActors.Add(InteractableActor);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// 디버그 안내 문구입니다.
	UKismetSystemLibrary::PrintString(this, TEXT("Interact (X)"), true, true, FLinearColor(0.0f, 0.66f, 1.0f), 2.0f);
#endif
}

/** 상호작용 박스 이탈을 처리합니다. */
void APdPlayer::HandleInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);

	// =================================================================================================================
	// === 상호작용 엔트리 확인

	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		return;
	}

	// =================================================================================================================
	// === 목록 제거

	CurrentInteractActors.RemoveAll(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});
}
