#include "Character/PdCharacterBase.h"

#include "AbilitySystem/AttributeSet/BasicAttributeSet.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Common/LabGameplayTags.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "PlayerComponent/CombatComponent.h"
#include "PlayerComponent/EquipmentComponent.h"
#include "Skin/SkinEquipmentComponent.h"
#include "TimerManager.h"
#include "UI/DamageIndicatorComponent.h"
#include "UI/Widget/EnemyAvatarWidget.h"
#include "UI/Widget/EnemyHealthBarWidget.h"
#include "UI/Widget/EnemyShieldBarWidget.h"
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

/** 캐릭??기본 ?�태�?초기?�합?�다. */
APdCharacterBase::APdCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// =================================================================================================================
	// === ?�전 ?�정

	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	// =================================================================================================================
	// === ?�동 ?�정

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 700.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 450.0f;

	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// =================================================================================================================
	// === 기본 컴포?�트 ?�성

	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
	HealthBarWidget->SetupAttachment(GetRootComponent());
	HealthBarWidget->SetUsingAbsoluteRotation(true);

	SkinEquipmentComponent = CreateDefaultSubobject<USkinEquipmentComponent>(TEXT("SkinEquipmentComponent"));
}

/** 컴포?�트 초기???�에 리시버�? ?�록?�니?? */
void APdCharacterBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

/** ?�작 ??초기 ?�태�?구성?�니?? */
void APdCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// =================================================================================================================
	// === ?�장 ?�벤???�송

	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	// =================================================================================================================
	// === 기본 ?�님 ?�이???�정


	// =================================================================================================================
	// === 초기 반영

	InitializeAbilitySystemActorInfo();
	ResetAnimationToDefault();
	RefreshHealthBarViewModel();
	UpdateHealthBarFacing();
}

/** 종료 ??바인?�과 리시버�? ?�리?�니?? */
void APdCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAimOffsetForAnimation();
	UpdateHealthBarFacing();
}

void APdCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindStaminaRegenFromASC();
	UnbindDeadTagEvent();
	GetWorldTimerManager().ClearTimer(HealthBarViewModelRetryTimerHandle);
	BindHealthBarViewModelToASC(nullptr);
	HealthBarViewModel = nullptr;

	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

/** 복제 ?�로?�티�??�록?�니?? */
void APdCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(APdCharacterBase, CurrentAnimLayer, Params);

	FDoRepLifetimeParams AimParams;
	AimParams.bIsPushBased = true;
	AimParams.Condition = COND_SkipOwner;

	DOREPLIFETIME_WITH_PARAMS_FAST(APdCharacterBase, AimYawForAnimation, AimParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(APdCharacterBase, AimPitchForAnimation, AimParams);
}

/** Initializes ASC when possessed. */
void APdCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystemActorInfo();
}

/** PlayerState 복제 ??ASC�??�시 초기?�합?�다. */
void APdCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystemActorInfo();
}

/** 빙의 ?�제 ??ASC?� UI 바인?�을 ?�리?�니?? */
void APdCharacterBase::UnPossessed()
{
	Super::UnPossessed();

	ClearAbilitySystemActorInfo();
	BindHealthBarViewModelToASC(nullptr);
	TryApplyHealthBarViewModelToWidget();
}

