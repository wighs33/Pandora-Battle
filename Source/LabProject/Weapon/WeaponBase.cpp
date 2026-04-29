#include "Weapon/WeaponBase.h"

#include "DrawDebugHelpers.h"
#include "Character/PdCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PlayerComponent/CombatComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponBase)

/** 무기 기본 상태를 초기화합니다. */
AWeaponBase::AWeaponBase()
{
	// =================================================================================================================
	// === 기본 설정

	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	// =================================================================================================================
	// === 루트 설정

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// =================================================================================================================
	// === 메시 설정

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// =================================================================================================================
	// === 충돌 박스 설정

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(WeaponMesh);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->OnComponentBeginOverlap.AddDynamic(this, &AWeaponBase::HandleBoxBeginOverlap);
}

/** 서버에 데미지 적용을 요청합니다. */
void AWeaponBase::RequestServerApplyDamage(AActor* TargetActor)
{
	if (HasAuthority())
	{
		ApplyDamageToTarget(TargetActor);
		return;
	}

	ServerApplyDamage(TargetActor);
}

/** BeginOverlap 판정을 켜거나 끕니다. */
void AWeaponBase::SetBeginOverlapEnabled(bool bEnabled)
{
	if (!Box)
	{
		return;
	}

	// =================================================================================================================
	// === 새 공격 시작 시 히트 목록 초기화

	if (bEnabled)
	{
		HitActorsInCurrentAttack.Reset();
	}

	Box->SetGenerateOverlapEvents(bEnabled);
	Box->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

/** 서버에서 데미지를 적용합니다. */
void AWeaponBase::ServerApplyDamage_Implementation(AActor* TargetActor)
{
	ApplyDamageToTarget(TargetActor);
}

/** 소유 캐릭터를 반환합니다. */
APdCharacterBase* AWeaponBase::GetOwningCharacter() const
{
	if (APdCharacterBase* OwnerCharacter = Cast<APdCharacterBase>(GetOwner()))
	{
		return OwnerCharacter;
	}

	return Cast<APdCharacterBase>(GetAttachParentActor());
}

/** 현재 오버랩을 처리할 수 있는지 반환합니다. */
bool AWeaponBase::CanProcessOverlapWith(AActor* OtherActor, UPrimitiveComponent* OtherComp) const
{
	// =================================================================================================================
	// === 기본 조건 검사

	if (!Cast<APdCharacterBase>(OtherActor) || OtherActor == GetAttachParentActor() || !OtherComp)
	{
		return false;
	}

	// =================================================================================================================
	// === 중복 타격 방지

	if (HitActorsInCurrentAttack.Contains(OtherActor))
	{
		return false;
	}

	// =================================================================================================================
	// === 로컬 제어 캐릭터만 처리

	const APdCharacterBase* OwnerCharacter = GetOwningCharacter();
	return OwnerCharacter && OwnerCharacter->IsLocallyControlled();
}

/** 오버랩 대상을 BoxTrace로 다시 확인합니다. */
bool AWeaponBase::TryTraceOverlapTarget(UPrimitiveComponent* OtherComp, FHitResult& OutHitResult) const
{
	if (!Box || !OtherComp)
	{
		return false;
	}

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	return UKismetSystemLibrary::BoxTraceSingleForObjects(
		this,
		Box->GetComponentLocation(),
		OtherComp->GetComponentLocation(),
		Box->GetComponentScale() * 0.5f,
		Box->GetComponentRotation(),
		ObjectTypes,
		false,
		{},
		EDrawDebugTrace::None,
		OutHitResult,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		5.f);
}

/** 성공한 타격을 디버그 출력합니다. */
void AWeaponBase::DebugSuccessfulHit(const FHitResult& HitResult) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UKismetSystemLibrary::PrintString(this, TEXT("success"), true, true, FLinearColor(0.f, 0.66f, 1.f, 1.f), 2.f);
	DrawDebugSphere(GetWorld(), HitResult.Location, 5.f, 12, FColor::Red, false, 5.f, 0, 3.f);
#else
	static_cast<void>(HitResult);
#endif
}

/** 박스 BeginOverlap을 처리합니다. */
void AWeaponBase::HandleBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	// =================================================================================================================
	// === 오버랩 처리 가능 여부 검사

	if (!CanProcessOverlapWith(OtherActor, OtherComp))
	{
		return;
	}

	// =================================================================================================================
	// === 트레이스로 실제 타격 확인

	FHitResult HitResult;
	if (!TryTraceOverlapTarget(OtherComp, HitResult))
	{
		return;
	}

	// =================================================================================================================
	// === 히트 기록 및 데미지 요청

	HitActorsInCurrentAttack.Add(OtherActor);
	DebugSuccessfulHit(HitResult);
	RequestServerApplyDamage(OtherActor);
}

/** 대상에게 실제 데미지를 적용합니다. */
void AWeaponBase::ApplyDamageToTarget(AActor* TargetActor)
{
	// =================================================================================================================
	// === 공격 주체와 대상 확인

	APdCharacterBase* SourceCharacter = GetOwningCharacter();
	APdCharacterBase* TargetCharacter = Cast<APdCharacterBase>(TargetActor);
	if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter)
	{
		return;
	}

	// =================================================================================================================
	// === 전투 컴포넌트로 위임

	UCombatComponent* CombatComponent = SourceCharacter->GetCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	CombatComponent->ApplyWeaponDamageToTarget(TargetCharacter);
}