#include "Character/CharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Common/CollisionChannels.h"
#include "Common/LabGameplayTags.h"
#include "Component/AbilitySystem/PdAbilitySystemComponent.h"
#include "Component/AbilitySystem/StatusEffectReplicationComponent.h"
#include "Component/Character/AbilityStateComponent.h"
#include "Component/Character/CharacterDeathComponent.h"
#include "Component/Character/CharacterHealthBarComponent.h"
#include "Component/Character/CharacterPresentationComponent.h"
#include "Component/Player/CombatComponent.h"
#include "Component/Player/EquipmentComponent.h"
#include "Component/Player/PlayerMatchComponent.h"
#include "Component/Skin/SkinEquipmentComponent.h"
#include "Component/UI/DamageIndicatorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Definition/Character/CharacterBaseDefinition.h"
#include "Definition/UI/WidgetClassDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Mode/PdPlayerState.h"
#include "NiagaraComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterBase)

DEFINE_LOG_CATEGORY_STATIC(LogCharacterBaseRuntime, Log, All);

// 플레이어와 적이 공통으로 사용할 이동·피격 충돌과 능력 연결·사망·외형·체력바·장비 컴포넌트를 구성한다.
ACharacterBase::ACharacterBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (MovementComponent)
	{
		MovementComponent->bOrientRotationToMovement = true;
		MovementComponent->RotationRate = FRotator(0.0f, 700.0f, 0.0f);
		MovementComponent->MaxWalkSpeed = 450.0f;
	}

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;
	}
	ApplySkillDamageCollisionToCharacterComponents();

	AbilityStateComponent = CreateDefaultSubobject<UAbilityStateComponent>(TEXT("CharacterAbilityRuntimeComponent"));
	CharacterDeathComponent = CreateDefaultSubobject<UCharacterDeathComponent>(TEXT("CharacterDeathComponent"));
	CharacterPresentationComponent = CreateDefaultSubobject<UCharacterPresentationComponent>(TEXT("CharacterPresentationComponent"));
	StatusEffectReplicationComponent = CreateDefaultSubobject<UStatusEffectReplicationComponent>(TEXT("StatusEffectReplicationComponent"));

	BodyAuraNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AuraNiagara"));
	BodyAuraNiagaraComponent->SetupAttachment(GetMesh());
	BodyAuraNiagaraComponent->SetAutoActivate(false);

	UCharacterHealthBarComponent* CharacterHealthBar = CreateDefaultSubobject<UCharacterHealthBarComponent>(TEXT("WidgetComponent"));
	CharacterHealthBar->SetupAttachment(GetRootComponent());
	HealthBarWidget = CharacterHealthBar;

	EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));
	SkinEquipmentComponent = CreateDefaultSubobject<USkinEquipmentComponent>(TEXT("SkinEquipmentComponent"));

	CharacterDefinition = TSoftObjectPtr<UCharacterBaseDefinition>(UCharacterBaseDefinition::GetDefaultDefinitionPath());

	ApplyCameraCollisionIgnoreToCharacterComponents();
}

// 캐릭터를 GFCM 확장 기능의 수신 대상으로 등록하고, 외형·사망 처리에 필요한 공통 설정 로딩을 시작한다.
void ACharacterBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
	BeginCharacterDefinitionPreload();
}

// 캐릭터가 월드에서 활동을 시작할 때 애니메이션·충돌 설정을 보정하고, 준비된 설정으로 공통 초기화를 시도한다.
void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();
	bCharacterBeginPlayCalled = true;

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;
	}
	ApplyCameraCollisionIgnoreToCharacterComponents();
	ApplySkillDamageCollisionToCharacterComponents();

	TryInitializeCharacterRuntime();
}