/** Initializes ASC ActorInfo. */
void APdCharacterBase::InitializeAbilitySystemActorInfo()
{
	// =================================================================================================================
	// === ASC 구성 ?�소 조회

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
	// === ActorInfo 초기??
	bAbilitySystemActorInfoInitializationQueued = false;
	ASC->InitAbilityActorInfo(OwnerActor, AvatarActor);
	BindStaminaRegenToASC(ASC);
	BindDeadTagEvent(ASC);

	if (UEquipmentComponent* CurrentEquipmentComponent = GetEquipmentComponent())
	{
		CurrentEquipmentComponent->RefreshCachedReferences();
	}

	if (UCombatComponent* CurrentCombatComponent = GetCombatComponent())
	{
		CurrentCombatComponent->RefreshCachedReferences();
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

/** ASC ActorInfo�??�리?�니?? */
void APdCharacterBase::ClearAbilitySystemActorInfo()
{
	UnbindStaminaRegenFromASC();
	UnbindDeadTagEvent();

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->ClearActorInfo();
	}
}

/** 체력�?ViewModel???�로 고칩?�다. */
void APdCharacterBase::RefreshHealthBarViewModel()
{
	// =================================================================================================================
	// === ���� ���� ����

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// =================================================================================================================
	// === ViewModel ����

	if (!HealthBarViewModel)
	{
		HealthBarViewModel = NewObject<UHealthBarViewModel>(this);
	}

	// =================================================================================================================
	// === ASC / AttributeSet �غ� ���� Ȯ�� �� ���ε�

	const bool bBoundToReadyASC = BindHealthBarViewModelToASC(GetAbilitySystemComponent());
	const bool bAppliedToWidget = TryApplyHealthBarViewModelToWidget();

	if (bBoundToReadyASC && bAppliedToWidget)
	{
		GetWorldTimerManager().ClearTimer(HealthBarViewModelRetryTimerHandle);
		return;
	}

	QueueHealthBarViewModelRefreshRetry();
}
void APdCharacterBase::ApplyHealthBarViewModelToWidget(UUserWidget* InWidget)
{
	TryApplyHealthBarViewModelToWidget(InWidget);
}

bool APdCharacterBase::TryApplyHealthBarViewModelToWidget(UUserWidget* InWidget)
{
	if (!InWidget)
	{
		return false;
	}

	if (UEnemyAvatarWidget* EnemyAvatarWidget = Cast<UEnemyAvatarWidget>(InWidget))
	{
		EnemyAvatarWidget->SetOwnerActor(this);
		return true;
	}

	if (UEnemyHealthBarWidget* EnemyHealthBarWidget = Cast<UEnemyHealthBarWidget>(InWidget))
	{
		EnemyHealthBarWidget->SetOwnerActor(this);
		return true;
	}

	if (UEnemyShieldBarWidget* EnemyShieldBarWidget = Cast<UEnemyShieldBarWidget>(InWidget))
	{
		EnemyShieldBarWidget->SetOwnerActor(this);
		return true;
	}

	if (!HealthBarViewModel)
	{
		return false;
	}

	// =================================================================================================================
	// === MVVMView ��ȸ

	UMVVMView* View = InWidget->GetExtension<UMVVMView>();
	if (!View)
	{
		UE_LOG(PdCharacterBaseLog, Warning, TEXT("ApplyHealthBarViewModelToWidget failed: widget '%s' does not have an MVVMView extension."), *GetNameSafe(InWidget));
		return false;
	}

	// =================================================================================================================
	// === ViewModel ����

	const bool bSuccess = View->SetViewModel(UHealthBarViewModel::ViewModelName, HealthBarViewModel);
	if (!bSuccess)
	{
		UE_LOG(PdCharacterBaseLog, Warning, TEXT("ApplyHealthBarViewModelToWidget failed: could not set viewmodel '%s' on widget '%s'."),
			*UHealthBarViewModel::ViewModelName.ToString(),
			*GetNameSafe(InWidget));
		return false;
	}

	if (HealthBarViewModel->IsViewModelInitialized())
	{
		HealthBarViewModel->UpdateAllData();
	}

	return HealthBarViewModel->IsViewModelInitialized();
}
bool APdCharacterBase::TryApplyHealthBarViewModelToWidget()
{
	// =================================================================================================================
	// === ���� ���� ����

	if (GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	// =================================================================================================================
	// === ���� ������Ʈ �˻�
	if (!HealthBarWidget)
	{
		return false;
	}

	// =================================================================================================================
	// === ���� ���� �� ����

	HealthBarWidget->InitWidget();
	return TryApplyHealthBarViewModelToWidget(HealthBarWidget->GetUserWidgetObject());
}
bool APdCharacterBase::BindHealthBarViewModelToASC(UAbilitySystemComponent* InASC)
{
	if (!HealthBarViewModel)
	{
		return false;
	}

	// =================================================================================================================
	// === ASC / AttributeSet ���� ó��

	if (!IsHealthBarAttributeDataReady(InASC))
	{
		if (HealthBarViewModel->IsViewModelInitialized())
		{
			HealthBarViewModel->UninitializeViewModel();
		}
		return false;
	}

	// =================================================================================================================
	// === ASC ���ε�
	HealthBarViewModel->InitializeViewModel(InASC);
	return HealthBarViewModel->IsViewModelInitialized();
}

bool APdCharacterBase::IsHealthBarAttributeDataReady(const UAbilitySystemComponent* InASC) const
{
	return InASC
		&& InASC->IsRegistered()
		&& InASC->GetAttributeSet(UBasicAttributeSet::StaticClass()) != nullptr;
}

void APdCharacterBase::QueueHealthBarViewModelRefreshRetry()
{
	if (GetNetMode() == NM_DedicatedServer || !GetWorld())
	{
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(HealthBarViewModelRetryTimerHandle))
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		HealthBarViewModelRetryTimerHandle,
		this,
		&ThisClass::RetryRefreshHealthBarViewModel,
		0.1f,
		true);
}

void APdCharacterBase::RetryRefreshHealthBarViewModel()
{
	RefreshHealthBarViewModel();
}
void APdCharacterBase::UpdateAimOffsetForAnimation()
{
	if (!HasAuthority() && !IsLocallyControlled())
	{
		return;
	}

	const FRotator AimRotation = GetBaseAimRotation();
	const FRotator ActorRotation = GetActorRotation();
	const FRotator AimDelta = (AimRotation - ActorRotation).GetNormalized();

	const float NewAimYaw = AimDelta.Yaw;
	const float NewAimPitch = AimDelta.Pitch;
	const bool bAimYawChanged = !FMath::IsNearlyEqual(AimYawForAnimation, NewAimYaw, KINDA_SMALL_NUMBER);
	const bool bAimPitchChanged = !FMath::IsNearlyEqual(AimPitchForAnimation, NewAimPitch, KINDA_SMALL_NUMBER);

	AimYawForAnimation = NewAimYaw;
	AimPitchForAnimation = NewAimPitch;

	if (HasAuthority())
	{
		if (bAimYawChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(APdCharacterBase, AimYawForAnimation, this);
		}

		if (bAimPitchChanged)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(APdCharacterBase, AimPitchForAnimation, this);
		}
	}
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

/** ?�재 ?�님 ?�이?��? ?�정?�니?? */
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

/** ?�재 ?�님 ?�이??복제 ?�처리�? ?�행?�니?? */
void APdCharacterBase::OnRep_CurrentAnimLayer()
{
	LinkAnimLayer(CurrentAnimLayer ? CurrentAnimLayer : DefaultAnimLayer);
}

/** 지???�님 ?�이?��? 메시???�결?�니?? */
void APdCharacterBase::LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const
{
	// =================================================================================================================
	// === ?�용 가???��? 검??
	if (!AnimLayerClass)
	{
		return;
	}

	// =================================================================================================================
	// === 메시 ?�이???�결

	if (GetMesh())
	{
		GetMesh()->LinkAnimClassLayers(AnimLayerClass);
	}
}

/** ?�재 캐릭?�의 ASC�?반환?�니?? */
UAbilitySystemComponent* APdCharacterBase::GetAbilitySystemComponent() const
{
	return nullptr;
}

void APdCharacterBase::HandleGameplayCue(AActor* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType,
	const FGameplayCueParameters& Parameters)
{
	if (GameplayCueTag.MatchesTagExact(LabGameplayTags::GameplayCue_Dash_Active))
	{
		HandleDashGameplayCue(EventType, Parameters);
	}

	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

void APdCharacterBase::HandleDashGameplayCue(EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	if (EventType == EGameplayCueEvent::OnActive)
	{
		if (USkeletalMeshComponent* MeshComponent = GetMesh())
		{
			MeshComponent->SetVisibility(false, true);
		}

		OnDashCueActivated(Parameters);
		return;
	}

	if (EventType == EGameplayCueEvent::Removed)
	{
		if (USkeletalMeshComponent* MeshComponent = GetMesh())
		{
			MeshComponent->SetVisibility(true, true);
		}

		OnDashCueRemoved(Parameters);
	}
}

void APdCharacterBase::BindStaminaRegenToASC(UAbilitySystemComponent* InASC)
{
	if (!HasAuthority())
	{
		return;
	}

	if (StaminaRegenASC.Get() == InASC && StaminaChangedDelegateHandle.IsValid())
	{
		return;
	}

	UnbindStaminaRegenFromASC();
	if (!InASC)
	{
		return;
	}

	StaminaRegenASC = InASC;
	StaminaChangedDelegateHandle = InASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &ThisClass::HandleStaminaChanged);
}

void APdCharacterBase::UnbindStaminaRegenFromASC()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
	}

	if (UAbilitySystemComponent* ASC = StaminaRegenASC.Get())
	{
		if (StaminaChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBasicAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedDelegateHandle);
		}
	}

	StaminaChangedDelegateHandle.Reset();
	StaminaRegenASC.Reset();
}

