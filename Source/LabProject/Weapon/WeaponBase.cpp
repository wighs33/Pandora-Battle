#include "Weapon/WeaponBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Character/PdCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Item/ItemDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "PlayerComponent/CombatComponent.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(WeaponBase)

DEFINE_LOG_CATEGORY_STATIC(LogWeaponBase, Log, All);

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(SceneRoot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeaponBase, SourceItemDefinition);
}

void AWeaponBase::RequestServerApplyDamage(AActor* TargetActor)
{
	if (HasAuthority())
	{
		ApplyDamageToTarget(TargetActor);
		return;
	}

	ServerApplyDamage(TargetActor);
}

void AWeaponBase::SetBeginOverlapEnabled(bool bEnabled)
{
	UBoxComponent* CollisionBox = GetCollisionBox();
	if (!CollisionBox)
	{
		return;
	}

	if (bEnabled)
	{
		HitActorsInCurrentAttack.Reset();
	}

	CollisionBox->SetGenerateOverlapEvents(bEnabled);
	CollisionBox->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

bool AWeaponBase::PlayWeaponMontage(FName StartingSection)
{
	if (!WeaponMesh)
	{
		UE_LOG(LogWeaponBase, Warning, TEXT("%s failed to play weapon montage: WeaponMesh is null."), *GetName());
		return false;
	}

	UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance();
	UAnimMontage* Montage = GetConfiguredWeaponMontage();
	if (!AnimInstance || !Montage)
	{
		UE_LOG(
			LogWeaponBase,
			Warning,
			TEXT("%s failed to play weapon montage: AnimInstance=%s Montage=%s"),
			*GetName(),
			AnimInstance ? TEXT("Valid") : TEXT("Null"),
			*GetNameSafe(Montage));
		return false;
	}

	if (AnimInstance->Montage_Play(Montage) <= 0.0f)
	{
		return false;
	}

	if (!StartingSection.IsNone())
	{
		AnimInstance->Montage_JumpToSection(StartingSection, Montage);
	}

	return true;
}

bool AWeaponBase::JumpToWeaponMontageSectionAndResume(FName SectionName)
{
	if (!WeaponMesh)
	{
		UE_LOG(LogWeaponBase, Warning, TEXT("%s failed to jump weapon montage section: WeaponMesh is null."), *GetName());
		return false;
	}

	UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance();
	UAnimMontage* Montage = GetConfiguredWeaponMontage();
	if (!AnimInstance || !Montage)
	{
		UE_LOG(
			LogWeaponBase,
			Warning,
			TEXT("%s failed to jump weapon montage section: AnimInstance=%s Montage=%s"),
			*GetName(),
			AnimInstance ? TEXT("Valid") : TEXT("Null"),
			*GetNameSafe(Montage));
		return false;
	}

	if (!SectionName.IsNone())
	{
		AnimInstance->Montage_JumpToSection(SectionName, Montage);
	}

	AnimInstance->Montage_Resume(Montage);
	return true;
}

void AWeaponBase::StopWeaponMontage(float BlendOutTime)
{
	if (!WeaponMesh)
	{
		return;
	}

	UAnimInstance* AnimInstance = WeaponMesh->GetAnimInstance();
	UAnimMontage* Montage = GetConfiguredWeaponMontage();
	if (!AnimInstance || !Montage)
	{
		return;
	}

	AnimInstance->Montage_Stop(BlendOutTime, Montage);
}

void AWeaponBase::InitializeFromItemDefinition(const UItemDefinition* InItemDefinition)
{
	SourceItemDefinition = const_cast<UItemDefinition*>(InItemDefinition);
}

bool AWeaponBase::SupportsAimInput() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Aim.bSupportsInput;
	}

	return false;
}

FGameplayTag AWeaponBase::GetAimCrosshairWidgetTag() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Aim.CrosshairWidgetTag;
	}

	return FGameplayTag();
}

const FWeaponAimCameraSettings& AWeaponBase::GetAimCameraSettings() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Aim.CameraSettings;
	}

	static const FWeaponAimCameraSettings DefaultAimCameraSettings;
	return DefaultAimCameraSettings;
}

bool AWeaponBase::HandleAimStart(APdPlayer* PlayerCharacter)
{
	if (!SupportsAimInput() || !PlayerCharacter)
	{
		return false;
	}

	PlayerCharacter->SetWeaponAimActive(true, GetAimCameraSettings());
	return true;
}

void AWeaponBase::HandleAimEnd(APdPlayer* PlayerCharacter)
{
	if (!SupportsAimInput() || !PlayerCharacter)
	{
		return;
	}

	PlayerCharacter->SetWeaponAimActive(false, GetAimCameraSettings());
}

bool AWeaponBase::HandlePrimaryAttack(APdPlayer* PlayerCharacter)
{
	static_cast<void>(PlayerCharacter);
	return false;
}

bool AWeaponBase::SupportsAutomaticFire() const
{
	return false;
}

float AWeaponBase::GetAutomaticFireInterval() const
{
	return 0.0f;
}

bool AWeaponBase::OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter)
{
	static_cast<void>(NotifyName);
	static_cast<void>(PlayerCharacter);
	return false;
}

const UItemDefinition* AWeaponBase::GetSourceItemDefinition() const
{
	return SourceItemDefinition.Get();
}

