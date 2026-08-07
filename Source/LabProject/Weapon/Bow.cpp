#include "Weapon/Bow.h"

#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/CharacterBase.h"
#include "Character/PdPlayer.h"
#include "Common/CollisionChannels.h"
#include "Common/LabGameplayTags.h"
#include "Common/WeaponAnimNotifyNames.h"
#include "Component/Player/CombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Item/ArrowProjectileBase.h"
#include "Definition/Item/ItemDefinition.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(Bow)

namespace
{
void AddUniqueTraceObjectType(TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes, ECollisionChannel CollisionChannel)
{
	ObjectTypes.AddUnique(UEngineTypes::ConvertToObjectType(CollisionChannel));
}

TArray<TEnumAsByte<EObjectTypeQuery>> MakeBowTraceObjectTypes(TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes)
{
	ObjectTypes.RemoveAll(
		[](const TEnumAsByte<EObjectTypeQuery> ObjectType)
		{
			return UEngineTypes::ConvertToCollisionChannel(ObjectType) == ECC_Pawn;
		});
	AddUniqueTraceObjectType(ObjectTypes, ECC_WorldStatic);
	AddUniqueTraceObjectType(ObjectTypes, LabCollisionChannels::HitableBody());
	return ObjectTypes;
}
}

TSubclassOf<AActor> ABow::GetArrowActorClass() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.ArrowActorClass;
	}

	return nullptr;
}

FName ABow::GetArrowAttachSocketName() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.GetResolvedArrowAttachSocketName();
	}

	return FBowWeaponDefinitionData().GetResolvedArrowAttachSocketName();
}

float ABow::GetArrowTraceRange() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.TraceRange;
	}

	return 0.0f;
}

float ABow::GetMinimumDrawDuration() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.MinimumDrawDuration;
	}

	return FBowWeaponDefinitionData().MinimumDrawDuration;
}

float ABow::GetBowFireInterval() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.FireInterval;
	}

	return FBowWeaponDefinitionData().FireInterval;
}

TArray<TEnumAsByte<EObjectTypeQuery>> ABow::GetBowTraceObjectTypes() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return MakeBowTraceObjectTypes(ItemDefinition->WeaponData.Bow.TraceObjectTypes);
	}

	return MakeBowTraceObjectTypes({});
}

float ABow::GetEffectiveMinimumDrawDuration() const
{
	return FMath::Max(GetMinimumDrawDuration() / GetWeaponAttackSpeedPlayRate(), UE_SMALL_NUMBER);
}

float ABow::GetEffectiveBowFireInterval() const
{
	return FMath::Max(GetBowFireInterval() / GetWeaponAttackSpeedPlayRate(), UE_SMALL_NUMBER);
}

bool ABow::IsServerFireCadenceReady() const
{
	const UWorld* World = GetWorld();
	return HasAuthority() && World && static_cast<double>(World->GetTimeSeconds()) >= NextServerArrowLaunchTimeSeconds;
}

bool ABow::BeginServerDraw(APdPlayer* PlayerCharacter)
{
	if (!CanServerUseRangedWeapon(PlayerCharacter, false))
	{
		InvalidateServerDrawState(true);
		return false;
	}

	BindServerDrawInvalidation(PlayerCharacter->GetPdAbilitySystemComponent());

	if (bServerDrawPending || ServerReadyDrawToken != 0)
	{
		return false;
	}

	if (!PlayerCharacter->IsWeaponAimActive())
	{
		PlayerCharacter->SetWeaponAimActive(true, GetAimCameraSettings());
	}

	if (!CanServerUseRangedWeapon(PlayerCharacter, true))
	{
		InvalidateServerDrawState(true);
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	bServerDrawPending = true;
	World->GetTimerManager().SetTimer(
		ServerDrawReadyTimerHandle,
		this,
		&ThisClass::HandleServerDrawReady,
		GetEffectiveMinimumDrawDuration(),
		false);
	return true;
}

void ABow::BindServerDrawInvalidation(UPdAbilitySystemComponent* AbilitySystemComponent)
{
	if (!HasAuthority()
		|| !AbilitySystemComponent
		|| (ServerDrawBoundAbilitySystemComponent.Get() == AbilitySystemComponent
			&& OwnerDeadTagChangedDelegateHandle.IsValid()))
	{
		return;
	}

	UnbindServerDrawInvalidation();
	ServerDrawBoundAbilitySystemComponent = AbilitySystemComponent;
	OwnerDeadTagChangedDelegateHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(LabGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::HandleOwnerDeadTagChanged);
}

void ABow::UnbindServerDrawInvalidation()
{
	if (UPdAbilitySystemComponent* AbilitySystemComponent = ServerDrawBoundAbilitySystemComponent.Get())
	{
		if (OwnerDeadTagChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent
				->RegisterGameplayTagEvent(LabGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
				.Remove(OwnerDeadTagChangedDelegateHandle);
		}
	}

	OwnerDeadTagChangedDelegateHandle.Reset();
	ServerDrawBoundAbilitySystemComponent.Reset();
}

void ABow::HandleOwnerDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	static_cast<void>(CallbackTag);
	if (HasAuthority() && NewCount > 0)
	{
		InvalidateServerDrawState(true);
		StopWeaponMontage();
	}
}

void ABow::HandleServerDrawReady()
{
	ServerDrawReadyTimerHandle.Invalidate();
	if (!HasAuthority() || !bServerDrawPending)
	{
		return;
	}

	bServerDrawPending = false;
	const APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetOwningCharacter());
	if (!CanServerUseRangedWeapon(PlayerCharacter, true))
	{
		InvalidateServerDrawState(true);
		return;
	}

	++NextServerDrawToken;
	if (NextServerDrawToken == 0)
	{
		++NextServerDrawToken;
	}
	ServerReadyDrawToken = NextServerDrawToken;
}