// 캐릭터가 월드를 떠나거나 제거될 때 설정 로딩과 ASC·외형·체력바·사망 처리의 연결 및 GFCM 등록을 정리한다.
void ACharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCharacterDefinitionPreload();
	bCharacterBeginPlayCalled = false;
	bCharacterDefinitionReady = false;
	bCharacterRuntimeInitialized = false;
	LoadedCharacterDefinition = nullptr;

	if (AbilityStateComponent)
	{
		AbilityStateComponent->ClearAbilitySystemActorInfo();
	}
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ShutdownPresentation();
	}
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->ShutdownHealthBar();
	}
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ShutdownDeathRuntime();
	}

	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

// 캐릭터의 외형·사망 설정을 비동기로 준비하고, 이미 로드되었거나 지정되지 않은 경우에는 즉시 준비 완료로 처리한다.
void ACharacterBase::BeginCharacterDefinitionPreload()
{
	CancelCharacterDefinitionPreload();
	bCharacterDefinitionReady = false;
	const uint32 RequestGeneration = CharacterDefinitionLoadGeneration;

	if (CharacterDefinition.IsNull() || CharacterDefinition.IsValid())
	{
		HandleCharacterDefinitionPreloaded(RequestGeneration);
		return;
	}

	TSharedPtr<FStreamableHandle> NewLoadHandle =
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(CharacterDefinition.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &ThisClass::HandleCharacterDefinitionPreloaded, RequestGeneration));
	if (!NewLoadHandle.IsValid())
	{
		HandleCharacterDefinitionPreloaded(RequestGeneration);
		return;
	}

	// 콜백이 요청 반환 전에 실행되었거나 요청이 바뀌었다면 완료된 핸들을 다시 보관하지 않는다.
	if (RequestGeneration == CharacterDefinitionLoadGeneration && !bCharacterDefinitionReady)
	{
		CharacterDefinitionLoadHandle = MoveTemp(NewLoadHandle);
	}
	else
	{
		NewLoadHandle->ReleaseHandle();
	}
}

// 즉시 준비와 비동기 완료를 한곳에서 확정하고 초기화를 이어 간다. 정의 미지정·로드 실패는 기본 설정을 사용한다.
void ACharacterBase::HandleCharacterDefinitionPreloaded(const uint32 RequestGeneration)
{
	if (RequestGeneration != CharacterDefinitionLoadGeneration)
	{
		return;
	}

	LoadedCharacterDefinition = CharacterDefinition.Get();
	bCharacterDefinitionReady = true;
	if (!CharacterDefinition.IsNull() && !LoadedCharacterDefinition)
	{
		UE_LOG(LogCharacterBaseRuntime, Error,
			TEXT("Character definition '%s' did not resolve after preload; native defaults will be used."),
			*CharacterDefinition.ToString());
	}

	TryInitializeCharacterRuntime();
}

// 공통 설정의 로딩 요청을 취소하고 이전 완료 콜백을 무효화해, 종료된 캐릭터가 다시 초기화되지 않게 한다.
void ACharacterBase::CancelCharacterDefinitionPreload()
{
	++CharacterDefinitionLoadGeneration;
	if (CharacterDefinitionLoadHandle.IsValid())
	{
		CharacterDefinitionLoadHandle->CancelHandle();
		CharacterDefinitionLoadHandle->ReleaseHandle();
		CharacterDefinitionLoadHandle.Reset();
	}
}

// 로드된 공통 정의의 외형·사망 설정을 담당 컴포넌트에 적용하며, 정의가 없으면 기본값을 사용한다.
void ACharacterBase::ApplyCharacterDefinition()
{
	const UCharacterBaseDefinition* Definition =
		LoadedCharacterDefinition ? LoadedCharacterDefinition.Get() : GetDefault<UCharacterBaseDefinition>();
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ApplySettings(Definition->GetPresentationSettings());
	}
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ApplySettings(Definition->GetDeathSettings());
	}
}

