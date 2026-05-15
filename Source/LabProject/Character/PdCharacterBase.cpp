#include "Character/PdCharacterBase.h"

#include "AbilitySystem/Ability/HitReactAbility.h"
#include "AbilitySystem/Ability/RangedAttackAbility.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "PlayerComponent/CombatComponent.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "TimerManager.h"
#include "UI/DamageIndicatorComponent.h"
#include "View/MVVMView.h"
#include "ViewModel/HealthBarViewModel.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdCharacterBase)

DEFINE_LOG_CATEGORY(PdCharacterBaseLog);

namespace
{
	template<typename ComponentType>
	ComponentType* FindConfiguredComponent(const AActor* Owner, ComponentType* FallbackComponent)
	{
		if (!Owner)
		{
			return FallbackComponent;
		}

		TArray<ComponentType*> Components;
		Owner->GetComponents<ComponentType>(Components);
		for (ComponentType* Component : Components)
		{
			if (Component && Component != FallbackComponent)
			{
				return Component;
			}
		}

		return FallbackComponent ? FallbackComponent : (Components.IsEmpty() ? nullptr : Components[0]);
	}
}

/** 캐릭터 기본 상태를 초기화합니다. */
APdCharacterBase::APdCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================
	// === 회전 설정

	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	// =================================================================================================================
	// === 이동 설정

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 700.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 450.0f;

	// =================================================================================================================
	// === 기본 컴포넌트 생성

	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
	HealthBarWidget->SetupAttachment(GetRootComponent());
	HealthBarWidget->SetUsingAbsoluteRotation(true);
}

/** 컴포넌트 초기화 전에 리시버를 등록합니다. */
void APdCharacterBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

/** 시작 시 초기 상태를 구성합니다. */
void APdCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// =================================================================================================================
	// === 확장 이벤트 전송

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	// =================================================================================================================
	// === 기본 애님 레이어 설정


	// =================================================================================================================
	// === 초기 반영

	InitializeAbilitySystemActorInfo();
	ResetAnimationToDefault();
	RefreshHealthBarViewModel();
	UpdateHealthBarFacing();
}

/** 종료 시 바인딩과 리시버를 정리합니다. */
void APdCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAimOffsetForAnimation();
	UpdateHealthBarFacing();
}

void APdCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	BindHealthBarViewModelToASC(nullptr);
	HealthBarViewModel = nullptr;

	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

/** 복제 프로퍼티를 등록합니다. */
void APdCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(APdCharacterBase, CurrentAnimLayer, Params);
	DOREPLIFETIME_CONDITION(APdCharacterBase, AimYawForAnimation, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(APdCharacterBase, AimPitchForAnimation, COND_SkipOwner);
}

/** 빙의 시 ASC를 초기화하고 기본 Ability를 지급합니다. */
void APdCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystemActorInfo();
}

/** PlayerState 복제 후 ASC를 다시 초기화합니다. */
void APdCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystemActorInfo();
}

/** 빙의 해제 시 ASC와 UI 바인딩을 정리합니다. */
void APdCharacterBase::UnPossessed()
{
	Super::UnPossessed();

	ClearAbilitySystemActorInfo();
	BindHealthBarViewModelToASC(nullptr);
	ApplyHealthBarViewModelToWidget();
}

