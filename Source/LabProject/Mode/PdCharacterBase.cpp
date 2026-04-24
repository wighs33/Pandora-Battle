// Fill out your copyright notice in the Description page of Project Settings.

#include "PdCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Equipment/EquipmentComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "AbilitySystem/PdAbilitySystemComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Mode/PdPlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(PdCharacterBase)

DEFINE_LOG_CATEGORY(PdCharacterBaseLog);

APdCharacterBase::APdCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 700.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 450.0f;

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(GetRootComponent());
	InteractionBox->SetBoxExtent(FVector(50.0f, 50.0f, 100.0f));
	InteractionBox->SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));
	InteractionBox->SetRelativeRotation(FRotator::ZeroRotator);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	InteractionBox->SetGenerateOverlapEvents(true);
	InteractionBox->SetCanEverAffectNavigation(false);
	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &APdCharacterBase::HandleInteractionBoxBeginOverlap);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &APdCharacterBase::HandleInteractionBoxEndOverlap);

	EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void APdCharacterBase::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void APdCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);

	if (HasAuthority())
	{
		CurrentAnimLayer = DefaultAnimLayer;
	}

	ResetAnimationToDefault();
}

void APdCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	Super::EndPlay(EndPlayReason);
}

void APdCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APdCharacterBase, CurrentAnimLayer);
}

void APdCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystemActorInfo();
	GiveDefaultAbilities();
}

void APdCharacterBase::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystemActorInfo();
}

void APdCharacterBase::UnPossessed()
{
	Super::UnPossessed();

	if (APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>())
	{
		if (UPdAbilitySystemComponent* ASC = PdPlayerState->GetPdAbilitySystemComponent())
		{
			ASC->ClearActorInfo();
		}
	}
}

void APdCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

int32 APdCharacterBase::GiveDefaultAbilities()
{
	if (!HasAuthority())
	{
		UE_LOG(PdCharacterBaseLog, Warning, TEXT("GiveDefaultAbilities failed: '%s' can only grant abilities on the server."), *GetNameSafe(this));
		return 0;
	}

	UPdAbilitySystemComponent* ASC = GetPdAbilitySystemComponent();
	if (!ASC)
	{
		UE_LOG(PdCharacterBaseLog, Warning, TEXT("GiveDefaultAbilities failed: '%s' has no valid ability system component."), *GetNameSafe(this));
		return 0;
	}

	const int32 SafeAbilityLevel = 1;
	int32 GrantedCount = 0;

	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultAbilities)
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
		
		// 기본 어빌리티는 SourceObject 없이 클래스/레벨만 부여해도 충분합니다.
		FGameplayAbilitySpec AbilitySpec(AbilityClass, SafeAbilityLevel, INDEX_NONE, nullptr);
		ASC->GiveAbility(AbilitySpec);
		++GrantedCount;
	}

	return GrantedCount;
}

bool APdCharacterBase::HasCurrentInteractActors(TArray<TScriptInterface<IInteractableInterface>>& OutCurrentInteractActors) const
{
	OutCurrentInteractActors = CurrentInteractActors;
	return OutCurrentInteractActors.Num() > 0;
}

bool APdCharacterBase::TryMakeInteractableEntry(AActor* OtherActor, TScriptInterface<IInteractableInterface>& OutInteractableActor) const
{
	if (!IsValid(OtherActor) || OtherActor == this || !OtherActor->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return false;
	}

	OutInteractableActor.SetObject(OtherActor);
	OutInteractableActor.SetInterface(Cast<IInteractableInterface>(OtherActor));
	return true;
}

void APdCharacterBase::HandleInteractionBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);
	static_cast<void>(bFromSweep);
	static_cast<void>(SweepResult);

	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		return;
	}

	const bool bAlreadyTracked = CurrentInteractActors.ContainsByPredicate(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});

	if (bAlreadyTracked)
	{
		return;
	}

	CurrentInteractActors.Add(InteractableActor);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UKismetSystemLibrary::PrintString(this, TEXT("Interact (X)"), true, true, FLinearColor(0.0f, 0.66f, 1.0f), 2.0f);
#endif
}

void APdCharacterBase::HandleInteractionBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	static_cast<void>(OverlappedComponent);
	static_cast<void>(OtherComp);
	static_cast<void>(OtherBodyIndex);

	TScriptInterface<IInteractableInterface> InteractableActor;
	if (!TryMakeInteractableEntry(OtherActor, InteractableActor))
	{
		return;
	}

	CurrentInteractActors.RemoveAll(
		[OtherActor](const TScriptInterface<IInteractableInterface>& Entry)
		{
			return Entry.GetObject() == OtherActor;
		});
}

void APdCharacterBase::InitializeAbilitySystemActorInfo()
{
	APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>();
	if (!PdPlayerState)
	{
		return;
	}

	UPdAbilitySystemComponent* ASC = PdPlayerState->GetPdAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	ASC->InitAbilityActorInfo(PdPlayerState, this);
}


void APdCharacterBase::ResetAnimationToDefault()
{
	SetCurrentAnimLayer(DefaultAnimLayer);
}

void APdCharacterBase::SetCurrentAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass)
{
	const TSubclassOf<UAnimInstance> NewAnimLayer = AnimLayerClass ? AnimLayerClass : DefaultAnimLayer;
	CurrentAnimLayer = NewAnimLayer;
	LinkAnimLayer(NewAnimLayer);
}

void APdCharacterBase::OnRep_CurrentAnimLayer()
{
	LinkAnimLayer(CurrentAnimLayer ? CurrentAnimLayer : DefaultAnimLayer);
}

void APdCharacterBase::LinkAnimLayer(TSubclassOf<UAnimInstance> AnimLayerClass) const
{
	// 애니메이션은 클라이언트에서만 필요합니다
	if (HasAuthority() || !AnimLayerClass)
	{
		if (GetNetMode() == NM_DedicatedServer)
		{
			return;
		}
	}
	
	if (GetMesh() && AnimLayerClass)
	{
		GetMesh()->LinkAnimClassLayers(AnimLayerClass);
	}
}

UAbilitySystemComponent* APdCharacterBase::GetAbilitySystemComponent() const
{
	return GetPdAbilitySystemComponent();
}

UPdAbilitySystemComponent* APdCharacterBase::GetPdAbilitySystemComponent() const
{
	if (const APdPlayerState* PdPlayerState = GetPlayerState<APdPlayerState>())
	{
		return PdPlayerState->GetPdAbilitySystemComponent();
	}

	return nullptr;
}

void APdCharacterBase::HandleDeathAuth()
{
	UE_LOG(PdCharacterBaseLog, Log, TEXT("%s 사망"), *GetName());

	Destroy();
}

int32 APdCharacterBase::GetFactionId() const
{
	return FactionId;
}