// BeginPlay와 공통·전용 설정이 준비되면 사망·ASC·외형·체력바를 한 번 초기화하고 파생 클래스와 확장 기능에 알린다.
void ACharacterBase::TryInitializeCharacterRuntime()
{
	if (bCharacterRuntimeInitialized || !bCharacterBeginPlayCalled || !bCharacterDefinitionReady
		|| !IsAdditionalCharacterRuntimeContentReady())
	{
		return;
	}

	ApplyCharacterDefinition();
	bCharacterRuntimeInitialized = true;

	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->InitializeDeathRuntime();
	}

	if (AbilityStateComponent)
	{
		AbilityStateComponent->CaptureBaseMovementSpeed();
		AbilityStateComponent->InitializeAbilitySystemActorInfo();
	}
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->InitializePresentation(BodyAuraNiagaraComponent);
	}
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->InitializeHealthBar();
	}

	HandleCharacterRuntimeInitialized();
	RefreshCharacterTickEnabled();

	// 정의와 파생 클래스 설정을 적용한 뒤 확장 기능에 캐릭터 준비를 알린다.
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
}

// 플레이어·적이 전용 설정의 준비 여부를 추가할 수 있는 조건 함수다. 공통 캐릭터 자체에는 추가 대기 조건이 없다.
bool ACharacterBase::IsAdditionalCharacterRuntimeContentReady() const
{
	return true;
}

// 공통 초기화 뒤 플레이어는 장비를, 적은 전용 전투 설정 등을 이어 붙일 수 있게 둔 확장 지점이다.
void ACharacterBase::HandleCharacterRuntimeInitialized() {}

// 서버에서 플레이어나 AI의 조종자가 배정되면 현재 소유 관계에 맞춰 ASC 연결과 팀 외형을 갱신한다.
void ACharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshAbilitySystemAndTeamBindings();
}

// 클라이언트에 PlayerState가 도착하거나 바뀌면 캐릭터의 ASC 연결과 팀 외형을 갱신한다.
void ACharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	RefreshAbilitySystemAndTeamBindings();
}

// 조종이 해제되는 캐릭터의 ASC·입력 잠금·팀 색상 구독·체력바 연결을 끊어 이전 조종 상태가 남지 않게 한다.
void ACharacterBase::UnPossessed()
{
	// Super가 PlayerState와 Controller를 비우기 전에 ASC와 입력 잠금을 해제한다.
	ClearAbilitySystemActorInfo();
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->UnbindMatchTeamColorChanged();
	}
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->ShutdownHealthBar();
	}
	Super::UnPossessed();
	RefreshCharacterTickEnabled();
}

// 조종자가 바뀌면 로컬 플레이어 여부 등 새 조건에 맞춰 캐릭터 Tick 필요 여부를 다시 판단한다.
void ACharacterBase::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	RefreshCharacterTickEnabled();
}

// 빙의와 PlayerState 복제 양쪽에서 준비된 캐릭터의 ASC·팀 색상 연결을 갱신하고 Tick 필요 여부를 다시 판단한다.
void ACharacterBase::RefreshAbilitySystemAndTeamBindings()
{
	if (bCharacterRuntimeInitialized)
	{
		InitializeAbilitySystemActorInfo();
		if (CharacterPresentationComponent)
		{
			CharacterPresentationComponent->BindMatchTeamColorChanged();
			CharacterPresentationComponent->RefreshCharacterOverlayMaterial();
		}
	}
	RefreshCharacterTickEnabled();
}

// 공통 캐릭터는 ASC를 직접 제공하지 않으며, 플레이어와 적이 각자의 ASC 소유 방식에 맞게 재정의하도록 한다.
UAbilitySystemComponent* ACharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}

// 캐릭터의 ASC를 프로젝트 확장 타입으로 제공해 사망 리셋 등 전용 기능에 접근할 수 있게 한다.
UPdAbilitySystemComponent* ACharacterBase::GetPdAbilitySystemComponent() const
{
	return Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponent());
}

// 별도 재정의가 없으면 능력 데이터의 소유자를 이 캐릭터로 정한다. 플레이어는 PlayerState로 바꾼다.
AActor* ACharacterBase::GetAbilitySystemOwnerActor() const
{
	return const_cast<ACharacterBase*>(this);
}