/** 기본 Ability를 지급합니다. */
int32 APdCharacterBase::GiveDefaultAbilities()
{
	// =================================================================================================================
	// === 서버 권한 검사

	if (!HasAuthority())
	{
		UE_LOG(PdCharacterBaseLog, Warning, TEXT("GiveDefaultAbilities failed: '%s' can only grant abilities on the server."), *GetNameSafe(this));
		return 0;
	}

	// =================================================================================================================
	// === ASC 검사

	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(PdCharacterBaseLog, Warning, TEXT("GiveDefaultAbilities failed: '%s' has no valid ability system component."), *GetNameSafe(this));
		return 0;
	}

	const int32 SafeAbilityLevel = 1;
	int32 GrantedCount = 0;
	TArray<TSubclassOf<UGameplayAbility>> AbilityClassesToGrant = DefaultAbilities;

	bool bHasHitReactAbilityClass = false;
	bool bHasRangedAttackAbilityClass = false;
	for (TSubclassOf<UGameplayAbility> AbilityClass : AbilityClassesToGrant)
	{
		const UClass* AbilityClassType = AbilityClass.Get();
		if (AbilityClassType && AbilityClassType->IsChildOf(UHitReactAbility::StaticClass()))
		{
			bHasHitReactAbilityClass = true;
		}

		if (AbilityClassType && AbilityClassType->IsChildOf(URangedAttackAbility::StaticClass()))
		{
			bHasRangedAttackAbilityClass = true;
		}
	}

	if (!bHasHitReactAbilityClass)
	{
		AbilityClassesToGrant.Add(UHitReactAbility::StaticClass());
	}

	if (!bHasRangedAttackAbilityClass)
	{
		AbilityClassesToGrant.Add(URangedAttackAbility::StaticClass());
	}

	// =================================================================================================================
	// === 중복 없이 지급

	for (TSubclassOf<UGameplayAbility> AbilityClass : AbilityClassesToGrant)
	{
		const UClass* AbilityClassType = AbilityClass.Get();
		if (!AbilityClassType)
		{
			continue;
		}

		bool bAlreadyGranted = false;
		for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
		{
			if (AbilitySpec.Ability && AbilitySpec.Ability->GetClass() == AbilityClassType)
			{
				bAlreadyGranted = true;
				break;
			}
		}

		if (bAlreadyGranted)
		{
			continue;
		}

		FGameplayAbilitySpec AbilitySpec(AbilityClass, SafeAbilityLevel, INDEX_NONE, nullptr);
		ASC->GiveAbility(AbilitySpec);
		++GrantedCount;
	}

	return GrantedCount;
}

/** ASC ActorInfo를 초기화합니다. */
void APdCharacterBase::InitializeAbilitySystemActorInfo()
{
	// =================================================================================================================
	// === ASC 구성 요소 조회

	UPdAbilitySystemComponent* PdASC = GetPdAbilitySystemComponent();
	UAbilitySystemComponent* ASC = PdASC;
	AActor* OwnerActor = GetAbilitySystemOwnerActor();
	AActor* AvatarActor = GetAbilitySystemAvatarActor();
	if (!ASC || !OwnerActor || !AvatarActor)
	{
		QueueAbilitySystemActorInfoInitializationRetry();
		RefreshHealthBarViewModel();
		return;
	}

	if (!PdASC->IsRegistered() || !PdASC->HasAbilityActorInfoAllocated())
	{
		QueueAbilitySystemActorInfoInitializationRetry();
		RefreshHealthBarViewModel();
		return;
	}

	// =================================================================================================================
	// === ActorInfo 초기화

	bAbilitySystemActorInfoInitializationQueued = false;
	ASC->InitAbilityActorInfo(OwnerActor, AvatarActor);

	if (UEquipmentComponent* CurrentEquipmentComponent = GetEquipmentComponent())
	{
		CurrentEquipmentComponent->RefreshCachedReferences();
	}

	if (UCombatComponent* CurrentCombatComponent = GetCombatComponent())
	{
		CurrentCombatComponent->RefreshCachedReferences();
	}

	if (HasAuthority())
	{
		GiveDefaultAbilities();
	}

	RefreshHealthBarViewModel();
}

void APdCharacterBase::QueueAbilitySystemActorInfoInitializationRetry()
{
	if (bAbilitySystemActorInfoInitializationQueued || !GetWorld())
	{
		return;
	}

	bAbilitySystemActorInfoInitializationQueued = true;
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bAbilitySystemActorInfoInitializationQueued = false;
		InitializeAbilitySystemActorInfo();
	}));
}

/** ASC ActorInfo를 정리합니다. */
void APdCharacterBase::ClearAbilitySystemActorInfo()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->ClearActorInfo();
	}
}

/** 체력바 ViewModel을 새로 고칩니다. */
void APdCharacterBase::RefreshHealthBarViewModel()
{
	// =================================================================================================================
	// === 전용 서버 제외

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// =================================================================================================================
	// === ViewModel 생성

	if (!HealthBarViewModel)
	{
		HealthBarViewModel = NewObject<UHealthBarViewModel>(this);
	}

	// =================================================================================================================
	// === ASC 바인딩 및 위젯 반영

	BindHealthBarViewModelToASC(GetAbilitySystemComponent());
	ApplyHealthBarViewModelToWidget();
}

