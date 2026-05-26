#include "Weapon/WeaponBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/PdPlayer.h"
#include "Character/PdCharacterBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Item/ItemDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "PlayerComponent/CombatComponent.h"
#include "TimerManager.h"
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

	AttackTraceStart = CreateDefaultSubobject<USceneComponent>(TEXT("TraceStart"));
	AttackTraceStart->SetupAttachment(WeaponMesh);
	AttackTraceStart->bEditableWhenInherited = true;

	AttackTraceEnd = CreateDefaultSubobject<USceneComponent>(TEXT("TraceEnd"));
	AttackTraceEnd->SetupAttachment(WeaponMesh);
	AttackTraceEnd->bEditableWhenInherited = true;
}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(AWeaponBase, SourceItemDefinition, Params);
}

void AWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAttackTrace();
	Super::EndPlay(EndPlayReason);
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
	if (bEnabled)
	{
		StartAttackTrace();
	}
	else
	{
		StopAttackTrace();
	}
}

void AWeaponBase::StartAttackTrace()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bAttackTraceActive)
	{
		return;
	}

	bAttackTraceActive = true;
	HitActorsInCurrentAttack.Reset();
	PerformAttackTrace();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AttackTraceTimerHandle,
			this,
			&ThisClass::PerformAttackTrace,
			FMath::Max(AttackTraceInterval, UE_SMALL_NUMBER),
			true);
	}
}

void AWeaponBase::StopAttackTrace()
{
	bAttackTraceActive = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTraceTimerHandle);
	}

	AttackTraceTimerHandle.Invalidate();
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
	if (HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AWeaponBase, SourceItemDefinition, this);
	}
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

bool AWeaponBase::CanDamageTracedActor(AActor* HitActor) const
{
	const APdCharacterBase* TargetCharacter = Cast<APdCharacterBase>(HitActor);
	const APdCharacterBase* SourceCharacter = GetOwningCharacter();
	if (!TargetCharacter || !SourceCharacter || TargetCharacter == SourceCharacter)
	{
		return false;
	}

	return !HitActorsInCurrentAttack.Contains(HitActor);
}

FVector AWeaponBase::GetAttackTraceHalfSize() const
{
	if (!AttackTraceHalfSize.IsNearlyZero())
	{
		return AttackTraceHalfSize;
	}

	return FVector(20.0f);
}

void AWeaponBase::PerformAttackTrace()
{
	if (!HasAuthority() || !AttackTraceStart || !AttackTraceEnd)
	{
		return;
	}

	const FVector TraceStartLocation = AttackTraceStart->GetComponentLocation();
	const FVector TraceEndLocation = AttackTraceEnd->GetComponentLocation();
	if (TraceStartLocation.Equals(TraceEndLocation, KINDA_SMALL_NUMBER))
	{
		return;
	}

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);
	if (AActor* ParentActor = GetAttachParentActor())
	{
		ActorsToIgnore.Add(ParentActor);
	}
	if (APawn* OwnerInstigator = GetInstigator())
	{
		ActorsToIgnore.Add(OwnerInstigator);
	}
	if (APdCharacterBase* SourceCharacter = GetOwningCharacter())
	{
		ActorsToIgnore.Add(SourceCharacter);
	}

	TArray<FHitResult> HitResults;
	const FVector TraceHalfSize = GetAttackTraceHalfSize();
	const FRotator TraceRotation = AttackTraceStart->GetComponentRotation();

	UKismetSystemLibrary::BoxTraceMultiForObjects(
		this,
		TraceStartLocation,
		TraceEndLocation,
		TraceHalfSize,
		TraceRotation,
		ObjectTypes,
		false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResults,
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		0.1f);

	if (bDrawAttackTraceDebug)
	{
		MulticastDrawAttackTraceDebug(TraceStartLocation, TraceEndLocation, TraceHalfSize, TraceRotation, HitResults);
	}

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!CanDamageTracedActor(HitActor))
		{
			continue;
		}

		HitActorsInCurrentAttack.Add(HitActor);
		DebugSuccessfulHit(HitResult);
		ApplyDamageToTarget(HitActor);
	}
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
void AWeaponBase::MulticastDrawAttackTraceDebug_Implementation(
	const FVector& StartLocation,
	const FVector& EndLocation,
	const FVector& HalfSize,
	const FRotator& TraceRotation,
	const TArray<FHitResult>& Hits)
{
	DrawAttackTraceDebug(StartLocation, EndLocation, HalfSize, TraceRotation, Hits);
}

void AWeaponBase::DrawAttackTraceDebug(
	const FVector& StartLocation,
	const FVector& EndLocation,
	const FVector& HalfSize,
	const FRotator& TraceRotation,
	const TArray<FHitResult>& Hits) const
{
	UWorld* World = GetWorld();
	if (!bDrawAttackTraceDebug || !World)
	{
		return;
	}

	const FColor TraceColor = AttackTraceDebugTraceColor.ToFColor(true);
	const FColor HitColor = AttackTraceDebugHitColor.ToFColor(true);
	const FColor SweepColor = Hits.IsEmpty() ? TraceColor : HitColor;
	const float DrawTime = FMath::Max(0.0f, AttackTraceDebugDrawTime);
	const FQuat TraceQuat = TraceRotation.Quaternion();
	const float TraceDistance = FVector::Distance(StartLocation, EndLocation);
	const float StepDistance = FMath::Max(HalfSize.GetMax() * 2.0f, 1.0f);
	const int32 StepCount = FMath::Clamp(FMath::CeilToInt(TraceDistance / StepDistance), 1, 8);

	for (int32 StepIndex = 0; StepIndex <= StepCount; ++StepIndex)
	{
		const float Alpha = static_cast<float>(StepIndex) / static_cast<float>(StepCount);
		const FVector BoxLocation = FMath::Lerp(StartLocation, EndLocation, Alpha);
		DrawDebugBox(World, BoxLocation, HalfSize, TraceQuat, SweepColor, false, DrawTime, 0, 1.5f);
	}

	DrawDebugLine(World, StartLocation, EndLocation, SweepColor, false, DrawTime, 0, 2.0f);
	DrawDebugSphere(World, StartLocation, FMath::Max(4.0f, HalfSize.GetMax() * 0.25f), 12, TraceColor, false, DrawTime, 0, 1.5f);
	DrawDebugSphere(World, EndLocation, FMath::Max(4.0f, HalfSize.GetMax() * 0.25f), 12, SweepColor, false, DrawTime, 0, 1.5f);

	for (const FHitResult& Hit : Hits)
	{
		if (!Hit.GetActor())
		{
			continue;
		}

		const FVector HitLocation = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;
		DrawDebugSphere(World, HitLocation, FMath::Max(8.0f, HalfSize.GetMax() * 0.35f), 12, HitColor, false, DrawTime, 0, 3.0f);
	}
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