// 능력이 실제로 이동·공격·연출을 수행할 대상으로 월드에 있는 이 캐릭터를 지정한다.
AActor* ACharacterBase::GetAbilitySystemAvatarActor() const
{
	return const_cast<ACharacterBase*>(this);
}

// 공통 초기화가 끝난 캐릭터에 한해 ASC의 소유자·실행 캐릭터 연결과 상태 구독을 준비하도록 요청한다.
void ACharacterBase::InitializeAbilitySystemActorInfo()
{
	if (bCharacterRuntimeInitialized && AbilityStateComponent)
	{
		AbilityStateComponent->InitializeAbilitySystemActorInfo();
	}
}

// 캐릭터의 ASC 연결과 상태 구독을 해제하도록 요청하며, 이미 새 캐릭터로 넘어간 ASC와 일반 능력·쿨다운은 보존한다.
void ACharacterBase::ClearAbilitySystemActorInfo()
{
	if (AbilityStateComponent)
	{
		AbilityStateComponent->ClearAbilitySystemActorInfo();
	}
}

// 빙결 중 회전 고정과 사망 연출을 갱신하고, 계속 처리할 일이 없으면 캐릭터 Tick을 끈다.
void ACharacterBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AbilityStateComponent)
	{
		AbilityStateComponent->MaintainFrozenRotationLock();
	}
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->TickRuntime(DeltaSeconds);
	}
	RefreshCharacterTickEnabled();
}

// 초기 구동·파생 클래스의 상시 갱신·빙결·사망 연출 중 필요한 일이 있을 때만 캐릭터 Tick을 켠다.
void ACharacterBase::RefreshCharacterTickEnabled()
{
	const bool bAbilityStateNeedsTick = AbilityStateComponent && AbilityStateComponent->NeedsCharacterTick();
	const bool bDeathNeedsTick = CharacterDeathComponent && CharacterDeathComponent->NeedsCharacterTick();
	SetActorTickEnabled(!HasActorBegunPlay() || ShouldUseContinuousCharacterTick() || bAbilityStateNeedsTick || bDeathNeedsTick);
}

// 파생 클래스가 상시 갱신이 필요한 캐릭터인지 정하는 조건 함수다. 공통 캐릭터는 별도 요구가 없으면 상시 Tick을 쓰지 않는다.
bool ACharacterBase::ShouldUseContinuousCharacterTick() const
{
	return false;
}

// 점프·낙하·착지 등 이동 방식의 변화를 능력 연결 컴포넌트에 알려 공중 상태 태그와 관련 공격 취소를 갱신한다.
void ACharacterBase::OnMovementModeChanged(const EMovementMode PrevMovementMode, const uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);
	if (AbilityStateComponent)
	{
		AbilityStateComponent->RefreshAirborneGameplayTag();
	}
}

// GAS의 이동속도 속성과 현재 상태에 맞춰 실제 캐릭터 이동속도를 갱신하도록 요청한다.
void ACharacterBase::ApplyMovementSpeedFromAttribute()
{
	if (AbilityStateComponent)
	{
		AbilityStateComponent->ApplyMovementSpeedFromAttribute();
	}
}

// 캐릭터의 이동과 회전이 빙결 상태로 잠겨 있는지 능력 연결 컴포넌트에서 확인한다.
bool ACharacterBase::IsStatusFrozen() const
{
	return AbilityStateComponent && AbilityStateComponent->IsFrozen();
}

// 빙결로 회전이 잠긴 동안은 건드리지 않고, 그 외에는 현재 조준·이동 상태에 맞는 회전 방식을 다시 적용한다.
void ACharacterBase::ReapplyCurrentRotationPolicy()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent || IsStatusFrozen())
	{
		return;
	}

	ApplyCurrentRotationPolicy(MovementComponent);
}

// 빙결 전에 저장한 회전 설정을 복구한 뒤 현재 조준·이동 상태에 맞는 회전 방식을 다시 적용한다.
void ACharacterBase::RestoreRotationSettingsAfterFrozen(UCharacterMovementComponent* MovementComponent)
{
	if (AbilityStateComponent)
	{
		AbilityStateComponent->RestoreCachedRotationSettings(MovementComponent);
	}

	ReapplyCurrentRotationPolicy();
}

