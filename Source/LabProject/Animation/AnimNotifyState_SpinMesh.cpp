#include "Animation/AnimNotifyState_SpinMesh.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_SpinMesh)

// 노티파이 객체는 같은 몽타주를 재생하는 모든 캐릭터가 함께 쓰므로, 진행 정도는 상태로 들고 있지 않고 몽타주 재생 위치에서 매번 계산한다.
void UAnimNotifyState_SpinMesh::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	const FAnimNotifyEvent* Notify = EventReference.GetNotify();
	const UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	const UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	if (!Notify || !AnimInstance || !Montage || Notify->GetDuration() <= 0.0f)
	{
		return;
	}
	const float Elapsed = AnimInstance->Montage_GetPosition(Montage) - Notify->GetTriggerTime();
	ApplySpin(*MeshComp, FMath::Clamp(Elapsed / Notify->GetDuration(), 0.0f, 1.0f));
}

void UAnimNotifyState_SpinMesh::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (MeshComp)
	{
		ApplySpin(*MeshComp, 0.0f);
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_SpinMesh::GetNotifyName_Implementation() const
{
	return TEXT("Spin Mesh");
}

// 기본 회전은 캐릭터 클래스 기본값의 메시 회전이다. 실행 중에 바뀐 값을 기준으로 삼으면 끊긴 회전이 다음 회전에 쌓인다.
void UAnimNotifyState_SpinMesh::ApplySpin(USkeletalMeshComponent& MeshComp, const float Alpha) const
{
	ACharacter* Character = Cast<ACharacter>(MeshComp.GetOwner());
	const ACharacter* Defaults = Character ? Character->GetClass()->GetDefaultObject<ACharacter>() : nullptr;
	const USkeletalMeshComponent* DefaultMesh = Defaults ? Defaults->GetMesh() : nullptr;
	if (!DefaultMesh || Character->GetMesh() != &MeshComp)
	{
		return;
	}
	FRotator Rotation = DefaultMesh->GetRelativeRotation();
	Rotation.Yaw += 360.0f * Turns * Alpha;
	Character->CacheInitialMeshOffset(Character->GetBaseTranslationOffset(), Rotation);
	MeshComp.SetRelativeRotation(Rotation);
}