void ABow::InvalidateServerDrawState(bool bDestroyServerDrawnArrow)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ServerDrawReadyTimerHandle);
	}

	ServerDrawReadyTimerHandle.Invalidate();
	bServerDrawPending = false;
	ServerReadyDrawToken = 0;
	bCanLaunchDrawnArrow = false;

	if (bDestroyServerDrawnArrow)
	{
		DestroyDrawnArrow();
	}
}

uint32 ABow::ConsumeServerDrawToken()
{
	if (!HasAuthority() || ServerReadyDrawToken == 0)
	{
		return 0;
	}

	const uint32 ConsumedToken = ServerReadyDrawToken;
	ServerReadyDrawToken = 0;
	return ConsumedToken;
}

void ABow::RecordServerArrowLaunch()
{
	if (const UWorld* World = GetWorld())
	{
		NextServerArrowLaunchTimeSeconds = static_cast<double>(World->GetTimeSeconds()) + GetEffectiveBowFireInterval();
	}
}

bool ABow::HandleAimStart(APdPlayer* PlayerCharacter)
{
	const bool bWasAiming = PlayerCharacter && PlayerCharacter->IsWeaponAimActive();
	if (!Super::HandleAimStart(PlayerCharacter))
	{
		return false;
	}

	if (!bWasAiming)
	{
		bCanLaunchDrawnArrow = false;
		SpawnDrawnArrow(PlayerCharacter);
		PlayWeaponAttackMontage();

		if (HasAuthority())
		{
			BeginServerDraw(PlayerCharacter);
		}
		else
		{
			ServerBeginDraw();
		}
	}

	return true;
}

void ABow::HandleAimEnd(APdPlayer* PlayerCharacter)
{
	Super::HandleAimEnd(PlayerCharacter);
	bCanLaunchDrawnArrow = false;
	DestroyDrawnArrow();
	StopWeaponMontage();

	if (HasAuthority())
	{
		InvalidateServerDrawState(false);
	}
	else
	{
		ServerHandleAimEnd();
	}
}

bool ABow::HandlePrimaryAttack(APdPlayer* PlayerCharacter)
{
	if (!CanUseRangedWeapon(PlayerCharacter, true))
	{
		return false;
	}

	const UCombatComponent* CombatComponent = PlayerCharacter->GetCombatComponent();
	if (!CombatComponent || !CombatComponent->CanAffordRangedWeaponAttackStamina())
	{
		return false;
	}

	if (!bCanLaunchDrawnArrow || !IsValid(CurrentDrawnArrow))
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ZeroVector;
	if (!PlayerCharacter->GetWeaponAimViewPoint(ViewLocation, ViewDirection))
	{
		return false;
	}

	if (HasAuthority())
	{
		if (!LaunchArrowOnServer(PlayerCharacter, ViewLocation, ViewDirection))
		{
			return false;
		}
	}
	else
	{
		ServerLaunchArrow(ViewLocation, ViewDirection);
		DestroyDrawnArrow();
	}

	bCanLaunchDrawnArrow = false;

	const FName ResumeSectionName = GetConfiguredPrimaryAttackResumeWeaponMontageSectionName();
	if (!ResumeSectionName.IsNone())
	{
		JumpToWeaponMontageSectionAndResume(ResumeSectionName);
	}

	return true;
}