// 빙의 변경이나 빙결 해제 후 적용할 회전 방식을 파생 클래스가 정하는 확장 지점이며, 기본 구현은 변경하지 않는다.
void ACharacterBase::ApplyCurrentRotationPolicy(UCharacterMovementComponent*) {}

// 캐릭터 캡슐·메시·체력바가 카메라 충돌을 막아 시점을 불필요하게 밀어내지 않도록 설정한다.
void ACharacterBase::ApplyCameraCollisionIgnoreToCharacterComponents() const
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	if (HealthBarWidget)
	{
		HealthBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

// 투사체가 이동용 캡슐을 통과하고 실제 피격용 메시를 맞히도록 충돌 채널과 반응을 맞춘다.
void ACharacterBase::ApplySkillDamageCollisionToCharacterComponents() const
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(LabCollisionChannels::Projectile(), ECR_Ignore);
	}

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetCollisionObjectType(LabCollisionChannels::HitableBody());
		CharacterMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		CharacterMesh->SetCollisionResponseToChannel(LabCollisionChannels::Projectile(), ECR_Block);
		if (CharacterMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
	}
}

// 무기 장착·해제와 장착 외형을 처리할 수 있도록 이 캐릭터의 장비 컴포넌트를 제공한다.
UEquipmentComponent* ACharacterBase::GetEquipmentComponent() const
{
	return EquipmentComponent.Get();
}

// 캐릭터에 부착된 전투 컴포넌트를 찾아 기본 공격·조준 등 전투 동작의 진입점을 제공한다.
UCombatComponent* ACharacterBase::GetCombatComponent() const
{
	return FindComponentByClass<UCombatComponent>();
}

// 캐릭터에 부착된 피해 숫자 표시 컴포넌트를 찾는다. 해당 연출이 없는 캐릭터에서는 찾지 못할 수 있다.
UDamageIndicatorComponent* ACharacterBase::GetDamageIndicatorComponent() const
{
	return FindComponentByClass<UDamageIndicatorComponent>();
}

// 기존 체력바 위젯 참조를 통해 체력 데이터 연결과 화면 표시를 관리하는 컴포넌트를 제공한다.
UCharacterHealthBarComponent* ACharacterBase::GetCharacterHealthBarComponent() const
{
	return Cast<UCharacterHealthBarComponent>(HealthBarWidget.Get());
}

// UI 정의에서 이 캐릭터의 머리 위 체력바에 사용할 위젯 클래스를 선택한다.
TSubclassOf<UUserWidget> ACharacterBase::ResolveHealthBarWidgetClass(const UWidgetClassDefinition* WidgetDefinition) const
{
	return WidgetDefinition ? WidgetDefinition->GetHealthBarWidgetClass() : nullptr;
}

// 캐릭터에 이미 지정된 체력바 위젯은 유지하고, 비어 있을 때만 정의에서 찾은 위젯으로 채울지 판단한다.
bool ACharacterBase::ShouldApplyResolvedHealthBarWidgetClass(UClass* CurrentWidgetClass, TSubclassOf<UUserWidget> ResolvedWidgetClass) const
{
	return !CurrentWidgetClass && ResolvedWidgetClass != nullptr;
}

// ASC나 체력 데이터가 준비·변경되었을 때 체력바의 데이터 연결을 다시 구성하도록 요청한다.
void ACharacterBase::RefreshHealthBarViewModel()
{
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->RefreshViewModel();
	}
}

// 관찰자의 거리·시야와 캐릭터 상태를 기준으로 이 화면에 체력바를 보여 줄지 판단하고 방향을 맞추도록 요청한다.
void ACharacterBase::UpdateHealthBarVisibilityForLocalViewer(
	APlayerController* LocalPlayerController, const FVector& CameraLocation, const FRotator& CameraRotation, const float MaxDistanceSquared)
{
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->UpdateVisibilityForLocalViewer(LocalPlayerController, CameraLocation, CameraRotation, MaxDistanceSquared);
	}
}