void APdCharacterBase::BindDeadTagEvent(UAbilitySystemComponent* InASC)
{
	if (DeadTagBoundAbilitySystemComponent.Get() == InASC && DeadTagChangedDelegateHandle.IsValid())
	{
		return;
	}

	UnbindDeadTagEvent();
	if (!InASC)
	{
		return;
	}

	DeadTagBoundAbilitySystemComponent = InASC;
	DeadTagChangedDelegateHandle = InASC->RegisterGameplayTagEvent(LabGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &ThisClass::OnDeadTagChanged);

	const int32 CurrentDeadTagCount = InASC->GetTagCount(LabGameplayTags::State_Dead);
	if (CurrentDeadTagCount > 0)
	{
		OnDeadTagChanged(LabGameplayTags::State_Dead, CurrentDeadTagCount);
	}
}

void APdCharacterBase::UnbindDeadTagEvent()
{
	if (UAbilitySystemComponent* ASC = DeadTagBoundAbilitySystemComponent.Get())
	{
		if (DeadTagChangedDelegateHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(LabGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
				.Remove(DeadTagChangedDelegateHandle);
		}
	}

	DeadTagChangedDelegateHandle.Reset();
	DeadTagBoundAbilitySystemComponent.Reset();
}

void APdCharacterBase::OnDeadTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	static_cast<void>(CallbackTag);

	if (NewCount <= 0)
	{
		bDeathHandled = false;
		return;
	}

	if (bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;
	HandleDeath();
	if (HasAuthority())
	{
		MulticastHandleDeath();
	}
}
void APdCharacterBase::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	if (!HasAuthority() || FMath::IsNearlyEqual(Data.NewValue, Data.OldValue))
	{
		return;
	}

	if (Data.NewValue < Data.OldValue)
	{
		RemoveStaminaRegenEffects();
		if (GetWorld() && StaminaRegenEffectClass)
		{
			GetWorldTimerManager().SetTimer(
				StaminaRegenDelayTimerHandle,
				this,
				&ThisClass::ApplyStaminaRegenEffect,
				StaminaRegenDelay,
				false);
		}
		return;
	}

	const float MaxStamina = GetCurrentMaxStamina();
	if (MaxStamina > 0.0f && Data.NewValue >= MaxStamina - KINDA_SMALL_NUMBER)
	{
		if (GetWorld())
		{
			GetWorldTimerManager().ClearTimer(StaminaRegenDelayTimerHandle);
		}
		RemoveStaminaRegenEffects();
	}
}