bool AWeaponBase::TryGetOwnerMeshSocketLocation(const APdPlayer* PlayerCharacter, FName SocketName, FVector& OutLocation) const
{
	const USkeletalMeshComponent* CharacterMesh = PlayerCharacter ? PlayerCharacter->GetMesh() : nullptr;
	if (!CharacterMesh || SocketName.IsNone() || !CharacterMesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	OutLocation = CharacterMesh->GetSocketLocation(SocketName);
	return true;
}

bool AWeaponBase::ResolveServerAimViewPoint(
	const APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation,
	const FVector& RequestedViewDirection,
	FVector& OutViewLocation,
	FVector& OutViewDirection) const
{
	if (!PlayerCharacter)
	{
		return false;
	}

	const FVector RequestedDirection = RequestedViewDirection.GetSafeNormal();
	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const float MaxAcceptedViewDistance = ItemDefinition ? ItemDefinition->WeaponData.Aim.MaxAcceptedServerViewDistance : 0.0f;
	if (!RequestedDirection.IsNearlyZero()
		&& MaxAcceptedViewDistance > 0.0f
		&& FVector::DistSquared(RequestedViewLocation, PlayerCharacter->GetActorLocation()) <= FMath::Square(MaxAcceptedViewDistance))
	{
		OutViewLocation = RequestedViewLocation;
		OutViewDirection = RequestedDirection;
		return true;
	}

	return PlayerCharacter->GetWeaponAimViewPoint(OutViewLocation, OutViewDirection);
}

bool AWeaponBase::TryGetWeaponAimTargetLocation(
	const APdPlayer* PlayerCharacter,
	float TraceRange,
	const TArray<AActor*>& ActorsToIgnore,
	FVector& OutTargetLocation) const
{
	FVector ViewTraceStart = FVector::ZeroVector;
	FVector ViewTraceDirection = FVector::ZeroVector;
	if (!PlayerCharacter || TraceRange <= 0.0f || !PlayerCharacter->GetWeaponAimViewPoint(ViewTraceStart, ViewTraceDirection))
	{
		return false;
	}

	const FVector SafeViewTraceDirection = ViewTraceDirection.GetSafeNormal();
	if (SafeViewTraceDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector ViewTraceEnd = ViewTraceStart + (SafeViewTraceDirection * TraceRange);

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));

	FHitResult ViewHitResult;
	const bool bViewHit = UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		ViewTraceStart,
		ViewTraceEnd,
		ObjectTypes,
		false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		ViewHitResult,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		5.0f);

	OutTargetLocation = bViewHit ? ViewHitResult.Location : ViewTraceEnd;
	return true;
}

UBoxComponent* AWeaponBase::GetCollisionBox() const
{
	return nullptr;
}

void AWeaponBase::InitializeCollisionBox(UBoxComponent* CollisionBox)
{
	if (!CollisionBox)
	{
		return;
	}

	CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionBox->SetGenerateOverlapEvents(false);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AWeaponBase::OnCollisionBoxBeginOverlap);
}

UAnimMontage* AWeaponBase::GetConfiguredWeaponMontage() const
{
	return nullptr;
}

FName AWeaponBase::GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const
{
	return NAME_None;
}

void AWeaponBase::ServerApplyDamage_Implementation(AActor* TargetActor)
{
	ApplyDamageToTarget(TargetActor);
}

APdCharacterBase* AWeaponBase::GetOwningCharacter() const
{
	if (APdCharacterBase* OwnerCharacter = Cast<APdCharacterBase>(GetOwner()))
	{
		return OwnerCharacter;
	}

	return Cast<APdCharacterBase>(GetAttachParentActor());
}

bool AWeaponBase::CanProcessOverlapWith(AActor* OtherActor, UPrimitiveComponent* OtherComp) const
{
	if (!Cast<APdCharacterBase>(OtherActor) || OtherActor == GetAttachParentActor() || !OtherComp)
	{
		return false;
	}

	if (HitActorsInCurrentAttack.Contains(OtherActor))
	{
		return false;
	}

	const APdCharacterBase* OwnerCharacter = GetOwningCharacter();
	return OwnerCharacter && OwnerCharacter->IsLocallyControlled();
}

bool AWeaponBase::TryTraceOverlapTarget(UPrimitiveComponent* OtherComp, FHitResult& OutHitResult) const
{
	const UBoxComponent* CollisionBox = GetCollisionBox();
	if (!CollisionBox || !OtherComp)
	{
		return false;
	}

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	return UKismetSystemLibrary::BoxTraceSingleForObjects(
		this,
		CollisionBox->GetComponentLocation(),
		OtherComp->GetComponentLocation(),
		CollisionBox->GetComponentScale() * 0.5f,
		CollisionBox->GetComponentRotation(),
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

void AWeaponBase::DebugSuccessfulHit(const FHitResult& HitResult) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UKismetSystemLibrary::PrintString(this, TEXT("success"), true, true, FLinearColor(0.f, 0.66f, 1.f, 1.f), 2.f);
	DrawDebugSphere(GetWorld(), HitResult.Location, 5.f, 12, FColor::Red, false, 5.f, 0, 3.f);
#else
	static_cast<void>(HitResult);
#endif
}

void AWeaponBase::OnCollisionBoxBeginOverlap(
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

	if (!CanProcessOverlapWith(OtherActor, OtherComp))
	{
		return;
	}

	FHitResult HitResult;
	if (!TryTraceOverlapTarget(OtherComp, HitResult))
	{
		return;
	}

	HitActorsInCurrentAttack.Add(OtherActor);
	DebugSuccessfulHit(HitResult);
	RequestServerApplyDamage(OtherActor);
}

void AWeaponBase::ApplyDamageToTarget(AActor* TargetActor)
{
	APdCharacterBase* SourceCharacter = GetOwningCharacter();
	APdCharacterBase* TargetCharacter = Cast<APdCharacterBase>(TargetActor);
	if (!SourceCharacter || !TargetCharacter || SourceCharacter == TargetCharacter)
	{
		return;
	}

	UCombatComponent* CombatComponent = SourceCharacter->GetCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	CombatComponent->ApplyWeaponDamageToTarget(TargetCharacter);
}