// 현재 화면에서 체력바를 표시하거나 숨기도록 요청하며, 사망한 캐릭터는 표시 요청이 있어도 숨긴다.
void ACharacterBase::SetHealthBarVisibleForLocalViewer(const bool bVisible)
{
	if (UCharacterHealthBarComponent* CharacterHealthBar = GetCharacterHealthBarComponent())
	{
		CharacterHealthBar->SetVisibleForLocalViewer(bVisible);
	}
}

// 애니메이션이 몸의 방향과 조준 방향 사이의 좌우 각도 차이를 읽을 수 있도록 제공한다.
float ACharacterBase::GetAimYawForAnimation() const
{
	return CharacterPresentationComponent ? CharacterPresentationComponent->GetAimYaw() : 0.0f;
}

// 애니메이션이 몸의 방향과 조준 방향 사이의 상하 각도 차이를 읽을 수 있도록 제공한다.
float ACharacterBase::GetAimPitchForAnimation() const
{
	return CharacterPresentationComponent ? CharacterPresentationComponent->GetAimPitch() : 0.0f;
}

// 캐릭터가 바라보는 방향과 몸의 회전 차이를 계산해 상체 조준 애니메이션에 사용할 값을 갱신한다.
void ACharacterBase::UpdateAimOffsetForAnimation()
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->UpdateAimOffset();
	}
}

// 복제 등 외부에서 전달받은 좌우·상하 조준값을 캐릭터 애니메이션용 상태에 반영한다.
void ACharacterBase::SetAimOffsetForAnimation(const float AimYaw, const float AimPitch)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->SetAimOffset(AimYaw, AimPitch);
	}
}

// 무기나 특수 행동의 애니메이션을 기본 캐릭터 레이어로 되돌리도록 요청한다.
void ACharacterBase::ResetAnimationToDefault()
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ResetAnimationToDefault();
	}
}

// 장착 무기나 행동에 맞는 애니메이션 레이어를 현재 상태로 지정하고, 서버의 변경은 다른 클라이언트에도 반영하게 한다.
void ACharacterBase::SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->SetCurrentAnimLayer(AnimLayerClass);
	}
}

// 스킬이 캐릭터 위에 표시할 오버레이 재질을 해당 연출의 출처와 함께 등록한다.
void ACharacterBase::ApplySkillPresentationOverlay(UObject* PresentationSource, UMaterialInterface* OverlayMaterial)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ApplySkillPresentationOverlay(PresentationSource, OverlayMaterial);
	}
}

// 종료된 스킬 출처의 오버레이만 제거하고, 남아 있는 스킬이나 팀 색상에 맞춰 외형을 다시 결정하게 한다.
void ACharacterBase::ClearSkillPresentationOverlay(UObject* PresentationSource)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ClearSkillPresentationOverlay(PresentationSource);
	}
}

// 캐릭터 몸의 오라 이펙트에 종류·위치·크기를 적용하고 활성화 또는 초기화하도록 요청한다.
void ACharacterBase::ApplyBodyAuraNiagaraWithOffset(const FName ComponentName, UNiagaraSystem* NiagaraSystem, const bool bActivate,
	const bool bResetSystem, const FVector RelativeLocationOffset, const FVector RelativeScale)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ApplyBodyAuraNiagaraWithOffset(
			ComponentName, NiagaraSystem, bActivate, bResetSystem, RelativeLocationOffset, RelativeScale);
	}
}

// 끝나는 연출과 현재 오라 이펙트가 일치할 때만 제거해, 뒤이어 적용된 다른 오라를 지우지 않게 한다.
void ACharacterBase::ClearBodyAuraNiagaraIfMatching(const FName ComponentName, const UNiagaraSystem* ExpectedNiagaraSystem)
{
	if (CharacterPresentationComponent)
	{
		CharacterPresentationComponent->ClearBodyAuraNiagaraIfMatching(ComponentName, ExpectedNiagaraSystem);
	}
}