void APdCharacterBase::ApplyStaminaRegenEffect()
{
	if (!HasAuthority() || !StaminaRegenEffectClass)
	{
		return;
	}

	UAbilitySystemComponent* ASC = StaminaRegenASC.Get();
	if (!ASC)
	{
		return;
	}

	const float MaxStamina = GetCurrentMaxStamina();
	const float CurrentStamina = ASC->GetNumericAttribute(UBasicAttributeSet::GetStaminaAttribute());
	if (MaxStamina > 0.0f && CurrentStamina >= MaxStamina - KINDA_SMALL_NUMBER)
	{
		RemoveStaminaRegenEffects();
		return;
	}

	RemoveStaminaRegenEffects();

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(StaminaRegenEffectClass, FMath::Max(StaminaRegenEffectLevel, 1.0f), EffectContext);
	if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void APdCharacterBase::RemoveStaminaRegenEffects()
{
	UAbilitySystemComponent* ASC = StaminaRegenASC.Get();
	if (!ASC)
	{
		return;
	}

	FGameplayTagContainer RegenTags;
	RegenTags.AddTag(LabGameplayTags::Status_StaminaRegen);
	ASC->RemoveActiveEffectsWithGrantedTags(RegenTags);
}

float APdCharacterBase::GetCurrentMaxStamina() const
{
	const UAbilitySystemComponent* ASC = StaminaRegenASC.Get();
	return ASC ? ASC->GetNumericAttribute(UBasicAttributeSet::GetMaxStaminaAttribute()) : 0.0f;
}

/** ?�로?�트 ?�용 ASC�?반환?�니?? */
UPdAbilitySystemComponent* APdCharacterBase::GetPdAbilitySystemComponent() const
{
	return Cast<UPdAbilitySystemComponent>(GetAbilitySystemComponent());
}

void APdCharacterBase::ServerSendGameplayEventToSelf_Implementation(FGameplayEventData EventData)
{
	if (!EventData.EventTag.IsValid())
	{
		return;
	}

	if (!EventData.Instigator)
	{
		EventData.Instigator = this;
	}

	if (!EventData.Target)
	{
		EventData.Target = this;
	}

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, EventData.EventTag, EventData);
}