bool ABow::HandleAIPrimaryAttack(ACharacterBase* AttackingCharacter, AActor* TargetActor)
{
	if (!CanServerUseRangedWeapon(AttackingCharacter, false)
		|| !IsValid(TargetActor))
	{

		return false;
	}

	return LaunchArrowAtTargetOnServer(AttackingCharacter, TargetActor);
}

bool ABow::HandleAIPrimaryAttackAtLocation(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation)
{
	if (!CanServerUseRangedWeapon(AttackingCharacter, false)
		|| TargetLocation.IsNearlyZero())
	{

		return false;
	}

	return LaunchArrowAtLocationOnServer(AttackingCharacter, TargetActor, TargetLocation);
}

bool ABow::OnWeaponAnimNotifyTiming(FName NotifyName, APdPlayer* PlayerCharacter)
{
	if (NotifyName == WeaponAnimNotifyNames::HoldBow())
	{
		if (!SupportsAimInput() || !PlayerCharacter || !PlayerCharacter->IsWeaponAimActive() || !IsValid(CurrentDrawnArrow))
		{
			if (!PlayerCharacter || !PlayerCharacter->IsWeaponAimActive())
			{
				bCanLaunchDrawnArrow = false;
				DestroyDrawnArrow();
			}
			return false;
		}

		bCanLaunchDrawnArrow = true;
		return true;
	}

	if (NotifyName != WeaponAnimNotifyNames::RedrawBow())
	{
		return Super::OnWeaponAnimNotifyTiming(NotifyName, PlayerCharacter);
	}

	if (!SupportsAimInput() || !PlayerCharacter || !PlayerCharacter->IsWeaponAimActive())
	{
		bCanLaunchDrawnArrow = false;
		DestroyDrawnArrow();
		return false;
	}

	bCanLaunchDrawnArrow = false;
	const bool bArrowRefreshed = RefreshDrawnArrow(PlayerCharacter) != nullptr;
	const bool bMontagePlayed = PlayWeaponAttackMontage();

	if (HasAuthority())
	{
		BeginServerDraw(PlayerCharacter);
	}
	else
	{
		ServerBeginDraw();
	}

	return bArrowRefreshed || bMontagePlayed;
}

void ABow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	InvalidateServerDrawState(false);
	UnbindServerDrawInvalidation();
	DestroyDrawnArrow();
	Super::EndPlay(EndPlayReason);
}

UAnimMontage* ABow::GetConfiguredWeaponMontage() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		if (UAnimMontage* Montage = ItemDefinition->WeaponData.Bow.WeaponMontage.Get())
		{
			return Montage;
		}
	}

	return Super::GetConfiguredWeaponMontage();
}

FName ABow::GetConfiguredPrimaryAttackResumeWeaponMontageSectionName() const
{
	if (const UItemDefinition* ItemDefinition = GetSourceItemDefinition())
	{
		return ItemDefinition->WeaponData.Bow.AttackResumeSectionName;
	}

	return Super::GetConfiguredPrimaryAttackResumeWeaponMontageSectionName();
}

AActor* ABow::SpawnDrawnArrow(APdPlayer* PlayerCharacter)
{
	if (IsValid(CurrentDrawnArrow))
	{
		return CurrentDrawnArrow;
	}

	CurrentDrawnArrow = SpawnArrowActor(PlayerCharacter, true);
	return CurrentDrawnArrow.Get();
}