// 대시 GameplayCue를 캐릭터의 대시 연출로 연결하고, GAS의 기본 큐 처리도 이어서 실행한다.
void ACharacterBase::HandleGameplayCue(
	AActor* Self, const FGameplayTag GameplayCueTag, const EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	if (GameplayCueTag.MatchesTagExact(LabGameplayTags::GameplayCue_Dash_Active) && CharacterPresentationComponent)
	{
		CharacterPresentationComponent->HandleDashGameplayCue(EventType, Parameters);
	}
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

// 현재 PlayerState의 경기 팀 색상 번호를 읽어 외형과 팀 판정에 사용하며, 팀 정보가 없으면 INDEX_NONE을 반환한다.
int32 ACharacterBase::GetMatchTeamColorIndex() const
{
	const APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>();
	const UPlayerMatchComponent* MatchComponent = PdPlayerState ? PdPlayerState->GetPlayerMatchComponent() : nullptr;
	return MatchComponent ? MatchComponent->GetMatchTeamColorIndex() : INDEX_NONE;
}

// 두 캐릭터가 모두 유효한 경기 팀 색상 번호를 가지고 그 번호가 같은지 판단한다. FactionId 비교는 하지 않는다.
bool ACharacterBase::IsSameTeam(const ACharacterBase* OtherCharacter) const
{
	if (!OtherCharacter)
	{
		return false;
	}
	const int32 MyTeamColorIndex = GetMatchTeamColorIndex();
	const int32 OtherTeamColorIndex = OtherCharacter->GetMatchTeamColorIndex();
	return MyTeamColorIndex != INDEX_NONE && MyTeamColorIndex == OtherTeamColorIndex;
}

// 자기 자신과 같은 팀을 제외하는 팀 기준 피해 허용 여부를 판단한다. 무적 등 다른 피해 조건은 검사하지 않는다.
bool ACharacterBase::CanDamageCharacterByTeam(const ACharacterBase* OtherCharacter) const
{
	return OtherCharacter && OtherCharacter != this && !IsSameTeam(OtherCharacter);
}

// 캐릭터의 진영 식별자를 제공한다. 경기의 팀 색상 번호와는 별도로 보관되는 값이다.
int32 ACharacterBase::GetFactionId() const
{
	return FactionId;
}

// GAS의 Dead 태그로 현재 사망 상태를 조회한다. 사망 연출을 이미 처리했는지는 IsDeathHandled에서 별도로 확인한다.
bool ACharacterBase::IsDead() const
{
	const UAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(LabGameplayTags::State_Dead);
}

// 이 캐릭터의 사망 처리가 이미 시작되었는지 확인해 외형이나 체력바 등의 후속 판단에 제공한다.
bool ACharacterBase::IsDeathHandled() const
{
	return CharacterDeathComponent && CharacterDeathComponent->IsDeathHandled();
}

// 서버에서 확정된 피해량과 치명타 여부를 피해 표시 알림으로 전달한다. 체력 차감이나 피격 반응 실행은 맡지 않는다.
void ACharacterBase::HandleDamageTaken(const float DamageAmount, const bool bCriticalHit, bool, AActor*, AActor*)
{
	// 추가 인자는 적의 피격 반응과 몬스터의 공격자·보상 소유자 판정을 위해 가상 함수 계약에 유지한다.
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->HandleDamageTaken(DamageAmount, bCriticalHit);
	}
}

// 피해 숫자가 나타날 월드 위치를 구하며, 전용 표시 컴포넌트가 없으면 캐릭터 머리 위 위치를 사용한다.
FVector ACharacterBase::GetDamageIndicatorWorldLocation() const
{
	if (UDamageIndicatorComponent* DamageIndicator = GetDamageIndicatorComponent())
	{
		return DamageIndicator->ResolveDamageIndicatorWorldLocation();
	}

	const UCapsuleComponent* CharacterCapsule = GetCapsuleComponent();
	const float HeightOffset = CharacterCapsule ? CharacterCapsule->GetScaledCapsuleHalfHeight() + 40.0f : 120.0f;
	return GetActorLocation() + FVector(0.0f, 0.0f, HeightOffset);
}