void APdCharacterBase::MulticastSendGameplayEventToActor_Implementation(AActor* TargetActor, FGameplayEventData EventData)
{
	if (!IsValid(TargetActor) || !EventData.EventTag.IsValid())
	{
		return;
	}

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, EventData.EventTag, EventData);
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

/** ASC OwnerActor�?반환?�니?? */
AActor* APdCharacterBase::GetAbilitySystemOwnerActor() const
{
	return const_cast<APdCharacterBase*>(this);
}

/** ASC AvatarActor�?반환?�니?? */
AActor* APdCharacterBase::GetAbilitySystemAvatarActor() const
{
	return const_cast<APdCharacterBase*>(this);
}

/** ?�버?�서 ?�망 처리�??�행?�니?? */
void APdCharacterBase::HandleDeathAuth()
{
	UE_LOG(PdCharacterBaseLog, Log, TEXT("%s died"), *GetName());
	Destroy();
}

void APdCharacterBase::HandleDeath_Implementation()
{
	UE_LOG(PdCharacterBaseLog, Log, TEXT("HandleDeath: character=%s authority=%s"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("true") : TEXT("false"));

	if (HealthBarWidget)
	{
		HealthBarWidget->SetVisibility(false, true);
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MeshComponent->SetAllBodiesSimulatePhysics(true);
		MeshComponent->SetSimulatePhysics(true);
		MeshComponent->WakeAllRigidBodies();
		MeshComponent->bBlendPhysics = true;

		const FVector DeathImpulse = (-GetActorForwardVector() * DeathImpulseHorizontalStrength)
			+ (GetActorRightVector() * DeathImpulseSideStrength)
			+ (FVector::UpVector * DeathImpulseUpwardStrength);
		const FVector DeathImpulseLocation = GetActorLocation() + FVector(0.0f, 0.0f, DeathImpulseLocationZOffset);
		MeshComponent->AddImpulseAtLocation(DeathImpulse, DeathImpulseLocation);
	}
}


/** 진영 ID�?반환?�니?? */
void APdCharacterBase::MulticastHandleDeath_Implementation()
{
	if (HasAuthority() || bDeathHandled)
	{
		return;
	}

	bDeathHandled = true;
	UE_LOG(PdCharacterBaseLog, Log, TEXT("MulticastHandleDeath: character=%s"),
		*GetNameSafe(this));
	HandleDeath();
}

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