/** 지정 위젯에 체력바 ViewModel을 적용합니다. */
void APdCharacterBase::ApplyHealthBarViewModelToWidget(UUserWidget* InWidget)
{
	if (!InWidget || !HealthBarViewModel)
	{
		return;
	}

	// =================================================================================================================
	// === MVVMView 조회

	UMVVMView* View = InWidget->GetExtension<UMVVMView>();
	if (!View)
	{
		UE_LOG(PdCharacterBaseLog, Warning, TEXT("ApplyHealthBarViewModelToWidget failed: widget '%s' does not have an MVVMView extension."), *GetNameSafe(InWidget));
		return;
	}

	// =================================================================================================================
	// === ViewModel 적용

	const bool bSuccess = View->SetViewModel(UHealthBarViewModel::ViewModelName, HealthBarViewModel);
	if (!bSuccess)
	{
		UE_LOG(PdCharacterBaseLog, Warning, TEXT("ApplyHealthBarViewModelToWidget failed: could not set viewmodel '%s' on widget '%s'."),
			*UHealthBarViewModel::ViewModelName.ToString(),
			*GetNameSafe(InWidget));
		return;
	}

	HealthBarViewModel->UpdateAllData();
}

/** 체력바 위젯 컴포넌트에 ViewModel을 적용합니다. */
void APdCharacterBase::ApplyHealthBarViewModelToWidget()
{
	// =================================================================================================================
	// === 전용 서버 제외

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// =================================================================================================================
	// === 위젯 컴포넌트 검사

	if (!HealthBarWidget)
	{
		return;
	}

	// =================================================================================================================
	// === 위젯 생성 및 적용

	HealthBarWidget->InitWidget();
	ApplyHealthBarViewModelToWidget(HealthBarWidget->GetUserWidgetObject());
}

/** 체력바 ViewModel을 ASC에 바인딩합니다. */
void APdCharacterBase::BindHealthBarViewModelToASC(UAbilitySystemComponent* InASC)
{
	if (!HealthBarViewModel)
	{
		return;
	}

	// =================================================================================================================
	// === ASC 없음 처리

	if (!InASC)
	{
		if (HealthBarViewModel->IsViewModelInitialized())
		{
			HealthBarViewModel->UninitializeViewModel();
		}
		return;
	}

	// =================================================================================================================
	// === ASC 바인딩

	HealthBarViewModel->InitializeViewModel(InASC);
}

/** 기본 애님 레이어로 되돌립니다. */
void APdCharacterBase::UpdateAimOffsetForAnimation()
{
	if (!HasAuthority() && !IsLocallyControlled())
	{
		return;
	}

	const FRotator AimRotation = GetBaseAimRotation();
	const FRotator ActorRotation = GetActorRotation();
	const FRotator AimDelta = (AimRotation - ActorRotation).GetNormalized();

	AimYawForAnimation = AimDelta.Yaw;
	AimPitchForAnimation = AimDelta.Pitch;
}

void APdCharacterBase::UpdateHealthBarFacing()
{
	if (GetNetMode() == NM_DedicatedServer || !HealthBarWidget)
	{
		return;
	}

	const bool bShouldShowHealthBar = !IsLocallyControlled();
	if (HealthBarWidget->IsVisible() != bShouldShowHealthBar)
	{
		HealthBarWidget->SetVisibility(bShouldShowHealthBar, true);
	}

	if (!bShouldShowHealthBar || HealthBarWidget->GetWidgetSpace() != EWidgetSpace::World)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* LocalPlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!LocalPlayerController || !LocalPlayerController->IsLocalController())
	{
		return;
	}

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	LocalPlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector WidgetLocation = HealthBarWidget->GetComponentLocation();
	const FRotator FacingRotation = (CameraLocation - WidgetLocation).Rotation();
	HealthBarWidget->SetWorldRotation(FacingRotation);
}

void APdCharacterBase::ResetAnimationToDefault()
{
	SetCurrentAnimLayer(DefaultAnimLayer);
}