// 전달된 피해 정보를 각 화면의 피해 숫자·로컬 피격 화면 효과·Blueprint 피격 알림으로 연결한다.
void ACharacterBase::MulticastHandleDamageTaken_Implementation(
	const float DamageAmount, const bool bCriticalHit, const FVector_NetQuantize WorldLocation)
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->HandleRemoteDamageTaken(DamageAmount, bCriticalHit, WorldLocation);
	}
}

// 사망한 캐릭터의 이동·충돌·체력바·메시 물리를 사망 상태로 전환하도록 사망 컴포넌트에 요청한다.
void ACharacterBase::HandleDeath_Implementation()
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ApplyDeathPhysics();
	}
}

// 서버의 사망 알림을 받은 클라이언트가 능력 상태 정리와 캐릭터 사망 처리를 중복 없이 수행하게 한다.
void ACharacterBase::MulticastHandleDeath_Implementation()
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->HandleRemoteDeath();
	}
}

// 시체가 서서히 사라지는 디졸브 연출을 시작하며, 서버의 요청은 클라이언트에도 전달한다.
void ACharacterBase::StartDeathDissolve(const float DurationSeconds)
{
	if (!CharacterDeathComponent)
	{
		return;
	}
	const float SafeDuration = CharacterDeathComponent->GetSafeDissolveDuration(DurationSeconds);
	if (HasAuthority())
	{
		MulticastStartDeathDissolve(SafeDuration);
		return;
	}
	CharacterDeathComponent->StartDeathDissolveLocal(SafeDuration);
}

// 서버가 지정한 시간으로 각 인스턴스에서 사망 디졸브 연출을 재생한다.
void ACharacterBase::MulticastStartDeathDissolve_Implementation(const float DurationSeconds)
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->StartDeathDissolveLocal(DurationSeconds);
	}
}

// 같은 캐릭터를 다시 사용할 수 있도록 사망 연출과 물리·이동·외형 등의 상태를 리스폰 기준으로 복구한다.
void ACharacterBase::ResetDeathStateForRespawn()
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ResetDeathStateForRespawn();
	}
}

// 캐릭터를 지정된 리스폰 위치로 옮겨 복구하고, 서버에서는 같은 위치와 복구 요청을 클라이언트에도 전달한다.
void ACharacterBase::ResetDeathStateForRespawnAtTransform(const FTransform& RespawnTransform)
{
	SetActorTransform(RespawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	ResetDeathStateForRespawn();

	if (HasAuthority())
	{
		MulticastResetDeathStateForRespawnAtTransform(RespawnTransform);
		ForceNetUpdate();
	}
}

// 클라이언트의 캐릭터를 서버가 지정한 리스폰 위치로 옮기고 사망 상태를 복구한다. 서버에서는 중복 실행하지 않는다.
void ACharacterBase::MulticastResetDeathStateForRespawnAtTransform_Implementation(const FTransform& RespawnTransform)
{
	if (!HasAuthority())
	{
		ResetDeathStateForRespawnAtTransform(RespawnTransform);
	}
}

// 캐릭터에 남은 오버레이 재질을 제거하도록 요청하며, 서버의 요청은 클라이언트에도 전달한다.
void ACharacterBase::ClearCharacterOverlayMaterial()
{
	if (HasAuthority())
	{
		MulticastClearCharacterOverlayMaterial();
		return;
	}
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ClearCharacterOverlayMaterialLocal();
	}
}

// 서버의 오버레이 제거 요청을 각 인스턴스의 캐릭터 외형에 적용한다.
void ACharacterBase::MulticastClearCharacterOverlayMaterial_Implementation()
{
	if (CharacterDeathComponent)
	{
		CharacterDeathComponent->ClearCharacterOverlayMaterialLocal();
	}
}