AActor* ABow::SpawnArrowActor(ACharacterBase* Character, bool bAttachToCharacter)
{
	TSubclassOf<AActor> ArrowClass = GetArrowActorClass();
	USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	UWorld* World = GetWorld();
	if (!Character || !CharacterMesh || !ArrowClass || !World)
	{

		return nullptr;
	}

	const FName AttachSocketName = ResolveArrowAttachSocketName();
	FTransform SpawnTransform = CharacterMesh->GetComponentTransform();
	if (AttachSocketName != NAME_None && CharacterMesh->DoesSocketExist(AttachSocketName))
	{
		SpawnTransform = CharacterMesh->GetSocketTransform(AttachSocketName);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Character;
	SpawnParams.Instigator = Character;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedArrow = World->SpawnActor<AActor>(ArrowClass, SpawnTransform, SpawnParams);
	if (!SpawnedArrow)
	{

		return nullptr;
	}

	SpawnedArrow->SetActorHiddenInGame(false);
	if (bAttachToCharacter)
	{
		SpawnedArrow->AttachToComponent(
			CharacterMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			AttachSocketName);
	}

	return SpawnedArrow;
}

void ABow::DestroyDrawnArrow()
{
	if (!IsValid(CurrentDrawnArrow))
	{
		CurrentDrawnArrow = nullptr;
		return;
	}

	AActor* DrawnArrow = CurrentDrawnArrow.Get();
	CurrentDrawnArrow = nullptr;
	DrawnArrow->Destroy();
}

bool ABow::TryGetArrowLaunchStartLocation(const ACharacterBase* Character, FVector& OutLocation) const
{
	const FName AttachSocketName = ResolveArrowAttachSocketName();
	if (TryGetOwnerMeshSocketLocation(Character, AttachSocketName, OutLocation))
	{
		return true;
	}

	if (IsValid(CurrentDrawnArrow))
	{
		OutLocation = CurrentDrawnArrow->GetActorLocation();
		return true;
	}

	OutLocation = GetActorLocation();
	return true;
}

FVector ABow::GetAIArrowAimLocation(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FVector::ZeroVector;
	}

	float TargetRadius = 0.0f;
	float TargetHalfHeight = 0.0f;
	TargetActor->GetSimpleCollisionCylinder(TargetRadius, TargetHalfHeight);

	FVector AimLocation = TargetActor->GetActorLocation();
	AimLocation.Z += FMath::Max(TargetHalfHeight * 0.5f, 0.0f);
	return AimLocation;
}

bool ABow::LaunchArrowAtTargetOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor)
{
	if (!HasAuthority() || !AttackingCharacter || !IsValid(TargetActor))
	{

		return false;
	}

	return LaunchArrowAtLocationOnServer(AttackingCharacter, TargetActor, GetAIArrowAimLocation(TargetActor));
}

bool ABow::LaunchArrowAtLocationOnServer(ACharacterBase* AttackingCharacter, AActor* TargetActor, const FVector& TargetLocation)
{
	if (!CanServerUseRangedWeapon(AttackingCharacter, false)
		|| !IsServerFireCadenceReady()
		|| TargetLocation.IsNearlyZero())
	{
		return false;
	}

	FVector LaunchStartLocation = FVector::ZeroVector;
	if (!TryGetArrowLaunchStartLocation(AttackingCharacter, LaunchStartLocation))
	{

		return false;
	}

	const FVector LaunchDirection = (TargetLocation - LaunchStartLocation).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero())
	{

		return false;
	}

	AActor* ArrowActor = SpawnArrowActor(AttackingCharacter, false);
	if (!ArrowActor)
	{

		return false;
	}

	ArrowActor->SetActorLocation(LaunchStartLocation);
	ArrowActor->SetActorRotation(LaunchDirection.Rotation());

	AArrowProjectileBase* ArrowProjectile = Cast<AArrowProjectileBase>(ArrowActor);
	const bool bLaunched = ArrowProjectile && ArrowProjectile->LaunchArrowActor(LaunchDirection);
	if (!bLaunched)
	{
		ArrowActor->Destroy();
		return false;
	}

	RecordServerArrowLaunch();
	return true;
}

bool ABow::LaunchArrowOnServer(
	APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation,
	const FVector& RequestedViewDirection)
{
	UCombatComponent* CombatComponent = PlayerCharacter
		? PlayerCharacter->GetCombatComponent()
		: nullptr;
	if (!CanServerUseRangedWeapon(PlayerCharacter, true)
		|| !IsServerFireCadenceReady()
		|| !CombatComponent
		|| !CombatComponent->CanAffordRangedWeaponAttackStamina())
	{
		InvalidateServerDrawState(true);
		return false;
	}

	if (ConsumeServerDrawToken() == 0)
	{
		return false;
	}

	FVector LaunchStartLocation = FVector::ZeroVector;
	if (!TryGetArrowLaunchStartLocation(PlayerCharacter, LaunchStartLocation))
	{
		DestroyDrawnArrow();
		return false;
	}

	const FVector LaunchDirection = CalculateArrowLaunchDirection(
		PlayerCharacter,
		RequestedViewLocation,
		RequestedViewDirection,
		LaunchStartLocation);
	if (LaunchDirection.IsNearlyZero())
	{
		DestroyDrawnArrow();
		return false;
	}

	AActor* ArrowActor = CurrentDrawnArrow.Get();
	if (IsValid(ArrowActor))
	{
		ArrowActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		CurrentDrawnArrow = nullptr;
	}
	else
	{
		ArrowActor = SpawnArrowActor(PlayerCharacter, false);
	}

	if (ArrowActor)
	{
		ArrowActor->SetActorLocation(LaunchStartLocation);
		ArrowActor->SetActorRotation(LaunchDirection.Rotation());
	}

	AArrowProjectileBase* ArrowProjectile = Cast<AArrowProjectileBase>(ArrowActor);
	if (!ArrowProjectile)
	{
		if (IsValid(ArrowActor))
		{
			ArrowActor->Destroy();
		}
		return false;
	}

	if (!CombatComponent->TryCommitRangedWeaponAttackStamina())
	{
		ArrowActor->Destroy();
		return false;
	}

	if (!ArrowProjectile->LaunchArrowActor(LaunchDirection))
	{
		ArrowActor->Destroy();
		return false;
	}

	RecordServerArrowLaunch();
	return true;
}