/** 현재 애님 레이어를 설정합니다. */
void APdCharacterBase::SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass)
{
	const TSubclassOf<UAnimInstance> NewAnimLayer = AnimLayerClass ? AnimLayerClass : DefaultAnimLayer;
	const bool bAnimLayerChanged = CurrentAnimLayer != NewAnimLayer;
	CurrentAnimLayer = NewAnimLayer;

	if (bAnimLayerChanged && HasAuthority())
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(APdCharacterBase, CurrentAnimLayer, this);
	}

	LinkAnimLayer(NewAnimLayer);
}

/** 현재 애님 레이어 복제 후처리를 수행합니다. */
void APdCharacterBase::OnRep_CurrentAnimLayer()
{
	LinkAnimLayer(CurrentAnimLayer ? CurrentAnimLayer : DefaultAnimLayer);
}

/** 지정 애님 레이어를 메시에 연결합니다. */
void APdCharacterBase::LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const
{
	// =================================================================================================================
	// === 적용 가능 여부 검사

	if (GetNetMode() == NM_DedicatedServer || !AnimLayerClass)
	{
		return;
	}

	// =================================================================================================================
	// === 메시 레이어 연결

	if (GetMesh())
	{
		GetMesh()->LinkAnimClassLayers(AnimLayerClass);
	}
}

/** 현재 캐릭터의 ASC를 반환합니다. */
UAbilitySystemComponent* APdCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}

/** 프로젝트 전용 ASC를 반환합니다. */
UPdAbilitySystemComponent* APdCharacterBase::GetPdAbilitySystemComponent() const
{
	return Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponent());
}

//----------------------------------------------------------------------------------------------------------------------
//--- Component
UEquipmentComponent* APdCharacterBase::GetEquipmentComponent() const
{
	return FindConfiguredComponent(this, EquipmentComponent.Get());
}

UCombatComponent* APdCharacterBase::GetCombatComponent() const
{
	return FindConfiguredComponent(this, CombatComponent.Get());
}

UDamageIndicatorComponent* APdCharacterBase::GetDamageIndicatorComponent() const
{
	return FindConfiguredComponent(this, DamageIndicatorComponent.Get());
}

/** ASC OwnerActor를 반환합니다. */
AActor* APdCharacterBase::GetAbilitySystemOwnerActor() const
{
	return const_cast<APdCharacterBase*>(this);
}

/** ASC AvatarActor를 반환합니다. */
AActor* APdCharacterBase::GetAbilitySystemAvatarActor() const
{
	return const_cast<APdCharacterBase*>(this);
}

/** 서버에서 사망 처리를 수행합니다. */
void APdCharacterBase::HandleDeathAuth()
{
	UE_LOG(PdCharacterBaseLog, Log, TEXT("%s died"), *GetName());
	Destroy();
}

/** 진영 ID를 반환합니다. */
void APdCharacterBase::HandleDamageTaken(float DamageAmount, bool bCriticalHit)
{
	const float DisplayDamageAmount = FMath::Max(DamageAmount, 0.0f);

	if (HasAuthority())
	{
		MulticastHandleDamageTaken(DisplayDamageAmount, bCriticalHit, GetDamageIndicatorWorldLocation());
	}
}

FVector APdCharacterBase::GetDamageIndicatorWorldLocation() const
{
	if (UDamageIndicatorComponent* CurrentDamageIndicatorComponent = GetDamageIndicatorComponent())
	{
		return CurrentDamageIndicatorComponent->ResolveDamageIndicatorWorldLocation();
	}

	const UCapsuleComponent* CharacterCapsule = GetCapsuleComponent();
	const float HeightOffset = CharacterCapsule ? CharacterCapsule->GetScaledCapsuleHalfHeight() + 40.0f : 120.0f;
	return GetActorLocation() + FVector(0.0f, 0.0f, HeightOffset);
}

void APdCharacterBase::MulticastHandleDamageTaken_Implementation(float DamageAmount, bool bCriticalHit, FVector_NetQuantize WorldLocation)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const float DisplayDamageAmount = FMath::Max(DamageAmount, 0.0f);

	if (UDamageIndicatorComponent* CurrentDamageIndicatorComponent = GetDamageIndicatorComponent())
	{
		CurrentDamageIndicatorComponent->ShowDamageIndicator(DisplayDamageAmount, WorldLocation, bCriticalHit);
	}

	OnDamageTaken(DisplayDamageAmount, bCriticalHit, WorldLocation);
}

int32 APdCharacterBase::GetFactionId() const
{
	return FactionId;
}