void ABow::ServerBeginDraw_Implementation()
{
	BeginServerDraw(Cast<APdPlayer>(GetOwningCharacter()));
}

void ABow::ServerLaunchArrow_Implementation(
	FVector_NetQuantize RequestedViewLocation,
	FVector_NetQuantizeNormal RequestedViewDirection)
{
	LaunchArrowOnServer(Cast<APdPlayer>(GetOwningCharacter()), RequestedViewLocation, RequestedViewDirection);
}

void ABow::ServerHandleAimEnd_Implementation()
{
	InvalidateServerDrawState(true);
	StopWeaponMontage();

	if (APdPlayer* PlayerCharacter = Cast<APdPlayer>(GetOwningCharacter()))
	{
		PlayerCharacter->SetWeaponAimActive(false, GetAimCameraSettings());
	}
}

AActor* ABow::RefreshDrawnArrow(APdPlayer* PlayerCharacter)
{
	if (!PlayerCharacter || !PlayerCharacter->IsWeaponAimActive())
	{
		return nullptr;
	}

	if (IsValid(CurrentDrawnArrow))
	{
		return CurrentDrawnArrow;
	}

	return SpawnDrawnArrow(PlayerCharacter);
}

FName ABow::ResolveArrowAttachSocketName() const
{
	return GetArrowAttachSocketName();
}

FVector ABow::CalculateArrowLaunchDirection(
	const APdPlayer* PlayerCharacter,
	const FVector& RequestedViewLocation,
	const FVector& RequestedViewDirection,
	const FVector& LaunchStartLocation) const
{
	const float TraceRange = GetArrowTraceRange();
	if (!PlayerCharacter || TraceRange <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<APdPlayer*>(PlayerCharacter));
	ActorsToIgnore.Add(const_cast<ABow*>(this));
	if (IsValid(CurrentDrawnArrow))
	{
		ActorsToIgnore.Add(CurrentDrawnArrow.Get());
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ZeroVector;
	if (!ResolveServerAimViewPoint(PlayerCharacter, RequestedViewLocation, RequestedViewDirection, ViewLocation, ViewDirection))
	{
		return FVector::ZeroVector;
	}

	const FVector SafeViewDirection = ViewDirection.GetSafeNormal();
	if (SafeViewDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const UItemDefinition* ItemDefinition = GetSourceItemDefinition();
	const EDrawDebugTrace::Type AimTraceDebugDrawType = IsAttackDebugVisualizationEnabled() && ItemDefinition
		? ItemDefinition->WeaponData.Bow.AimTraceDebugDrawType.GetValue()
		: EDrawDebugTrace::None;
	FVector AimTargetLocation = FVector::ZeroVector;
	if (!ResolveAimTargetBeyondLaunchPoint(
		ViewLocation,
		SafeViewDirection,
		LaunchStartLocation,
		TraceRange,
		GetBowTraceObjectTypes(),
		ActorsToIgnore,
		AimTraceDebugDrawType,
		AimTargetLocation))
	{
		return FVector::ZeroVector;
	}

	const FVector LaunchDirection = (AimTargetLocation - LaunchStartLocation).GetSafeNormal();
	if (LaunchDirection.IsNearlyZero()
		|| FVector::DotProduct(LaunchDirection, SafeViewDirection) <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	const float LaunchTraceDistance = FMath::Min(
		TraceRange,
		FVector::Distance(LaunchStartLocation, AimTargetLocation) + 1.0f);
	const FVector LaunchTraceEnd = LaunchStartLocation + (LaunchDirection * LaunchTraceDistance);
	FHitResult HitResult;
	const bool bHit = UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		LaunchStartLocation,
		LaunchTraceEnd,
		GetBowTraceObjectTypes(),
		false,
		ActorsToIgnore,
		EDrawDebugTrace::None,
		HitResult,
		true);

	const FVector TargetLocation = bHit ? HitResult.Location : LaunchTraceEnd;
	return (TargetLocation - LaunchStartLocation).GetSafeNormal();
}
